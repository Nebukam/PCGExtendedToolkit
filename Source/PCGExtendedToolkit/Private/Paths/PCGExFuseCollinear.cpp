// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExFuseCollinear.h"

#include "PCGExMath.h"
#include "PCGExMT.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExUnionBlender.h"
#include "Details/PCGExDetailsDistances.h"
#include "Graph/PCGExGraph.h"
#include "Paths/PCGExPaths.h"
#include "Paths/PCGExPathShift.h"


#define LOCTEXT_NAMESPACE "PCGExFuseCollinearElement"
#define PCGEX_NAMESPACE FuseCollinear

PCGEX_INITIALIZE_ELEMENT(FuseCollinear)
PCGEX_ELEMENT_BATCH_POINT_IMPL(FuseCollinear)

bool FPCGExFuseCollinearElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(FuseCollinear)

	Context->DotThreshold = PCGExMath::DegreesToDot(Settings->Threshold);
	Context->FuseDistSquared = FMath::Square(Settings->FuseDistance);

	return true;
}


bool FPCGExFuseCollinearElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFuseCollinearElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FuseCollinear)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		// TODO : Skip completion

		if (!Context->StartBatchProcessingPoints([&](const TSharedPtr<PCGExData::FPointIO>& Entry)
		                                         {
			                                         if (Entry->GetNum() < 2)
			                                         {
				                                         bHasInvalidInputs = true;
				                                         if (!Settings->bOmitInvalidPathsFromOutput) { Entry->InitializeOutput(PCGExData::EIOInit::Forward); }
				                                         return false;
			                                         }
			                                         return true;
		                                         }, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
		                                         {
		                                         }))
		{
			Context->CancelExecution(TEXT("Could not find any paths to fuse."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	return Context->TryComplete();
}

namespace PCGExFuseCollinear
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExFuseCollinear::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }

		Path = MakeShared<PCGExPaths::FPath>(PointDataFacade->GetIn(), 0);


		TArray<int32> ReadIndices;
		ReadIndices.Reserve(Path->NumPoints);
		LastPosition = Path->GetPos(0);

		bForceSingleThreadedProcessPoints = true;
		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		//PointDataFacade->Fetch(Scope);
		FilterAll();

		// Preserve start & end
		PointFilterCache[0] = true;

		// Only force-preserve last point if not closed loop
		if (!Path->IsClosedLoop()) { PointFilterCache[Path->LastIndex] = true; }

		// Process path...

		for (int i = 0; i < Path->NumPoints; i++)
		{
			if (PointFilterCache[i])
			{
				// Kept point, as per filters
				ReadIndices.Add(i);
				LastPosition = Path->GetPos(i);
				continue;
			}

			const FVector CurrentPos = Path->GetPos(i);
			if (Settings->bFuseCollocated && FVector::DistSquared(LastPosition, CurrentPos) <= Context->FuseDistSquared)
			{
				// Collocated points
				continue;
			}

			// Use last position to avoid removing smooth arcs
			const double Dot = FVector::DotProduct((CurrentPos - LastPosition).GetSafeNormal(), Path->DirToNextPoint(i));
			if ((!Settings->bInvertThreshold && Dot > Context->DotThreshold) || (Settings->bInvertThreshold && Dot < Context->DotThreshold))
			{
				// Collinear with previous, keep moving
				continue;
			}

			ReadIndices.Add(i);
			LastPosition = Path->GetPos(i);
		}

		if (ReadIndices.Num() < 2) { return false; }

		if (Path->IsClosedLoop())
		{
			// Make sure the first point isn't collinear with next valid/last valid
			const int32 NextIndex = ReadIndices[1];
			const int32 LastIndex = ReadIndices.Last();

			const FVector FirstPos = Path->GetPos(0);
			const FVector ForwardDir = (Path->GetPos(NextIndex) - FirstPos).GetSafeNormal();
			const FVector WrapDir = (FirstPos - Path->GetPos(LastIndex)).GetSafeNormal();

			// Use last position to avoid removing smooth arcs
			const double Dot = FVector::DotProduct(WrapDir, ForwardDir);
			if ((!Settings->bInvertThreshold && Dot > Context->DotThreshold) || (Settings->bInvertThreshold && Dot < Context->DotThreshold))
			{
				// Collinear with previous, keep moving
				ReadIndices.RemoveAt(0);
			}
		}

		if (ReadIndices.Num() < 2) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::New)

		PCGEx::SetNumPointsAllocated(PointDataFacade->GetOut(), ReadIndices.Num(), PointDataFacade->GetAllocations());
		PointDataFacade->Source->InheritPoints(ReadIndices, 0);

		if (Settings->bDoBlend) { Blend(ReadIndices); }

		return true;
	}

	void FProcessor::Blend(TArray<int32>& ReadIndices)
	{
		const PCGExDetails::FDistances* NoneDistances = PCGExDetails::GetNoneDistances();
		const TSharedPtr<PCGExDataBlending::FUnionBlender> Blender = MakeShared<PCGExDataBlending::FUnionBlender>(
			const_cast<FPCGExBlendingDetails*>(&Settings->BlendingDetails), nullptr, NoneDistances);

		TArray<TSharedRef<PCGExData::FFacade>> UnionSources;
		UnionSources.Add(PointDataFacade);

		Blender->AddSources(UnionSources, nullptr);
		if (!Blender->Init(Context, PointDataFacade))
		{
			bIsProcessorValid = false;
			return;
		}
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

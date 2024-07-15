// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExSubdivide.h"

#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExSubdivideElement"
#define PCGEX_NAMESPACE Subdivide

PCGExData::EInit UPCGExSubdivideSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

PCGEX_INITIALIZE_ELEMENT(Subdivide)

FPCGExSubdivideContext::~FPCGExSubdivideContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExSubdivideElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Subdivide)

	if (Settings->bFlagSubPoints) { PCGEX_VALIDATE_NAME(Settings->FlagName) }

	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInterpolate)
	Context->Blending->bClosedPath = Settings->bClosedPath;

	return true;
}


bool FPCGExSubdivideElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSubdivideElement::Execute);

	PCGEX_CONTEXT(Subdivide)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSubdivide::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bInvalidInputs = true;
					Entry->InitializeOutput(PCGExData::EInit::Forward);
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExSubdivide::FProcessor>* NewBatch)
			{
				NewBatch->PrimaryOperation = Context->Blending;
				NewBatch->bRequiresWriteStep = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any paths to subdivide."));
			return true;
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->OutputMainPoints();
	
	return Context->TryComplete();
}

namespace PCGExSubdivide
{
	FProcessor::~FProcessor()
	{
		Milestones.Empty();
		MilestonesMetrics.Empty();
		FlagAttribute = nullptr;
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSubdivide::Process);
		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(Subdivide)

		PointIO->InitializeOutput(PCGExData::EInit::NewOutput);

		if (Settings->bFlagSubPoints)
		{
			FlagAttribute = PointIO->GetOut()->Metadata->FindOrCreateAttribute(Settings->FlagName, false, false);
		}

		Blending = Cast<UPCGExSubPointsBlendOperation>(PrimaryOperation);

		for (int i = 0; i < PointIO->GetNum(); i++) { ProcessPathPoint(i); }

		return true;
	}

	void FProcessor::ProcessPathPoint(const int32 Index)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(Subdivide)

		int32 LastIndex;

		const FPCGPoint& StartPoint = PointIO->GetInPoint(Index);
		const FPCGPoint* EndPtr = PointIO->TryGetInPoint(Index + 1);
		PointIO->CopyPoint(StartPoint, LastIndex);

		Milestones.Add(LastIndex);
		PCGExMath::FPathMetricsSquared& Metrics = MilestonesMetrics.Emplace_GetRef();

		if (!EndPtr) { return; }

		const FVector StartPos = StartPoint.Transform.GetLocation();
		const FVector EndPos = EndPtr->Transform.GetLocation();
		const FVector Dir = (EndPos - StartPos).GetSafeNormal();

		const double Distance = FVector::Distance(StartPos, EndPos);
		const int32 NumSubdivisions = Settings->SubdivideMethod == EPCGExSubdivideMode::Count ?
			                              Settings->Count :
			                              FMath::Floor(FVector::Distance(StartPos, EndPos) / Settings->Distance);

		const double StepSize = Distance / static_cast<double>(NumSubdivisions);
		const double StartOffset = (Distance - StepSize * NumSubdivisions) * 0.5;

		Metrics.Reset(StartPos);

		for (int i = 0; i < NumSubdivisions; i++)
		{
			FPCGPoint& NewPoint = PointIO->CopyPoint(StartPoint, LastIndex);
			FVector SubLocation = StartPos + Dir * (StartOffset + i * StepSize);
			NewPoint.Transform.SetLocation(SubLocation);
			Metrics.Add(SubLocation);

			if (FlagAttribute) { FlagAttribute->SetValue(NewPoint.MetadataEntry, true); }
		}

		Metrics.Add(EndPos);
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		if (!Milestones.IsValidIndex(Iteration + 1)) { return; } // Ignore last point

		const int32 StartIndex = Milestones[Iteration];
		const int32 EndIndex = Milestones[Iteration + 1];
		const int32 Range = EndIndex - StartIndex;

		if (Range <= 0) { return; } // No sub points

		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
		TArrayView<FPCGPoint> View = MakeArrayView(MutablePoints.GetData() + StartIndex, Range);

		Blending->ProcessSubPoints(
			PointIO->GetOutPointRef(StartIndex), PointIO->GetOutPointRef(EndIndex),
			View, MilestonesMetrics[Iteration]);

		for (FPCGPoint& Pt : View) { PCGExMath::RandomizeSeed(Pt); }
	}

	void FProcessor::CompleteWork()
	{
		Blending->PrepareForData(PointDataFacade, PointDataFacade, PCGExData::ESource::Out);
		StartParallelLoopForRange(Milestones.Num());
	}

	void FProcessor::Write()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

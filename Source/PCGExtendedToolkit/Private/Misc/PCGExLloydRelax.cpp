// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExLloydRelax.h"

#include "Geometry/PCGExGeoDelaunay.h"

#define LOCTEXT_NAMESPACE "PCGExLloydRelaxElement"
#define PCGEX_NAMESPACE LloydRelax

PCGExData::EInit UPCGExLloydRelaxSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(LloydRelax)

FPCGExLloydRelaxContext::~FPCGExLloydRelaxContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExLloydRelaxElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(LloydRelax)

	return true;
}

bool FPCGExLloydRelaxElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExLloydRelaxElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(LloydRelax)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExLloydRelax::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() <= 4)
				{
					Entry->InitializeOutput(PCGExData::EInit::Forward);
					bInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExLloydRelax::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any paths to relax."));
			return true;
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 4 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
	}

	return Context->TryComplete();
}

namespace PCGExLloydRelax
{
	FProcessor::~FProcessor()
	{
		ActivePositions.Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(LloydRelax)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		InfluenceDetails = Settings->InfluenceDetails;
		if (!InfluenceDetails.Init(Context, PointDataFacade)) { return false; }

		PointIO->InitializeOutput(PCGExData::EInit::DuplicateInput);
		PCGExGeo::PointsToPositions(PointIO->GetIn()->GetPoints(), ActivePositions);

		AsyncManagerPtr->Start<FLloydRelaxTask>(0, PointIO, this, &InfluenceDetails, Settings->Iterations);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		Point.Transform.SetLocation(
			InfluenceDetails.bProgressiveInfluence ?
				ActivePositions[Index] :
				FMath::Lerp(Point.Transform.GetLocation(), ActivePositions[Index], InfluenceDetails.GetInfluence(Index)));
	}

	void FProcessor::CompleteWork()
	{
		StartParallelLoopForPoints();
	}

	bool FLloydRelaxTask::ExecuteTask()
	{
		NumIterations--;

		PCGExGeo::TDelaunay3* Delaunay = new PCGExGeo::TDelaunay3();
		TArray<FVector>& Positions = Processor->ActivePositions;

		//FPCGExPointsProcessorContext* Context = static_cast<FPCGExPointsProcessorContext*>(Manager->Context);

		const TArrayView<FVector> View = MakeArrayView(Positions);
		if (!Delaunay->Process(View, false)) { return false; }

		const int32 NumPoints = Positions.Num();

		TArray<FVector> Sum;
		TArray<double> Counts;
		Sum.Append(Processor->ActivePositions);
		Counts.SetNum(NumPoints);
		for (int i = 0; i < NumPoints; i++) { Counts[i] = 1; }

		FVector Centroid;
		for (const PCGExGeo::FDelaunaySite3& Site : Delaunay->Sites)
		{
			PCGExGeo::GetCentroid(Positions, Site.Vtx, Centroid);
			for (const int32 PtIndex : Site.Vtx)
			{
				Counts[PtIndex] += 1;
				Sum[PtIndex] += Centroid;
			}
		}

		if (InfluenceSettings->bProgressiveInfluence)
		{
			for (int i = 0; i < NumPoints; i++) { Positions[i] = FMath::Lerp(Positions[i], Sum[i] / Counts[i], InfluenceSettings->GetInfluence(i)); }
		}

		PCGEX_DELETE(Delaunay)

		if (NumIterations > 0)
		{
			InternalStart<FLloydRelaxTask>(TaskIndex + 1, PointIO, Processor, InfluenceSettings, NumIterations);
		}

		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

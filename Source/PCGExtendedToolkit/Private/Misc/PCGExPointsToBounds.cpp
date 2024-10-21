// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExPointsToBounds.h"

#include "Data/PCGExData.h"


#define LOCTEXT_NAMESPACE "PCGExPointsToBoundsElement"
#define PCGEX_NAMESPACE PointsToBounds

PCGExData::EInit UPCGExPointsToBoundsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

PCGEX_INITIALIZE_ELEMENT(PointsToBounds)

bool FPCGExPointsToBoundsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PointsToBounds)

	if (Settings->bWritePointsCount) { PCGEX_VALIDATE_NAME(Settings->PointsCountAttributeName) }

	return true;
}

bool FPCGExPointsToBoundsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPointsToBoundsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PointsToBounds)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPointsToBounds::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExPointsToBounds::FProcessor>>& NewBatch)
			{
				//NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExPointsToBounds
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPointsToBounds::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		Bounds = MakeShared<FBounds>(PointDataFacade->Source);
		const TArray<FPCGPoint>& InPoints = PointDataFacade->GetIn()->GetPoints();

		switch (Settings->BoundsSource)
		{
		default: ;
		case EPCGExPointBoundsSource::DensityBounds:
			for (const FPCGPoint& Pt : InPoints)
			{
				const FBox Box = Pt.GetDensityBounds().GetBox();
				Bounds->Bounds += Box;
				Bounds->FastVolume += Box.GetExtent().Length();
			}
			break;
		case EPCGExPointBoundsSource::ScaledBounds:
			for (const FPCGPoint& Pt : InPoints)
			{
				const FBox Box = FBoxCenterAndExtent(Pt.Transform.GetLocation(), Pt.GetScaledExtents()).GetBox();
				Bounds->Bounds += Box;
				Bounds->FastVolume += Box.GetExtent().Length();
			}
			break;
		case EPCGExPointBoundsSource::Bounds:
			for (const FPCGPoint& Pt : InPoints)
			{
				const FBox Box = FBoxCenterAndExtent(Pt.Transform.GetLocation(), Pt.GetExtents()).GetBox();
				Bounds->Bounds += Box;
				Bounds->FastVolume += Box.GetExtent().Length();
			}
			break;
		}

		return true;
	}

	void FProcessor::CompleteWork()
	{
		const TArray<FPCGPoint>& InPoints = PointDataFacade->GetIn()->GetPoints();
		UPCGPointData* OutData = PointDataFacade->GetOut();

		TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
		MutablePoints.Emplace();

		const double NumPoints = InPoints.Num();

		const FBox& Box = Bounds->Bounds;
		const FVector Center = Box.GetCenter();
		const double SqrDist = Box.GetExtent().SquaredLength();

		MutablePoints[0].Transform.SetLocation(Center);
		MutablePoints[0].BoundsMin = Box.Min - Center;
		MutablePoints[0].BoundsMax = Box.Max - Center;

		if (Settings->bBlendProperties)
		{
			MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&Settings->BlendingSettings);
			MetadataBlender->PrepareForData(PointDataFacade);

			const PCGExData::FPointRef Target = PointDataFacade->Source->GetOutPointRef(0);
			MetadataBlender->PrepareForBlending(Target);

			double TotalWeight = 0;

			for (int i = 0; i < NumPoints; i++)
			{
				FVector Location = InPoints[i].Transform.GetLocation();
				const double Weight = FVector::DistSquared(Center, Location) / SqrDist;
				MetadataBlender->Blend(Target, PointDataFacade->Source->GetInPointRef(i), Target, Weight);
				TotalWeight += Weight;
			}

			MetadataBlender->CompleteBlending(Target, NumPoints, TotalWeight);

			MutablePoints[0].Transform.SetLocation(Center);
			MutablePoints[0].BoundsMin = Box.Min - Center;
			MutablePoints[0].BoundsMax = Box.Max - Center;
		}

		if (Settings->bWritePointsCount) { WriteMark(PointDataFacade->Source, Settings->PointsCountAttributeName, NumPoints); }

		PointDataFacade->Write(AsyncManager);
	}

	bool FComputeIOBoundsTask::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		const TArray<FPCGPoint>& InPoints = Bounds->PointIO->GetIn()->GetPoints();

		switch (BoundsSource)
		{
		default: ;
		case EPCGExPointBoundsSource::DensityBounds:
			for (const FPCGPoint& Pt : InPoints)
			{
				const FBox Box = Pt.GetDensityBounds().GetBox();
				Bounds->Bounds += Box;
				Bounds->FastVolume += Box.GetExtent().Length();
			}
			break;
		case EPCGExPointBoundsSource::ScaledBounds:
			for (const FPCGPoint& Pt : InPoints)
			{
				const FBox Box = FBoxCenterAndExtent(Pt.Transform.GetLocation(), Pt.GetScaledExtents()).GetBox();
				Bounds->Bounds += Box;
				Bounds->FastVolume += Box.GetExtent().Length();
			}
			break;
		case EPCGExPointBoundsSource::Bounds:
			for (const FPCGPoint& Pt : InPoints)
			{
				const FBox Box = FBoxCenterAndExtent(Pt.Transform.GetLocation(), Pt.GetExtents()).GetBox();
				Bounds->Bounds += Box;
				Bounds->FastVolume += Box.GetExtent().Length();
			}
			break;
		}

		return true;
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExPointsToBounds.h"

#include "MinVolumeBox3.h"
#include "OrientedBoxTypes.h"
#include "Data/PCGExData.h"


#define LOCTEXT_NAMESPACE "PCGExPointsToBoundsElement"
#define PCGEX_NAMESPACE PointsToBounds

PCGExData::EIOInit UPCGExPointsToBoundsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::New; }

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

		Bounds = FBox(ForceInit);
		const TArray<FPCGPoint>& InPoints = PointDataFacade->GetIn()->GetPoints();

		if (Settings->bOutputOrientedBoundingBox)
		{
			PCGEX_ASYNC_GROUP_CHKD(AsyncManager, MinBoxTask)
			MinBoxTask->AddSimpleCallback(
				[PCGEX_ASYNC_THIS_CAPTURE]()
				{
					PCGEX_ASYNC_THIS

					const TArray<FPCGPoint>& InPoints = This->PointDataFacade->GetIn()->GetPoints();
					UE::Geometry::TMinVolumeBox3<double> Box;
					if (Box.Solve(This->PointDataFacade->GetNum(), [&InPoints](int32 i) { return InPoints[i].Transform.GetLocation(); }))
					{
						Box.GetResult(This->OrientedBox);
						This->bOrientedBoxFound = true;
					}
				});

			MinBoxTask->StartSimpleCallbacks();
		}

		switch (Settings->BoundsSource)
		{
		default: ;
		case EPCGExPointBoundsSource::DensityBounds:
			for (const FPCGPoint& Pt : InPoints) { Bounds += Pt.GetDensityBounds().GetBox(); }
			break;
		case EPCGExPointBoundsSource::ScaledBounds:
			for (const FPCGPoint& Pt : InPoints) { Bounds += FBoxCenterAndExtent(Pt.Transform.GetLocation(), Pt.GetScaledExtents()).GetBox(); }
			break;
		case EPCGExPointBoundsSource::Bounds:
			for (const FPCGPoint& Pt : InPoints) { Bounds += FBoxCenterAndExtent(Pt.Transform.GetLocation(), Pt.GetExtents()).GetBox(); }
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

		if (Settings->bBlendProperties)
		{
			MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&Settings->BlendingSettings);
			MetadataBlender->PrepareForData(PointDataFacade);

			const PCGExData::FPointRef Target = PointDataFacade->Source->GetOutPointRef(0);
			MetadataBlender->PrepareForBlending(Target);

			double TotalWeight = 0;

			for (int i = 0; i < NumPoints; i++)
			{
				//FVector Location = InPoints[i].Transform.GetLocation();
				constexpr double Weight = 1; // FVector::DistSquared(Center, Location) / SqrDist;
				MetadataBlender->Blend(Target, PointDataFacade->Source->GetInPointRef(i), Target, Weight);
				TotalWeight += Weight;
			}

			MetadataBlender->CompleteBlending(Target, NumPoints, TotalWeight);
		}

		if (bOrientedBoxFound)
		{
			const FVector MinExtents = OrientedBox.Extents;
			MutablePoints[0].Transform.SetLocation(OrientedBox.Center());
			MutablePoints[0].Transform.SetRotation(FQuat(OrientedBox.Frame.Rotation));
			MutablePoints[0].BoundsMin = -MinExtents;
			MutablePoints[0].BoundsMax = MinExtents;
		}
		else
		{
			const FVector Center = Bounds.GetCenter();
			MutablePoints[0].Transform.SetLocation(Center);
			MutablePoints[0].BoundsMin = Bounds.Min - Center;
			MutablePoints[0].BoundsMax = Bounds.Max - Center;
		}

		if (Settings->bWritePointsCount) { WriteMark(PointDataFacade->Source, Settings->PointsCountAttributeName, NumPoints); }

		PointDataFacade->Write(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

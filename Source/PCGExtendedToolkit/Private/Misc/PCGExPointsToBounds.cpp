// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExPointsToBounds.h"

#include "MinVolumeBox3.h"
#include "OrientedBoxTypes.h"
#include "Data/PCGExData.h"


#define LOCTEXT_NAMESPACE "PCGExPointsToBoundsElement"
#define PCGEX_NAMESPACE PointsToBounds

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

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPointsToBounds::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::New)

		Bounds = FBox(ForceInit);

		if (Settings->bOutputOrientedBoundingBox)
		{
			PCGEX_ASYNC_GROUP_CHKD(AsyncManager, MinBoxTask)
			MinBoxTask->AddSimpleCallback(
				[PCGEX_ASYNC_THIS_CAPTURE]()
				{
					PCGEX_ASYNC_THIS

					TConstPCGValueRange<FTransform> InTransforms = This->PointDataFacade->GetIn()->GetConstTransformValueRange();
					UE::Geometry::TMinVolumeBox3<double> Box;
					if (Box.Solve(This->PointDataFacade->GetNum(), [InTransforms](int32 i) { return InTransforms[i].GetLocation(); }))
					{
						Box.GetResult(This->OrientedBox);
						This->bOrientedBoxFound = true;
					}
				});

			MinBoxTask->StartSimpleCallbacks();
		}

		const UPCGBasePointData* InPointData = PointDataFacade->GetIn();
		const int32 NumPoints = InPointData->GetNumPoints();

		TConstPCGValueRange<FTransform> InTransforms = InPointData->GetConstTransformValueRange();

		switch (Settings->BoundsSource)
		{
		default: ;
		case EPCGExPointBoundsSource::DensityBounds:
			for (int i = 0; i < NumPoints; i++) { Bounds += InPointData->GetDensityBounds(i).GetBox(); }
			break;
		case EPCGExPointBoundsSource::ScaledBounds:
			for (int i = 0; i < NumPoints; i++) { Bounds += FBoxCenterAndExtent(InTransforms[i].GetLocation(), InPointData->GetScaledExtents(i)).GetBox(); }
			break;
		case EPCGExPointBoundsSource::Bounds:
			for (int i = 0; i < NumPoints; i++) { Bounds += FBoxCenterAndExtent(InTransforms[i].GetLocation(), InPointData->GetExtents(i)).GetBox(); }
			break;
		case EPCGExPointBoundsSource::Center:
			for (int i = 0; i < NumPoints; i++) { Bounds += InTransforms[i].GetLocation(); }
			break;
		}

		return true;
	}

	void FProcessor::CompleteWork()
	{
		const UPCGBasePointData* InData = PointDataFacade->GetIn();
		UPCGBasePointData* OutData = PointDataFacade->GetOut();
		OutData->SetNumPoints(1);

		const double NumPoints = InData->GetNumPoints();

		if (Settings->bBlendProperties)
		{
			MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>(&Settings->BlendingSettings);
			MetadataBlender->PrepareForData(PointDataFacade);

			const PCGExData::FConstPoint Target = PointDataFacade->GetOutPoint(0);
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

		TPCGValueRange<FTransform> OutTransforms = OutData->GetTransformValueRange();
		TPCGValueRange<FVector> OutBoundsMin = OutData->GetBoundsMinValueRange();
		TPCGValueRange<FVector> OutBoundsMax = OutData->GetBoundsMaxValueRange();

		if (bOrientedBoxFound)
		{
			const FVector Extents = OrientedBox.Extents;
			OutTransforms[0].SetLocation(OrientedBox.Center());
			OutTransforms[0].SetRotation(FQuat(OrientedBox.Frame.Rotation));
			OutBoundsMin[0] = -Extents;
			OutBoundsMax[0] = Extents;
		}
		else
		{
			const FVector Center = Bounds.GetCenter();
			OutTransforms[0].SetLocation(Center);
			OutBoundsMin[0] = Bounds.Min - Center;
			OutBoundsMax[0] = Bounds.Max - Center;
		}

		if (Settings->bWritePointsCount) { WriteMark(PointDataFacade->Source, Settings->PointsCountAttributeName, NumPoints); }

		PointDataFacade->Write(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

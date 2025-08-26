﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExPointsToBounds.h"

#include "PCGExMath.h"
#include "Data/PCGExData.h"
#include "Geometry/PCGExGeo.h"
#include "Graph/PCGExGraph.h"


#define LOCTEXT_NAMESPACE "PCGExPointsToBoundsElement"
#define PCGEX_NAMESPACE PointsToBounds

void FPCGExPointsToBoundsDataDetails::Output(const UPCGBasePointData* InBoundsData, UPCGBasePointData* OutData, const TArray<FPCGAttributeIdentifier>& AttributeIdentifiers, PCGExGeo::FBestFitPlane& Plane) const
{
	if (!AttributeIdentifiers.IsEmpty())
	{
		for (const FPCGAttributeIdentifier& AttributeIdentifier : AttributeIdentifiers)
		{
			// Only carry over non-data attributes
			if (AttributeIdentifier.MetadataDomain.Flag != EPCGMetadataDomainFlag::Elements) { continue; }

			const FPCGMetadataAttributeBase* Source = InBoundsData->Metadata->GetConstAttribute(AttributeIdentifier);

			PCGEx::ExecuteWithRightType(
				Source->GetTypeId(), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					const FPCGMetadataAttribute<T>* TypedSource = static_cast<const FPCGMetadataAttribute<T>*>(Source);

					FPCGAttributeIdentifier DataIdentifier = FPCGAttributeIdentifier(AttributeIdentifier.Name, PCGMetadataDomainID::Data);
					const T Value = TypedSource->GetValueFromItemKey(PCGFirstEntryKey);
					FPCGMetadataAttribute<T>* Target = OutData->Metadata->FindOrCreateAttribute(DataIdentifier, Value);
					Target->SetDefaultValue(Value);
				});
		}
	}

#define PCGEX_WRITE_REDUCED_PROPERTY(_NAME)	if (bWrite##_NAME){ PCGExData::WriteMark(OutData, PCGEx::GetAttributeIdentifier(_NAME##AttributeName), InBoundsData->GetConst##_NAME##ValueRange()[0]); }

	PCGEX_WRITE_REDUCED_PROPERTY(Transform)
	PCGEX_WRITE_REDUCED_PROPERTY(Density)
	PCGEX_WRITE_REDUCED_PROPERTY(BoundsMin)
	PCGEX_WRITE_REDUCED_PROPERTY(BoundsMax)
	PCGEX_WRITE_REDUCED_PROPERTY(Color)
	PCGEX_WRITE_REDUCED_PROPERTY(Steepness)

#undef PCGEX_WRITE_REDUCED_PROPERTY

	if (bWriteBestFitPlane)
	{
		PCGExData::WriteMark(OutData, PCGEx::GetAttributeIdentifier(BestFitPlaneAttributeName), Plane.GetTransform(AxisOrder));
	}
}

void FPCGExPointsToBoundsDataDetails::OutputInverse(const UPCGBasePointData* InPoints, UPCGBasePointData* OutData, const TArray<FPCGAttributeIdentifier>& AttributeIdentifiers, PCGExGeo::FBestFitPlane& Plane) const
{
	if (!AttributeIdentifiers.IsEmpty())
	{
		for (const FPCGAttributeIdentifier& AttributeIdentifier : AttributeIdentifiers)
		{
			// Only carry over non-data attributes
			if (AttributeIdentifier.MetadataDomain.Flag != EPCGMetadataDomainFlag::Elements) { continue; }

			const FPCGMetadataAttributeBase* Source = OutData->Metadata->GetConstAttribute(AttributeIdentifier);

			PCGEx::ExecuteWithRightType(
				Source->GetTypeId(), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					const FPCGMetadataAttribute<T>* TypedSource = static_cast<const FPCGMetadataAttribute<T>*>(Source);

					FPCGAttributeIdentifier DataIdentifier = FPCGAttributeIdentifier(AttributeIdentifier.Name, PCGMetadataDomainID::Data);
					const T Value = TypedSource->GetValueFromItemKey(PCGFirstEntryKey);
					FPCGMetadataAttribute<T>* Target = OutData->Metadata->FindOrCreateAttribute(DataIdentifier, Value);
					Target->SetDefaultValue(Value);
				});
		}
	}

#define PCGEX_WRITE_REDUCED_PROPERTY(_NAME)	if (bWrite##_NAME){ PCGExData::WriteMark(OutData, PCGEx::GetAttributeIdentifier(_NAME##AttributeName), OutData->GetConst##_NAME##ValueRange()[0]); }

	PCGEX_WRITE_REDUCED_PROPERTY(Transform)
	PCGEX_WRITE_REDUCED_PROPERTY(Density)
	PCGEX_WRITE_REDUCED_PROPERTY(BoundsMin)
	PCGEX_WRITE_REDUCED_PROPERTY(BoundsMax)
	PCGEX_WRITE_REDUCED_PROPERTY(Color)
	PCGEX_WRITE_REDUCED_PROPERTY(Steepness)

#undef PCGEX_WRITE_REDUCED_PROPERTY

	if (bWriteBestFitPlane)
	{
		PCGExData::WriteMark(OutData, PCGEx::GetAttributeIdentifier(BestFitPlaneAttributeName), Plane.GetTransform(AxisOrder));
	}
}

PCGEX_INITIALIZE_ELEMENT(PointsToBounds)
PCGEX_ELEMENT_BATCH_POINT_IMPL(PointsToBounds)

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
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				//NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

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

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		if (Settings->OutputMode == EPCGExPointsToBoundsOutputMode::Collapse)
		{
			PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::New)
			OutputIO = PointDataFacade->Source;
			OutputFacade = PointDataFacade;
		}
		else
		{
			PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)
			OutputIO = MakeShared<PCGExData::FPointIO>(PointDataFacade->Source);
			OutputIO->InitializeOutput(PCGExData::EIOInit::New);
			OutputIO->Disable();
			OutputFacade = MakeShared<PCGExData::FFacade>(OutputIO.ToSharedRef());
		}

		Bounds = FBox(ForceInit);
		BestFitPlane = PCGExGeo::FBestFitPlane(PointDataFacade->GetIn()->GetConstTransformValueRange());
		FTransform InvTransform = BestFitPlane.GetTransform(Settings->AxisOrder).Inverse();

		const UPCGBasePointData* InPointData = OutputIO->GetIn();
		const int32 NumPoints = InPointData->GetNumPoints();

		TConstPCGValueRange<FTransform> InTransforms = InPointData->GetConstTransformValueRange();
		switch (Settings->BoundsSource)
		{
		default: ;
		case EPCGExPointBoundsSource::DensityBounds:
			if (Settings->bOutputOrientedBoundingBox) { for (int i = 0; i < NumPoints; i++) { Bounds += InPointData->GetDensityBounds(i).GetBox().TransformBy(InvTransform); } }
			else { for (int i = 0; i < NumPoints; i++) { Bounds += InPointData->GetDensityBounds(i).GetBox(); } }
			break;
		case EPCGExPointBoundsSource::ScaledBounds:
			if (Settings->bOutputOrientedBoundingBox) { for (int i = 0; i < NumPoints; i++) { Bounds += FBoxCenterAndExtent(InvTransform.TransformPosition(InTransforms[i].GetLocation()), InPointData->GetScaledExtents(i)).GetBox(); } }
			else { for (int i = 0; i < NumPoints; i++) { Bounds += FBoxCenterAndExtent(InTransforms[i].GetLocation(), InPointData->GetScaledExtents(i)).GetBox(); } }
			break;
		case EPCGExPointBoundsSource::Bounds:
			if (Settings->bOutputOrientedBoundingBox) { for (int i = 0; i < NumPoints; i++) { Bounds += FBoxCenterAndExtent(InvTransform.TransformPosition(InTransforms[i].GetLocation()), InPointData->GetExtents(i)).GetBox(); } }
			else { for (int i = 0; i < NumPoints; i++) { Bounds += FBoxCenterAndExtent(InTransforms[i].GetLocation(), InPointData->GetExtents(i)).GetBox(); } }
			break;
		case EPCGExPointBoundsSource::Center:
			if (Settings->bOutputOrientedBoundingBox) { for (int i = 0; i < NumPoints; i++) { Bounds += InvTransform.TransformPosition(InTransforms[i].GetLocation()); } }
			else { for (int i = 0; i < NumPoints; i++) { Bounds += InTransforms[i].GetLocation(); } }
			break;
		}

		return true;
	}

	void FProcessor::CompleteWork()
	{
		const UPCGBasePointData* InData = OutputIO->GetIn();
		UPCGBasePointData* OutData = OutputIO->GetOut();
		PCGEx::SetNumPointsAllocated(OutData, 1);

		OutputIO->InheritPoints(0, 0, 1);

		const double NumPoints = InData->GetNumPoints();

		if (Settings->bBlendProperties)
		{
			MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>();
			MetadataBlender->SetTargetData(OutputFacade);
			MetadataBlender->SetSourceData(PointDataFacade);

			if (!MetadataBlender->Init(Context, Settings->BlendingSettings))
			{
				bIsProcessorValid = false;
				return;
			}

			BlendedAttributes = MetadataBlender->GetAttributeIdentifiers();

			TArray<PCGEx::FOpStats> Trackers;
			MetadataBlender->InitTrackers(Trackers);
			MetadataBlender->BeginMultiBlend(0, Trackers);

			const PCGExData::FConstPoint Target = OutputIO->GetOutPoint(0);

			for (int i = 0; i < NumPoints; i++)
			{
				//FVector Location = InPoints[i].Transform.GetLocation();
				constexpr double Weight = 1; // FVector::DistSquared(Center, Location) / SqrDist;
				MetadataBlender->MultiBlend(i, 0, Weight, Trackers);
			}

			MetadataBlender->EndMultiBlend(0, Trackers);
		}

		TPCGValueRange<FTransform> OutTransforms = OutData->GetTransformValueRange(false);
		TPCGValueRange<FVector> OutBoundsMin = OutData->GetBoundsMinValueRange(false);
		TPCGValueRange<FVector> OutBoundsMax = OutData->GetBoundsMaxValueRange(false);

		if (Settings->bOutputOrientedBoundingBox)
		{
			OutTransforms[0] = BestFitPlane.GetTransform(Settings->AxisOrder);
			OutBoundsMin[0] = Bounds.Min;
			OutBoundsMax[0] = Bounds.Max;
			//PCGExMath::Swizzle(OutBoundsMin[0], BestFitPlane.Swizzle);
			//PCGExMath::Swizzle(OutBoundsMax[0], BestFitPlane.Swizzle);
		}
		else
		{
			const FVector Center = Bounds.GetCenter();
			OutTransforms[0] = FTransform(FQuat::Identity, Center);
			OutBoundsMin[0] = Bounds.Min - Center;
			OutBoundsMax[0] = Bounds.Max - Center;
		}


		if (Settings->bWritePointsCount) { WriteMark(OutputFacade->Source, Settings->PointsCountAttributeName, NumPoints); }

		OutputFacade->WriteSynchronous();

		if (Settings->OutputMode == EPCGExPointsToBoundsOutputMode::WriteData)
		{
			Settings->DataDetails.Output(OutputFacade->GetOut(), PointDataFacade->GetOut(), BlendedAttributes, BestFitPlane);
		}
		else
		{
			Settings->DataDetails.OutputInverse(PointDataFacade->GetIn(), OutputFacade->GetOut(), BlendedAttributes, BestFitPlane);
			PCGExGraph::CleanupClusterData(OutputFacade->Source);
		}
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

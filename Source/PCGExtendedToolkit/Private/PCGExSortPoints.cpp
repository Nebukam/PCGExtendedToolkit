// Fill out your copyright notice in the Description page of Project Settings.


#include "PCGExSortPoints.h"
#include "Data/PCGSpatialData.h"
#include "Helpers/PCGAsync.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExSortPointsElement"

namespace PCGExSortPoints
{
	const FName SourceLabel = TEXT("Source");
	const FName TargetLabel = TEXT("Target");

}

#if WITH_EDITOR
FText UPCGExSortPointsSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PCGDistanceTooltip", "Calculates and appends a signed 'Distance' attribute to the source data. For each of the source points, a distance attribute will be calculated between it and the nearest target point.");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGExSortPointsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertySource = PinProperties.Emplace_GetRef(PCGExSortPoints::SourceLabel, EPCGDataType::Point);
	FPCGPinProperties& PinPropertyTarget = PinProperties.Emplace_GetRef(PCGExSortPoints::TargetLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertySource.Tooltip = LOCTEXT("PCGSourcePinTooltip", "For each of the source points, a distance attribute will be calculated between it and the nearest target point.");
	PinPropertyTarget.Tooltip = LOCTEXT("PCGTargetPinTooltip", "The target points to conduct a distance check with each source point.");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExSortPointsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinPropertyOutput = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinPropertyOutput.Tooltip = LOCTEXT("PCGOutputPinTooltip", "The source points will be output with the newly added 'Distance' attribute as well as have their density set to [0,1] based on the 'Maximum Distance' if 'Set Density' is enabled.");
#endif // WITH_EDITOR
	
	return PinProperties;
}

FPCGElementPtr UPCGExSortPointsSettings::CreateElement() const
{
	return MakeShared<FPCGExSortPointsElement>();
}

bool FPCGExSortPointsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSortPointsElement::Execute);

	const UPCGExSortPointsSettings* Settings = Context->GetInputSettings<UPCGExSortPointsSettings>();
	check(Settings);

	const FName AttributeName = Settings->AttributeName;
	const bool bSetDensity = Settings->bSetDensity;
	const bool bOutputDistanceVector = Settings->bOutputDistanceVector;
	const double MaximumDistance = Settings->MaximumDistance;

	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExSortPoints::SourceLabel);
	TArray<FPCGTaggedData> Targets = Context->InputData.GetInputsByPin(PCGExSortPoints::TargetLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	TArray<const UPCGPointData*> TargetPointDatas;
	TargetPointDatas.Reserve(Targets.Num());

	for (const FPCGTaggedData& Target : Targets)
	{
		const UPCGSpatialData* TargetData = Cast<UPCGSpatialData>(Target.Data);

		if (!TargetData)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("TargetMustBeSpatial", "Target must be Spatial data, found '{0}'"), FText::FromString(Target.Data->GetClass()->GetName())));
			continue;
		}

		const UPCGPointData* TargetPointData = TargetData->ToPointData(Context);
		if (!TargetPointData)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("CannotConvertToPoint", "Cannot convert target '{0}' into Point data"), FText::FromString(Target.Data->GetClass()->GetName())));
			continue;			
		}

		TargetPointDatas.Add(TargetPointData);
	}

	// first find the total Input bounds which will determine the size of each cell
	for (const FPCGTaggedData& Source : Sources) 
	{
		// add the point bounds to the input cell

		const UPCGSpatialData* SourceData = Cast<UPCGSpatialData>(Source.Data);

		if (!SourceData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid input data"));
			continue;
		}

		const UPCGPointData* SourcePointData = SourceData->ToPointData(Context);
		if (!SourcePointData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("CannotConvertToPointData", "Cannot convert input Spatial data to Point data"));
			continue;			
		}

		UPCGPointData* OutputData = NewObject<UPCGPointData>();
		OutputData->InitializeFromData(SourcePointData);
		Outputs.Add_GetRef(Source).Data = OutputData;

		FPCGMetadataAttribute<float>* ScalarAttribute = nullptr;
		FPCGMetadataAttribute<FVector>* VectorAttribute = nullptr;

		if (AttributeName != NAME_None && !bOutputDistanceVector)
		{
			ScalarAttribute = OutputData->Metadata->FindOrCreateAttribute<float>(AttributeName, 0.0f);
		}

		if (AttributeName != NAME_None && bOutputDistanceVector)
		{
			VectorAttribute = OutputData->Metadata->FindOrCreateAttribute<FVector>(AttributeName, FVector::ZeroVector);
		}

		FPCGAsync::AsyncPointProcessing(Context, SourcePointData->GetPoints(), OutputData->GetMutablePoints(),
			[OutputData, &TargetPointDatas, MaximumDistance, ScalarAttribute, VectorAttribute, bSetDensity](const FPCGPoint& SourcePoint, FPCGPoint& OutPoint) {

				OutPoint = SourcePoint;

				const FBoxSphereBounds SourceQueryBounds = FBoxSphereBounds(FBox(SourcePoint.BoundsMin - FVector(MaximumDistance), SourcePoint.BoundsMax + FVector(MaximumDistance))).TransformBy(SourcePoint.Transform);

				const FVector SourceCenter = SourcePoint.Transform.TransformPosition(SourcePoint.GetLocalCenter());

				double MinDistanceSquared = MaximumDistance*MaximumDistance;
				FVector MinDistanceVector = FVector::ZeroVector;

				// Signed distance field for calculating the closest point of source and target
				auto CalculateSDF = [&MinDistanceSquared, &MinDistanceVector, &SourcePoint, SourceCenter](const FPCGPointRef& TargetPointRef)
				{
					// If the source pointer and target pointer are the same, ignore distance to the exact same point
					if (&SourcePoint == TargetPointRef.Point)
					{
						return;
					}
					
					const FPCGPoint& TargetPoint = *TargetPointRef.Point;
					const FVector& TargetCenter = TargetPointRef.Bounds.Origin;

					const FVector SourceShapePos = FVector::Zero();//PCGDistance::CalcPosition(SourceShape, SourcePoint, TargetPoint, SourceCenter, TargetCenter);
					const FVector TargetShapePos = FVector::Zero();//PCGDistance::CalcPosition(TargetShape, TargetPoint, SourcePoint, TargetCenter, SourceCenter);

					const FVector ToTargetShapeDir = TargetShapePos - SourceShapePos;
					const FVector ToTargetCenterDir = TargetCenter - SourceCenter;

					const double Sign = FMath::Sign(ToTargetShapeDir.Dot(ToTargetCenterDir));
					const double ThisDistanceSquared = ToTargetShapeDir.SquaredLength() * Sign;

					if (ThisDistanceSquared < MinDistanceSquared)
					{
						MinDistanceSquared = ThisDistanceSquared;
						MinDistanceVector = ToTargetShapeDir;
					}
				};

				for (const UPCGPointData* TargetPointData : TargetPointDatas)
				{
					const UPCGPointData::PointOctree& Octree = TargetPointData->GetOctree();

					Octree.FindElementsWithBoundsTest(
						FBoxCenterAndExtent(SourceQueryBounds.Origin, SourceQueryBounds.BoxExtent),
						CalculateSDF
					);
				}

				const float Distance = FMath::Sign(MinDistanceSquared) * FMath::Sqrt(FMath::Abs(MinDistanceSquared));

				if (ScalarAttribute || VectorAttribute)
				{
					OutputData->Metadata->InitializeOnSet(OutPoint.MetadataEntry);
				}

				if (ScalarAttribute)
				{
					ScalarAttribute->SetValue(OutPoint.MetadataEntry, Distance);
				}

				if (VectorAttribute)
				{
					VectorAttribute->SetValue(OutPoint.MetadataEntry, MinDistanceVector);
				}
				
				if (bSetDensity)
				{
					// set density instead
					OutPoint.Density = FMath::Clamp(Distance, -MaximumDistance, MaximumDistance) / MaximumDistance;
				}

				return true;
			}
		);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Geometry/PCGExGeoTasks.h"

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"
#include "PCGExMT.h"
#include "Transform/PCGExFitting.h"
#include "Data/PCGExPointIO.h"
#include "Async/ParallelFor.h"

namespace PCGExGeoTasks
{
	FTransformPointIO::FTransformPointIO(const int32 InTaskIndex,
						  const TSharedPtr<PCGExData::FPointIO>& InPointIO,
						  const TSharedPtr<PCGExData::FPointIO>& InToBeTransformedIO,
						  FPCGExTransformDetails* InTransformDetails,
						  bool bAllocate) :
			FPCGExIndexedTask(InTaskIndex),
			PointIO(InPointIO),
			ToBeTransformedIO(InToBeTransformedIO),
			TransformDetails(InTransformDetails)
	{
	}
	
	void FTransformPointIO::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		UPCGBasePointData* OutPointData = ToBeTransformedIO->GetOut();
		TPCGValueRange<FTransform> OutTransforms = OutPointData->GetTransformValueRange();
		FTransform TargetTransform = FTransform::Identity;

		FBox PointBounds = FBox(ForceInit);

		if (!TransformDetails->bIgnoreBounds)
		{
			for (int i = 0; i < OutTransforms.Num(); i++) { PointBounds += OutPointData->GetLocalBounds(i).TransformBy(OutTransforms[i]); }
		}
		else
		{
			for (const FTransform& Pt : OutTransforms) { PointBounds += Pt.GetLocation(); }
		}

		PointBounds = PointBounds.ExpandBy(0.1); // Avoid NaN
		TransformDetails->ComputeTransform(TaskIndex, TargetTransform, PointBounds);

		const int32 NumPoints = OutTransforms.Num();
		if (NumPoints < 4096)
		{
			if (TransformDetails->bInheritRotation && TransformDetails->bInheritScale)
			{
				for (FTransform& Transform : OutTransforms) { Transform *= TargetTransform; }
			}
			else if (TransformDetails->bInheritRotation)
			{
				for (FTransform& Transform : OutTransforms)
				{
					FVector OriginalScale = Transform.GetScale3D();
					Transform *= TargetTransform;
					Transform.SetScale3D(OriginalScale);
				}
			}
			else if (TransformDetails->bInheritScale)
			{
				for (FTransform& Transform : OutTransforms)
				{
					FQuat OriginalRot = Transform.GetRotation();
					Transform *= TargetTransform;
					Transform.SetRotation(OriginalRot);
				}
			}
			else
			{
				for (FTransform& Transform : OutTransforms)
				{
					Transform.SetLocation(TargetTransform.TransformPosition(Transform.GetLocation()));
				}
			}
		}
		else
		{
			if (TransformDetails->bInheritRotation && TransformDetails->bInheritScale)
			{
				ParallelFor(NumPoints, [&](const int32 i) { OutTransforms[i] *= TargetTransform; });
			}
			else if (TransformDetails->bInheritRotation)
			{
				ParallelFor(
					NumPoints, [&](const int32 i)
					{
						FTransform& Transform = OutTransforms[i];
						FVector OriginalScale = Transform.GetScale3D();
						Transform *= TargetTransform;
						Transform.SetScale3D(OriginalScale);
					});
			}
			else if (TransformDetails->bInheritScale)
			{
				ParallelFor(
					NumPoints, [&](const int32 i)
					{
						FTransform& Transform = OutTransforms[i];
						FQuat OriginalRot = Transform.GetRotation();
						Transform *= TargetTransform;
						Transform.SetRotation(OriginalRot);
					});
			}
			else
			{
				ParallelFor(
					NumPoints, [&](const int32 i)
					{
						FTransform& Transform = OutTransforms[i];
						Transform.SetLocation(TargetTransform.TransformPosition(Transform.GetLocation()));
					});
			}
		}
	}
}

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Fitting/PCGExFittingTasks.h"

#include "CoreMinimal.h"
#include "Data/PCGExPointIO.h"
#include "Fitting/PCGExFitting.h"
#include "Async/ParallelFor.h"

namespace PCGExFitting::Tasks
{
	FTransformPointIO::FTransformPointIO(const int32 InTaskIndex, const TSharedPtr<PCGExData::FPointIO>& InPointIO, const TSharedPtr<PCGExData::FPointIO>& InToBeTransformedIO, FPCGExTransformDetails* InTransformDetails, bool bAllocate)
		: FPCGExIndexedTask(InTaskIndex), PointIO(InPointIO), ToBeTransformedIO(InToBeTransformedIO), TransformDetails(InTransformDetails)
	{
	}

	void FTransformPointIO::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& TaskManager)
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

		const int Strategy = (TransformDetails->bInheritRotation ? 2 : 0)
			+ (TransformDetails->bInheritScale ? 1 : 0);

		switch (Strategy)
		{
		case 3: // Inherit rotation + inherit scale
			PCGEX_PARALLEL_FOR(
				OutTransforms.Num(),
				OutTransforms[i] *= TargetTransform;
			)
			break;
		case 2: // Inherit rotation only
			PCGEX_PARALLEL_FOR(
				OutTransforms.Num(),
				FTransform& Transform = OutTransforms[i];
				FQuat OriginalRot = Transform.GetRotation();
				Transform *= TargetTransform;
				Transform.SetRotation(OriginalRot);
			)
			break;
		case 1: // Inherit scale only
			PCGEX_PARALLEL_FOR(
				OutTransforms.Num(),
				FTransform& Transform = OutTransforms[i];
				FVector OriginalScale = Transform.GetScale3D();
				Transform *= TargetTransform;
				Transform.SetScale3D(OriginalScale);
			)
			break;
		default:
			PCGEX_PARALLEL_FOR(
				OutTransforms.Num(),
				FTransform& Transform = OutTransforms[i];
				Transform.SetLocation(TargetTransform.TransformPosition(Transform.GetLocation()));
			)
			break;
		}
	}
}

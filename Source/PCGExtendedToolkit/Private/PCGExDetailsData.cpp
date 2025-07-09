// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExDetailsData.h"

namespace PCGExDetails
{
	TSharedPtr<FDistances> MakeDistances(const EPCGExDistance Source, const EPCGExDistance Target, const bool bOverlapIsZero)
	{
		if (Source == EPCGExDistance::None || Target == EPCGExDistance::None)
		{
			return MakeShared<TDistances<EPCGExDistance::None, EPCGExDistance::None>>();
		}
		if (Source == EPCGExDistance::Center)
		{
			if (Target == EPCGExDistance::Center) { return MakeShared<TDistances<EPCGExDistance::Center, EPCGExDistance::Center>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::SphereBounds) { return MakeShared<TDistances<EPCGExDistance::Center, EPCGExDistance::SphereBounds>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::BoxBounds) { return MakeShared<TDistances<EPCGExDistance::Center, EPCGExDistance::BoxBounds>>(bOverlapIsZero); }
		}
		else if (Source == EPCGExDistance::SphereBounds)
		{
			if (Target == EPCGExDistance::Center) { return MakeShared<TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::Center>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::SphereBounds) { return MakeShared<TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::SphereBounds>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::BoxBounds) { return MakeShared<TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::BoxBounds>>(bOverlapIsZero); }
		}
		else if (Source == EPCGExDistance::BoxBounds)
		{
			if (Target == EPCGExDistance::Center) { return MakeShared<TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::Center>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::SphereBounds) { return MakeShared<TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::SphereBounds>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::BoxBounds) { return MakeShared<TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::BoxBounds>>(bOverlapIsZero); }
		}

		return nullptr;
	}

	TSharedPtr<FDistances> MakeNoneDistances()
	{
		return MakeShared<TDistances<EPCGExDistance::None, EPCGExDistance::None>>();
	}
}

TSharedPtr<PCGExDetails::FDistances> FPCGExDistanceDetails::MakeDistances() const
{
	return PCGExDetails::MakeDistances(Source, Target);
}

bool FPCGExInfluenceDetails::Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
{
	InfluenceBuffer = GetValueSettingInfluence();
	return InfluenceBuffer->Init(InContext, InPointDataFacade, false);
}

bool FPCGExFuseDetailsBase::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade)
{
	if (!bComponentWiseTolerance) { Tolerances = FVector(Tolerance); }

	if (!InDataFacade)
	{
		ToleranceGetter = PCGExDetails::MakeSettingValue<FVector>(Tolerances);
	}
	else
	{
		ToleranceGetter = PCGExDetails::MakeSettingValue<FVector>(ToleranceInput, ToleranceAttribute, Tolerances);
	}

	if (!ToleranceGetter->Init(InContext, InDataFacade)) { return false; }

	return true;
}

bool FPCGExFuseDetails::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade)
{
	if (!FPCGExFuseDetailsBase::Init(InContext, InDataFacade)) { return false; }

	DistanceDetails = PCGExDetails::MakeDistances(SourceDistance, TargetDistance);

	return true;
}

bool FPCGExManhattanDetails::IsValid() const
{
	return bInitialized;
}

bool FPCGExManhattanDetails::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade)
{
	if (bSupportAttribute)
	{
		GridSizeBuffer = GetValueSettingGridSize();
		if (!GridSizeBuffer->Init(InContext, InDataFacade)) { return false; }
	}
	else
	{
		GridSize = PCGExMath::Abs(GridSize);
		//GridIntSize = FIntVector3(FMath::Floor(GridSize.X), FMath::Floor(GridSize.Y), FMath::Floor(GridSize.Z));
		GridSizeBuffer = PCGExDetails::MakeSettingValue(GridSize);
	}

	PCGEx::GetAxisOrder(Order, Comps);

	bInitialized = true;
	return true;
}

int32 FPCGExManhattanDetails::ComputeSubdivisions(const FVector& A, const FVector& B, const int32 Index, TArray<FVector>& OutSubdivisions, double& OutDist) const
{
	FVector DirectionAndSize = B - A;
	const int32 StartIndex = OutSubdivisions.Num();

	/*
	FQuat Rotation = FQuat::Identity;

	if (SpaceAlign == EPCGExMinimalAxis::X) { Rotation = FRotationMatrix::MakeFromX(DirectionAndSize).ToQuat(); }
	else if (SpaceAlign == EPCGExMinimalAxis::Y) { Rotation = FRotationMatrix::MakeFromY(DirectionAndSize).ToQuat(); }
	else if (SpaceAlign == EPCGExMinimalAxis::Z) { Rotation = FRotationMatrix::MakeFromZ(DirectionAndSize).ToQuat(); }

	DirectionAndSize = Rotation.RotateVector(DirectionAndSize);
	*/

	if (Method == EPCGExManhattanMethod::Simple)
	{
		FVector Sub = A;
		for (int i = 0; i < 3; ++i)
		{
			const int32 Axis = Comps[i];
			const double Dist = B[Axis];

			if (FMath::IsNearlyZero(Dist)) { continue; }

			OutDist += Dist;
			Sub[Axis] = Dist;

			if (Sub == B) { break; }

			OutSubdivisions.Emplace(Sub);
		}
	}
	else if (Method == EPCGExManhattanMethod::GridDistance)
	{
	}
	else if (Method == EPCGExManhattanMethod::GridCount)
	{
	}

	//for (int i = StartIndex; i < OutSubdivisions.Num(); i++){ OutSubdivisions[i] = A + Rotation.UnrotateVector(OutSubdivisions[i]); }
	//for (int i = StartIndex; i < OutSubdivisions.Num(); i++) { OutSubdivisions[i] += A; }

	return OutSubdivisions.Num() - StartIndex;
}

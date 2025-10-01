// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExDetailsSubdivision.h"

#include "PCGExMath.h"
#include "Details/PCGExDetailsSettings.h"


TSharedPtr<PCGExDetails::TSettingValue<FVector>> FPCGExManhattanDetails::GetValueSettingGridSize(const bool bQuietErrors) const
{
	TSharedPtr<PCGExDetails::TSettingValue<FVector>> V = PCGExDetails::MakeSettingValue<FVector>(GridSizeInput, GridSizeAttribute, GridSize);
	V->bQuietErrors = bQuietErrors;
	return V;
}

TSharedPtr<PCGExDetails::TSettingValue<FQuat>> FPCGExManhattanDetails::GetValueSettingOrient(const bool bQuietErrors) const
{
	TSharedPtr<PCGExDetails::TSettingValue<FQuat>> V = PCGExDetails::MakeSettingValue<FQuat>(OrientInput, OrientAttribute, OrientConstant);
	V->bQuietErrors = bQuietErrors;
	return V;
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
		if (!GridSizeBuffer->Init(InDataFacade)) { return false; }

		if (SpaceAlign == EPCGExManhattanAlign::Custom) { OrientBuffer = GetValueSettingOrient(); }
		else if (SpaceAlign == EPCGExManhattanAlign::World) { OrientBuffer = PCGExDetails::MakeSettingValue(FQuat::Identity); }

		if (OrientBuffer && !OrientBuffer->Init(InDataFacade)) { return false; }
	}
	else
	{
		GridSize = PCGExMath::Abs(GridSize);
		//GridIntSize = FIntVector3(FMath::Floor(GridSize.X), FMath::Floor(GridSize.Y), FMath::Floor(GridSize.Z));
		GridSizeBuffer = PCGExDetails::MakeSettingValue(GridSize);
		if (SpaceAlign == EPCGExManhattanAlign::Custom) { OrientBuffer = PCGExDetails::MakeSettingValue(OrientConstant); }
		else if (SpaceAlign == EPCGExManhattanAlign::World) { OrientBuffer = PCGExDetails::MakeSettingValue(FQuat::Identity); }
	}

	PCGEx::GetAxisOrder(Order, Comps);

	bInitialized = true;
	return true;
}

int32 FPCGExManhattanDetails::ComputeSubdivisions(const FVector& A, const FVector& B, const int32 Index, TArray<FVector>& OutSubdivisions, double& OutDist) const
{
	FVector DirectionAndSize = B - A;
	const int32 StartIndex = OutSubdivisions.Num();

	FQuat Rotation = FQuat::Identity;

	switch (SpaceAlign)
	{
	case EPCGExManhattanAlign::World:
	case EPCGExManhattanAlign::Custom:
		Rotation = OrientBuffer->Read(Index);
		break;
	case EPCGExManhattanAlign::SegmentX:
		Rotation = FRotationMatrix::MakeFromX(DirectionAndSize).ToQuat();
		break;
	case EPCGExManhattanAlign::SegmentY:
		Rotation = FRotationMatrix::MakeFromY(DirectionAndSize).ToQuat();
		break;
	case EPCGExManhattanAlign::SegmentZ:
		Rotation = FRotationMatrix::MakeFromZ(DirectionAndSize).ToQuat();
		break;
	}

	DirectionAndSize = Rotation.RotateVector(DirectionAndSize);

	if (Method == EPCGExManhattanMethod::Simple)
	{
		OutSubdivisions.Reserve(OutSubdivisions.Num() + 3);

		FVector Sub = FVector::ZeroVector;
		for (int i = 0; i < 3; ++i)
		{
			const int32 Axis = Comps[i];
			const double Dist = DirectionAndSize[Axis];

			if (FMath::IsNearlyZero(Dist)) { continue; }

			OutDist += Dist;
			Sub[Axis] = Dist;

			if (Sub == B) { break; }

			OutSubdivisions.Emplace(Sub);
		}
	}
	else
	{
		FVector Subdivs = PCGExMath::Abs(GridSizeBuffer->Read(Index));
		FVector Maxes = PCGExMath::Abs(DirectionAndSize);
		if (Method == EPCGExManhattanMethod::GridCount)
		{
			Subdivs = FVector(
				FMath::Floor(Maxes.X / Subdivs.X),
				FMath::Floor(Maxes.Y / Subdivs.Y),
				FMath::Floor(Maxes.Z / Subdivs.Z));
		}

		const FVector StepSize = FVector::Min(Subdivs, Maxes);
		const FVector Sign = FVector(FMath::Sign(DirectionAndSize.X), FMath::Sign(DirectionAndSize.Y), FMath::Sign(DirectionAndSize.Z));

		FVector Sub = FVector::ZeroVector;

		bool bAdvance = true;
		while (bAdvance)
		{
			double DistBefore = OutDist;
			for (int i = 0; i < 3; ++i)
			{
				const int32 Axis = Comps[i];
				double Dist = StepSize[Axis];

				if (const double SubAbs = FMath::Abs(Sub[Axis]); SubAbs + Dist > Maxes[Axis]) { Dist = Maxes[Axis] - SubAbs; }
				if (FMath::IsNearlyZero(Dist)) { continue; }

				OutDist += Dist;
				Sub[Axis] += Dist * Sign[Axis];

				if (Sub == B)
				{
					bAdvance = false;
					break;
				}

				OutSubdivisions.Emplace(Sub);
			}

			if (DistBefore == OutDist) { bAdvance = false; }
		}
	}

	for (int i = StartIndex; i < OutSubdivisions.Num(); i++) { OutSubdivisions[i] = A + Rotation.UnrotateVector(OutSubdivisions[i]); }

	return OutSubdivisions.Num() - StartIndex;
}

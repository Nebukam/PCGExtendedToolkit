// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PCGData.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "PCGPoint.h"
#include "PCGExPointDataSorting.generated.h"

UENUM(BlueprintType)
enum class ESortDirection : uint8
{
	ASCENDING UMETA(DisplayName = "Ascending"),
	DESCENDING UMETA(DisplayName = "Descending")
};

UENUM(BlueprintType)
enum class ESortAxisOrder : uint8
{
	AXIS_X_Y_Z UMETA(DisplayName = "X → Y → Z"),
	AXIS_Y_X_Z UMETA(DisplayName = "Y → X → Z"),
	AXIS_Z_X_Y UMETA(DisplayName = "Z → X → Y"),
	AXIS_X_Z_Y UMETA(DisplayName = "X → Z → Y"),
	AXIS_Y_Z_X UMETA(DisplayName = "Y → Z → X"),
	AXIS_Z_Y_X UMETA(DisplayName = "Z → Y → X"),
	AXIS_LENGTH UMETA(DisplayName = "Vector length"),
};

/*
* Reminder:
* Return true if A should come before B in the sorted order.
* Return false if A should come after B.
*/

#pragma region Inline methods

class PCGEXTENDEDTOOLKIT_API VectorCompare {
public:

	static bool XYZ(const FVector& VA, const FVector& VB) {
		float DX = VA.X - VB.X;
		if (!FMath::IsNearlyZero(DX)) { return DX < 0; }
		float DY = VA.Y - VB.Y;
		if (!FMath::IsNearlyZero(DY)) { return DY < 0; }
		float DZ = VA.Z - VB.Z;
		if (!FMath::IsNearlyZero(DZ)) { return DZ < 0; }
		return false;
	}

	static bool Length(const FVector& VA, const FVector& VB) {	return VA.SquaredLength() < VB.SquaredLength();	}

};

#pragma endregion


#pragma region Singles

// Density

class PCGEXTENDEDTOOLKIT_API SortByDensity_ASC {
public:	bool operator()(const FPCGPoint& PtA, const FPCGPoint& PtB) const { return PtA.Density < PtB.Density; }
};

class PCGEXTENDEDTOOLKIT_API SortByDensity_DSC {
public:	bool operator()(const FPCGPoint& PtA, const FPCGPoint& PtB) const { return PtA.Density > PtB.Density; }
};

// Steepness

class PCGEXTENDEDTOOLKIT_API SortBySteepness_ASC {
public:	bool operator()(const FPCGPoint& PtA, const FPCGPoint& PtB) const { return PtA.Steepness < PtB.Steepness; }
};

class PCGEXTENDEDTOOLKIT_API SortBySteepness_DSC {
public:	bool operator()(const FPCGPoint& PtA, const FPCGPoint& PtB) const { return PtA.Steepness > PtB.Steepness; }
};

#pragma endregion

#pragma region Position

// XYZ

class PCGEXTENDEDTOOLKIT_API SortByPosition_XYZ_ASC {
public:	bool operator()(const FPCGPoint& PtA, const FPCGPoint& PtB) const {
	return VectorCompare::XYZ(PtA.Transform.GetLocation(), PtB.Transform.GetLocation());
}
};

class PCGEXTENDEDTOOLKIT_API SortByPosition_XYZ_DSC {
public:	bool operator()(const FPCGPoint& PtA, const FPCGPoint& PtB) const {
	return !VectorCompare::XYZ(PtA.Transform.GetLocation(), PtB.Transform.GetLocation());
}
};

// Length

class PCGEXTENDEDTOOLKIT_API SortByPositionLength_ASC {
public:	bool operator()(const FPCGPoint& PtA, const FPCGPoint& PtB) const {
	return VectorCompare::Length(PtA.Transform.GetLocation(), PtB.Transform.GetLocation());
}
};

class PCGEXTENDEDTOOLKIT_API SortByPositionLength_DSC {
public:	bool operator()(const FPCGPoint& PtA, const FPCGPoint& PtB) const {
	return !VectorCompare::Length(PtA.Transform.GetLocation(), PtB.Transform.GetLocation());
}
};

#pragma endregion
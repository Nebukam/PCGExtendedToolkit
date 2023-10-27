// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PCGData.h"
#include "PCGContext.h"
#include "PCGSettings.h"
#include "PCGPoint.h"
#include <map>
#include "PCGExPointDataSorting.generated.h"

// Sort function definition macros

#define SORTFUNC_SIMPLE_PAIR(_TYPE, _NAME, _ACCESSOR) \
static bool _TYPE##Compare##_NAME##Asc(const _TYPE& A, const _TYPE& B)	{\
return A._ACCESSOR < B._ACCESSOR;	}\
static bool _TYPE##Compare##_NAME##Dsc(const _TYPE& A, const _TYPE& B)	{\
return A._ACCESSOR > B._ACCESSOR;	}

#define SORTFUNC_3_ARGS(_TYPE, _A, _B, _C, _COMP, _ORDER) \
static bool _TYPE##Compare##_A##_B##_C##_ORDER(const _TYPE& V1, const _TYPE& V2) \
{ \
float D##_A = V1._A - V2._A; \
if (!FMath::IsNearlyZero(D##_A)) { return D##_A _COMP 0; } \
float D##_B = V1._B - V2._B; \
if (!FMath::IsNearlyZero(D##_B)) { return D##_B _COMP 0; } \
float D##_C = V1._C - V2._C; \
if (!FMath::IsNearlyZero(D##_C)) { return D##_C _COMP 0; } \
return false; \
}

#define SORTFUNC_3_PERMUTATIONS(_TYPE, _A, _B, _C, _COMP, _ORDER) \
SORTFUNC_3_ARGS(_TYPE, _A, _B, _C, _COMP, _ORDER) \
SORTFUNC_3_ARGS(_TYPE, _A, _C, _B, _COMP, _ORDER) \
SORTFUNC_3_ARGS(_TYPE, _B, _A, _C, _COMP, _ORDER) \
SORTFUNC_3_ARGS(_TYPE, _B, _C, _A, _COMP, _ORDER) \
SORTFUNC_3_ARGS(_TYPE, _C, _A, _B, _COMP, _ORDER) \
SORTFUNC_3_ARGS(_TYPE, _C, _B, _A, _COMP, _ORDER)

#define SORTFUNC_3(_TYPE, _A, _B, _C) \
SORTFUNC_3_PERMUTATIONS(_TYPE, _A, _B, _C, <, Asc) \
SORTFUNC_3_PERMUTATIONS(_TYPE, _A, _B, _C, >, Dsc)

// Predicate class definition macro

#define PREDICATE_SINGLE_FIELD_BASE(_NAME, _ACCESSOR, _ORDER) \
class PCGEXTENDEDTOOLKIT_API SortBy##_NAME##_##_ORDER {\
public:	bool operator()(const FPCGPoint& PtA, const FPCGPoint& PtB) const {	return PtA._ACCESSOR < PtB._ACCESSOR; }\
};

#define PREDICATE_FIELD_PAIR(_NAME, _ACCESSOR) \
PREDICATE_SINGLE_FIELD_BASE(_NAME, _ACCESSOR, Asc);\
PREDICATE_SINGLE_FIELD_BASE(_NAME, _ACCESSOR, Dsc);

#define PREDICATE_3_FIELDS_BASE(_TYPE, _NAME, _ENUM, _A, _B, _C, _ORDER, _ACCESSOR) \
class PCGEXTENDEDTOOLKIT_API SortBy##_NAME##_##_A##_B##_C##_##_ORDER {\
	public:	bool operator()(const FPCGPoint& PtA, const FPCGPoint& PtB) const {\
	return VectorCompare::_TYPE##Compare##_A##_B##_C##_ORDER(PtA._ACCESSOR, PtB._ACCESSOR); }\
};

#define PREDICATE_3_FIELDS_BASE_PAIR(_TYPE, _NAME, _ENUM, _A, _B, _C, _ACCESSOR) \
PREDICATE_3_FIELDS_BASE(_TYPE, _NAME, _ENUM, _A, _B, _C, Asc, _ACCESSOR)\
PREDICATE_3_FIELDS_BASE(_TYPE, _NAME, _ENUM, _A, _B, _C, Dsc, _ACCESSOR)

#define PREDICATE_3_FIELDS(_TYPE, _NAME, _A, _B, _C, _ACCESSOR) \
PREDICATE_3_FIELDS_BASE_PAIR(_TYPE, _NAME, XYZ, _A, _B, _C, _ACCESSOR)\
PREDICATE_3_FIELDS_BASE_PAIR(_TYPE, _NAME, XZY, _A, _C, _B, _ACCESSOR)\
PREDICATE_3_FIELDS_BASE_PAIR(_TYPE, _NAME, YXZ, _B, _A, _C, _ACCESSOR)\
PREDICATE_3_FIELDS_BASE_PAIR(_TYPE, _NAME, YZX, _B, _C, _A, _ACCESSOR)\
PREDICATE_3_FIELDS_BASE_PAIR(_TYPE, _NAME, ZXY, _C, _A, _B, _ACCESSOR)\
PREDICATE_3_FIELDS_BASE_PAIR(_TYPE, _NAME, ZYX, _C, _B, _A, _ACCESSOR)

#define PREDICATE_FVECTOR(_NAME, _ACCESSOR, _LENGTH_ACCESSOR) \
PREDICATE_3_FIELDS(FVector, _NAME, X, Y, Z, _ACCESSOR) \
PREDICATE_FIELD_PAIR(_NAME##Length, _LENGTH_ACCESSOR); \

#define PREDICATE_FROTATOR(_NAME, _ACCESSOR, _LENGTH_ACCESSOR) \
PREDICATE_3_FIELDS(FRotator, _NAME, Roll, Pitch, Yaw, _ACCESSOR) \
PREDICATE_FIELD_PAIR(_NAME##Length, _LENGTH_ACCESSOR); \

UENUM(BlueprintType)
enum class ESortDirection : uint8
{
	Ascending UMETA(DisplayName = "Ascending"),
	Descending UMETA(DisplayName = "Descending")
};

UENUM(BlueprintType)
enum class ESortAxisOrder : uint8
{
	Axis_X_Y_Z UMETA(DisplayName = "X → Y → Z"),
	Axis_X_Z_Y UMETA(DisplayName = "X → Z → Y"),
	Axis_Y_X_Z UMETA(DisplayName = "Y → X → Z"),
	Axis_Y_Z_X UMETA(DisplayName = "Y → Z → X"),
	Axis_Z_X_Y UMETA(DisplayName = "Z → X → Y"),
	Axis_Z_Y_X UMETA(DisplayName = "Z → Y → X"),
	Axis_Length UMETA(DisplayName = "Vector length"),
};

/*
* Reminder:
* Return true if A should come before B in the sorted order.
* Return false if A should come after B.
*/

#pragma region Inline methods

class PCGEXTENDEDTOOLKIT_API VectorCompare
{
public:
	typedef bool (*t_compare)(const FVector& VA, const FVector& VB);

	SORTFUNC_3(FVector, X, Y, Z)
	SORTFUNC_3(FRotator, Roll, Pitch, Yaw)
	SORTFUNC_SIMPLE_PAIR(FVector, Length, SquaredLength())
};

#pragma endregion

PREDICATE_FIELD_PAIR(Density, Density)

PREDICATE_FIELD_PAIR(Steepness, Steepness)

PREDICATE_FIELD_PAIR(Seed, Seed)

PREDICATE_FVECTOR(LocalCenter, GetLocalCenter(), GetLocalCenter().SquaredLength())

PREDICATE_FVECTOR(Position, Transform.GetLocation(), Transform.GetLocation().SquaredLength())

PREDICATE_FVECTOR(Scale, Transform.GetScale3D(), Transform.GetScale3D().SquaredLength())

PREDICATE_FROTATOR(Rotation, Transform.GetRotation().Rotator(), Transform.GetRotation().Rotator().Vector().SquaredLength())

PREDICATE_FVECTOR(Transform, Transform.GetLocation(), Transform.GetLocation().SquaredLength())

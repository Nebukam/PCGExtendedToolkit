// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGPoint.h"
#include "Metadata/PCGMetadataTypesConstantStruct.h"
#include "PCGExPointDataSorting.generated.h"

#pragma region Macros

#pragma region Sort function macros

// Sort function definition macros

#define PCGEX_SORTFUNC_SIMPLE_PAIR(_TYPE, _NAME, _ACCESSOR) \
static bool _TYPE##Compare##_NAME##Asc(const _TYPE& A, const _TYPE& B)	{\
return A._ACCESSOR < B._ACCESSOR;	}\
static bool _TYPE##Compare##_NAME##Dsc(const _TYPE& A, const _TYPE& B)	{\
return A._ACCESSOR > B._ACCESSOR;	}

#define PCGEX_SORTFUNC_3_ARGS(_TYPE, _A, _B, _C, _COMP, _ORDER) \
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

#define PCGEX_SORTFUNC_3_PERMUTATIONS(_TYPE, _A, _B, _C, _COMP, _ORDER) \
PCGEX_SORTFUNC_3_ARGS(_TYPE, _A, _B, _C, _COMP, _ORDER) \
PCGEX_SORTFUNC_3_ARGS(_TYPE, _A, _C, _B, _COMP, _ORDER) \
PCGEX_SORTFUNC_3_ARGS(_TYPE, _B, _A, _C, _COMP, _ORDER) \
PCGEX_SORTFUNC_3_ARGS(_TYPE, _B, _C, _A, _COMP, _ORDER) \
PCGEX_SORTFUNC_3_ARGS(_TYPE, _C, _A, _B, _COMP, _ORDER) \
PCGEX_SORTFUNC_3_ARGS(_TYPE, _C, _B, _A, _COMP, _ORDER)

#define PCGEX_SORTFUNC_3(_TYPE, _A, _B, _C) \
PCGEX_SORTFUNC_3_PERMUTATIONS(_TYPE, _A, _B, _C, <, Asc) \
PCGEX_SORTFUNC_3_PERMUTATIONS(_TYPE, _A, _B, _C, >, Dsc)

#pragma endregion

#pragma region Sort TArray shorthands

#define PCGEX_SORT_BY_SINGLE_FIELD(_NAME) \
if (SortDirection == ESortDirection::Ascending) Points.Sort(SortBy##_NAME##Asc()); else Points.Sort(SortBy##_NAME##Dsc());

#define PCGEX_SORT_LAMBDA_IN(_CAPTURE)\
[_CAPTURE](const FPCGPoint &PtA, const FPCGPoint &PtB)
#define PCGEX_INLINE_SORT_BY_SINGLE_FIELD(_NAME) \
if (SortDirection == ESortDirection::Ascending)\
{ Points.Sort( PCGEX_SORT_LAMBDA_IN() { return PtA._NAME < PtB._NAME; }); }\
else { Points.Sort( PCGEX_SORT_LAMBDA_IN() { return PtA._NAME > PtB._NAME; }); }
//

#define PCGEX_INLINE_SORT_BY_3_FIELD_BASE(_TYPE, _A, _B, _C, _ORDER, _ACCESSOR) \
Points.Sort( PCGEX_SORT_LAMBDA_IN() { return _TYPE##Compare##_A##_B##_C##_ORDER(PtA._ACCESSOR, PtB._ACCESSOR); });
#define PCGEX_INLINE_SORT_BY_3_FIELD_CASE(_TYPE, _ENUM, _A, _B, _C, _ORDER, _ACCESSOR) \
case ESortAxisOrder::Axis_##_ENUM: PCGEX_INLINE_SORT_BY_3_FIELD_BASE(_TYPE, _A, _B, _C, _ORDER, _ACCESSOR) break;
#define PCGEX_INLINE_SORT_BY_3_FIELD_CASELIST(_TYPE, _A, _B, _C, _ORDER, _ACCESSOR) \
PCGEX_INLINE_SORT_BY_3_FIELD_CASE(_TYPE, X_Y_Z, _A, _B, _C, _ORDER, _ACCESSOR) \
PCGEX_INLINE_SORT_BY_3_FIELD_CASE(_TYPE, X_Z_Y, _A, _C, _B, _ORDER, _ACCESSOR) \
PCGEX_INLINE_SORT_BY_3_FIELD_CASE(_TYPE, Y_X_Z, _B, _A, _C, _ORDER, _ACCESSOR) \
PCGEX_INLINE_SORT_BY_3_FIELD_CASE(_TYPE, Y_Z_X, _B, _C, _A, _ORDER, _ACCESSOR) \
PCGEX_INLINE_SORT_BY_3_FIELD_CASE(_TYPE, Z_X_Y, _C, _A, _B, _ORDER, _ACCESSOR) \
PCGEX_INLINE_SORT_BY_3_FIELD_CASE(_TYPE, Z_Y_X, _C, _B, _A, _ORDER, _ACCESSOR)

#define PCGEX_INLINE_SORT_BY_3_FIELD(_TYPE, _A, _B, _C, _ACCESSOR) \
if (SortDirection == ESortDirection::Ascending)\
{ switch(SortOrder){ PCGEX_INLINE_SORT_BY_3_FIELD_CASELIST(_TYPE, _A, _B, _C, Asc, _ACCESSOR) }\
} else { switch(SortOrder){ PCGEX_INLINE_SORT_BY_3_FIELD_CASELIST(_TYPE, _A, _B, _C, Dsc, _ACCESSOR) }}
#define PCGEX_INLINE_SORT_BY_FVECTOR(_ACCESSOR) \
PCGEX_INLINE_SORT_BY_3_FIELD(FVector, X, Y, Z, _ACCESSOR);
#define PCGEX_INLINE_SORT_BY_FROTATOR(_ACCESSOR) \
PCGEX_INLINE_SORT_BY_3_FIELD(FRotator, Roll, Pitch, Yaw, _ACCESSOR);
#define PCGEX_INLINE_SORT_BY_FRGB(_ACCESSOR) \
PCGEX_INLINE_SORT_BY_3_FIELD(FColor, R, G, B, _ACCESSOR);
#pragma endregion

#pragma region Predicates macros

// Predicate class definition macro

#define PCGEX_PREDICATE_SINGLE_FIELD_BASE(_NAME, _ACCESSOR, _ORDER) \
class PCGEXTENDEDTOOLKIT_API SortBy##_NAME##_ORDER {\
public:	bool operator()(const FPCGPoint& PtA, const FPCGPoint& PtB) const {	return PtA._ACCESSOR < PtB._ACCESSOR; }\
};

#define PCGEX_PREDICATE_FIELD_PAIR(_NAME, _ACCESSOR) \
PCGEX_PREDICATE_SINGLE_FIELD_BASE(_NAME, _ACCESSOR, Asc);\
PCGEX_PREDICATE_SINGLE_FIELD_BASE(_NAME, _ACCESSOR, Dsc);

#define PCGEX_PREDICATE_3_FIELDS_BASE(_TYPE, _NAME, _ENUM, _A, _B, _C, _ORDER, _ACCESSOR) \
class PCGEXTENDEDTOOLKIT_API SortBy##_NAME##_##_A##_B##_C##_##_ORDER {\
	public:	bool operator()(const FPCGPoint& PtA, const FPCGPoint& PtB) const {\
	return PCGExPointDataSorting::_TYPE##Compare##_A##_B##_C##_ORDER(PtA._ACCESSOR, PtB._ACCESSOR); }\
};

#define PCGEX_PREDICATE_3_FIELDS_BASE_PAIR(_TYPE, _NAME, _ENUM, _A, _B, _C, _ACCESSOR) \
PCGEX_PREDICATE_3_FIELDS_BASE(_TYPE, _NAME, _ENUM, _A, _B, _C, Asc, _ACCESSOR)\
PCGEX_PREDICATE_3_FIELDS_BASE(_TYPE, _NAME, _ENUM, _A, _B, _C, Dsc, _ACCESSOR)

#define PCGEX_PREDICATE_3_FIELDS(_TYPE, _NAME, _A, _B, _C, _ACCESSOR) \
PCGEX_PREDICATE_3_FIELDS_BASE_PAIR(_TYPE, _NAME, XYZ, _A, _B, _C, _ACCESSOR)\
PCGEX_PREDICATE_3_FIELDS_BASE_PAIR(_TYPE, _NAME, XZY, _A, _C, _B, _ACCESSOR)\
PCGEX_PREDICATE_3_FIELDS_BASE_PAIR(_TYPE, _NAME, YXZ, _B, _A, _C, _ACCESSOR)\
PCGEX_PREDICATE_3_FIELDS_BASE_PAIR(_TYPE, _NAME, YZX, _B, _C, _A, _ACCESSOR)\
PCGEX_PREDICATE_3_FIELDS_BASE_PAIR(_TYPE, _NAME, ZXY, _C, _A, _B, _ACCESSOR)\
PCGEX_PREDICATE_3_FIELDS_BASE_PAIR(_TYPE, _NAME, ZYX, _C, _B, _A, _ACCESSOR)

#define PCGEX_PREDICATE_FVECTOR(_NAME, _ACCESSOR, _LENGTH_ACCESSOR) \
PCGEX_PREDICATE_3_FIELDS(FVector, _NAME, X, Y, Z, _ACCESSOR) \
PCGEX_PREDICATE_FIELD_PAIR(_NAME##Length, _LENGTH_ACCESSOR);
#define PCGEX_PREDICATE_FROTATOR(_NAME, _ACCESSOR, _LENGTH_ACCESSOR) \
PCGEX_PREDICATE_3_FIELDS(FRotator, _NAME, Roll, Pitch, Yaw, _ACCESSOR) \
PCGEX_PREDICATE_FIELD_PAIR(_NAME##Length, _LENGTH_ACCESSOR);
#pragma endregion

#pragma endregion

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

class PCGEXTENDEDTOOLKIT_API PCGExPointDataSorting
{
public:
	typedef bool (*t_compare)(const FVector& VA, const FVector& VB);

	PCGEX_SORTFUNC_3(FVector, X, Y, Z)
	PCGEX_SORTFUNC_3(FRotator, Roll, Pitch, Yaw)
	PCGEX_SORTFUNC_3(FColor, R, G, B)
	PCGEX_SORTFUNC_SIMPLE_PAIR(FVector, Length, SquaredLength())

	static void Sort(TArray<FPCGPoint>& Points, EPCGPointProperties SortOver,
	                 ESortDirection SortDirection, ESortAxisOrder SortOrder)
	{
		switch (SortOver)
		{
		case EPCGPointProperties::Density:
			PCGEX_INLINE_SORT_BY_SINGLE_FIELD(Density)
			break;
		case EPCGPointProperties::BoundsMin:
			PCGEX_INLINE_SORT_BY_FVECTOR(BoundsMin)
			break;
		case EPCGPointProperties::BoundsMax:
			PCGEX_INLINE_SORT_BY_FVECTOR(BoundsMax)
			break;
		case EPCGPointProperties::Extents:
			PCGEX_INLINE_SORT_BY_FVECTOR(GetExtents())
			break;
		case EPCGPointProperties::Color:
			PCGEX_INLINE_SORT_BY_FVECTOR(Color)
			break;
		case EPCGPointProperties::Position:
			PCGEX_INLINE_SORT_BY_FVECTOR(Transform.GetLocation())
			break;
		case EPCGPointProperties::Rotation:
			PCGEX_INLINE_SORT_BY_FROTATOR(Transform.GetRotation().Rotator())
			break;
		case EPCGPointProperties::Scale:
			PCGEX_INLINE_SORT_BY_FVECTOR(Transform.GetScale3D())
			break;
		case EPCGPointProperties::Transform:
			PCGEX_INLINE_SORT_BY_FVECTOR(Transform.GetLocation())
			break;
		case EPCGPointProperties::Steepness:
			PCGEX_INLINE_SORT_BY_SINGLE_FIELD(Steepness)
			break;
		case EPCGPointProperties::LocalCenter:
			PCGEX_INLINE_SORT_BY_FVECTOR(GetLocalCenter())
			break;
		case EPCGPointProperties::Seed:
			PCGEX_INLINE_SORT_BY_SINGLE_FIELD(Seed)
			break;
		default: break;
		}
	}
	
	static void SortByAttribute(TArray<FPCGPoint>& Points, EPCGPointProperties SortOver,
					 ESortDirection SortDirection, ESortAxisOrder SortOrder)
	{
		EPCGMetadataTypes Type = EPCGMetadataTypes::Boolean;
		switch (Type) {
		case EPCGMetadataTypes::Float:
			break;
		case EPCGMetadataTypes::Double:
			break;
		case EPCGMetadataTypes::Integer32:
			break;
		case EPCGMetadataTypes::Integer64:
			break;
		case EPCGMetadataTypes::Vector2:
			break;
		case EPCGMetadataTypes::Vector:
			break;
		case EPCGMetadataTypes::Vector4:
			break;
		case EPCGMetadataTypes::Quaternion:
			break;
		case EPCGMetadataTypes::Transform:
			break;
		case EPCGMetadataTypes::String:
			break;
		case EPCGMetadataTypes::Boolean:
			break;
		case EPCGMetadataTypes::Rotator:
			break;
		case EPCGMetadataTypes::Name:
			break;
		case EPCGMetadataTypes::Count:
			break;
		case EPCGMetadataTypes::Unknown:
			break;
		default: ;
		}
	}

#pragma region Predicates

	/* // Use Inline instead of predicate for now
	 
	PCGEX_PREDICATE_FIELD_PAIR(Density, Density)

	PCGEX_PREDICATE_FIELD_PAIR(Steepness, Steepness)

	PCGEX_PREDICATE_FIELD_PAIR(Seed, Seed)

	PCGEX_PREDICATE_FVECTOR(LocalCenter, GetLocalCenter(), GetLocalCenter().SquaredLength())

	PCGEX_PREDICATE_FVECTOR(Position, Transform.GetLocation(), Transform.GetLocation().SquaredLength())

	PCGEX_PREDICATE_FVECTOR(Scale, Transform.GetScale3D(), Transform.GetScale3D().SquaredLength())

	PCGEX_PREDICATE_FROTATOR(Rotation, Transform.GetRotation().Rotator(),
	                   Transform.GetRotation().Rotator().Vector().SquaredLength())

	PCGEX_PREDICATE_FVECTOR(Transform, Transform.GetLocation(), Transform.GetLocation().SquaredLength())
	*/
#pragma endregion

protected:
	template <typename T>
	static bool SortByAttribute(T param) {
		// Function code here
		// You can use 'param' as a placeholder for the data type you specify when calling the function.
	}
	
};

#pragma region Undef macros

#undef PCGEX_SORTFUNC_SIMPLE_PAIR
#undef PCGEX_SORTFUNC_3_ARGS
#undef PCGEX_SORTFUNC_3_PERMUTATIONS
#undef PCGEX_SORTFUNC_3
#undef PCGEX_SORT_BY_SINGLE_FIELD
#undef PCGEX_SORT_LAMBDA_IN
#undef PCGEX_INLINE_SORT_BY_SINGLE_FIELD

#undef PCGEX_INLINE_SORT_BY_3_FIELD_BASE
#undef PCGEX_INLINE_SORT_BY_3_FIELD_CASE
#undef PCGEX_INLINE_SORT_BY_3_FIELD_CASELIST
#undef PCGEX_INLINE_SORT_BY_3_FIELD
#undef PCGEX_INLINE_SORT_BY_FVECTOR
#undef PCGEX_INLINE_SORT_BY_FROTATOR
#undef PCGEX_INLINE_SORT_BY_FRGB

#undef PCGEX_PREDICATE_SINGLE_FIELD_BASE
#undef PCGEX_PREDICATE_FIELD_PAIR
#undef PCGEX_PREDICATE_3_FIELDS_BASE
#undef PCGEX_PREDICATE_3_FIELDS_BASE_PAIR
#undef PCGEX_PREDICATE_3_FIELDS
#undef PCGEX_PREDICATE_FVECTOR
#undef PCGEX_PREDICATE_FROTATOR

#pragma endregion

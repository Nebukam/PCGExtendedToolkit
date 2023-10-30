// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PCGExAttributesUtils.h"
#include "PCGPoint.h"
#include "Metadata/PCGMetadataTypesConstantStruct.h"
#include "PCGExPointSortHelpers.generated.h"

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

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSortAttributeDetails
{
	GENERATED_BODY()

public:
	/** Name of the attribute to compare */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FName AttributeName = "AttributeName";

	/** Sub-sorting order, used only for multi-field attributes (FVector, FRotator etc). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	ESortAxisOrder SortOrder = ESortAxisOrder::Axis_X_Y_Z;
};

/*
* Reminder:
* Return true if A should come before B in the sorted order.
* Return false if A should come after B.
*/

class PCGEXTENDEDTOOLKIT_API PCGExPointSortHelpers
{
public:
#pragma region COMPARE METHODS

#pragma region Compare Macros

	// Single Fields
#define PCGEX_COMPARE_SINGLE(_TYPE, _NAME, _ACCESSOR, _COMP, _DIR) \
	static int Compare_##_TYPE##_NAME##_DIR(const _TYPE& A, const _TYPE& B)	{\
	float VA = A._ACCESSOR, VB = B._ACCESSOR; \
	return VA _COMP VB ? -1 : FMath::IsNearlyEqual(VA, VB) ? 0 : 1;	}
#define PCGEX_COMPARE_PAIR(_TYPE, _NAME, _ACCESSOR) \
	PCGEX_COMPARE_SINGLE(_TYPE, _NAME, _ACCESSOR, <, Asc) \
	PCGEX_COMPARE_SINGLE(_TYPE, _NAME, _ACCESSOR, >, Dsc)
#define PCGEX_RETURN_NON_ZERO if(Result != 0){return Result;}
#define PCGEX_COMPARE_FIELD_ROW(_A, _COMP) \
	Result = V1._A _COMP V2._A ? -1 : FMath::IsNearlyEqual(V1._A, V2._A) ? 0 : 1;
	// 2 Fields

#define PCGEX_COMPARE_2_FIELDS_BASE(_TYPE, _A, _B, _COMP, _DIR) \
	static int Compare_##_TYPE##_A##_B##_DIR(const _TYPE& V1, const _TYPE& V2) \
	{ \
	int PCGEX_COMPARE_FIELD_ROW(_A, _COMP) PCGEX_RETURN_NON_ZERO\
	PCGEX_COMPARE_FIELD_ROW(_B, _COMP) \
	return Result; \
	}

#define PCGEX_COMPARE_2_FIELDS(_ID, _TYPE, _A, _B, ...) \
	PCGEX_COMPARE_2_FIELDS_BASE(_TYPE, _A, _B, <, Asc) \
	PCGEX_COMPARE_2_FIELDS_BASE(_TYPE, _A, _B, >, Dsc) \
	PCGEX_COMPARE_2_FIELDS_BASE(_TYPE, _B, _A, <, Asc) \
	PCGEX_COMPARE_2_FIELDS_BASE(_TYPE, _B, _A, >, Dsc)
	// 3 Fields

#define PCGEX_COMPARE_3_FIELDS_BASE(_TYPE, _A, _B, _C, _COMP, _DIR, ...) \
	static int Compare_##_TYPE##_A##_B##_C##_DIR(const _TYPE& V1, const _TYPE& V2) \
	{ \
	int PCGEX_COMPARE_FIELD_ROW(_A, _COMP) PCGEX_RETURN_NON_ZERO\
	PCGEX_COMPARE_FIELD_ROW(_B, _COMP) PCGEX_RETURN_NON_ZERO\
	PCGEX_COMPARE_FIELD_ROW(_C, _COMP) \
	return Result; \
	}

#define PCGEX_COMPARE_3_FIELDS_PAIR(_TYPE, _A, _B, _C) \
	PCGEX_COMPARE_3_FIELDS_BASE(_TYPE, _A, _B, _C, <, Asc) \
	PCGEX_COMPARE_3_FIELDS_BASE(_TYPE, _A, _B, _C, >, Dsc)
#define PCGEX_COMPARE_3_FIELDS(_ID, _TYPE, _A, _B, _C, ...) \
	PCGEX_COMPARE_3_FIELDS_PAIR(_TYPE, _A, _B, _C) \
	PCGEX_COMPARE_3_FIELDS_PAIR(_TYPE, _A, _C, _B) \
	PCGEX_COMPARE_3_FIELDS_PAIR(_TYPE, _B, _A, _C) \
	PCGEX_COMPARE_3_FIELDS_PAIR(_TYPE, _B, _C, _A) \
	PCGEX_COMPARE_3_FIELDS_PAIR(_TYPE, _C, _A, _B) \
	PCGEX_COMPARE_3_FIELDS_PAIR(_TYPE, _C, _B, _A)

#pragma endregion

	PCGEX_FOREACH_SUPPORTEDTYPES_2_FIELDS(PCGEX_COMPARE_2_FIELDS)
	PCGEX_FOREACH_SUPPORTEDTYPES_3_FIELDS(PCGEX_COMPARE_3_FIELDS)
	PCGEX_COMPARE_PAIR(FVector2D, Length, SquaredLength())
	PCGEX_COMPARE_PAIR(FVector, Length, SquaredLength())

	template <typename T>
	static int CompareAsc(const T& A, const T& B) { return A < B ? -1 : A == B ? 0 : 1; }

	template <typename T>
	static int CompareDsc(const T& A, const T& B) { return A > B ? -1 : A == B ? 0 : 1; }

	static int CompareFName(const FName& A, const FName& B) { return CompareAsc(A.ToString(), B.ToString()); }

	static int CompareFNameAsc(const FName& A, const FName& B) { return CompareFName(A, B); }
	static int CompareFNameDsc(const FName& A, const FName& B) { return CompareFName(A, B) * -1; }


#pragma region Undef Compare Macros

#undef PCGEX_COMPARE_SINGLE
#undef PCGEX_COMPARE_PAIR
#undef PCGEX_RETURN_NON_ZERO
#undef PCGEX_COMPARE_FIELD_ROW
#undef PCGEX_COMPARE_2_FIELDS_BASE
#undef PCGEX_COMPARE_2_FIELDS
#undef PCGEX_COMPARE_3_FIELDS_BASE
#undef PCGEX_COMPARE_3_FIELDS_PAIR
#undef PCGEX_COMPARE_3_FIELDS

#pragma endregion

#pragma endregion

#pragma region SORT BY SINGLE PROPERTY

	static void Sort(TArray<FPCGPoint>& Points, EPCGPointProperties SortOver,
	                 ESortDirection SortDirection, ESortAxisOrder SortOrder)
	{
#pragma region Sort lambdas

#define PCGEX_SORT_LAMBDA_IN(...)\
	[__VA_ARGS__](const FPCGPoint &PtA, const FPCGPoint &PtB)

#define PCGEX_SORT_COMPARE(_DIR, _TYPE, _ACCESSOR)\
Points.Sort( PCGEX_SORT_LAMBDA_IN() { return Compare##_DIR(PtA._ACCESSOR, PtB._ACCESSOR) < 0; });
#define PCGEX_SORT_COMPARE_TYPED(_DIR, _TYPE, _ACCESSOR, _COMPONENTS)\
Points.Sort( PCGEX_SORT_LAMBDA_IN() { return Compare_##_TYPE##_COMPONENTS##_DIR(PtA._ACCESSOR, PtB._ACCESSOR) < 0; });
#define PCGEX_SORT_COMPARE_2_FIELDS(_DIR, _TYPE, _A, _B, _ACCESSOR)\
switch(SortOrder){\
case ESortAxisOrder::Axis_X_Y_Z: case ESortAxisOrder::Axis_X_Z_Y: case ESortAxisOrder::Axis_Z_X_Y: PCGEX_SORT_COMPARE_TYPED(_DIR, _TYPE, _ACCESSOR, _A##_B) break; \
case ESortAxisOrder::Axis_Y_X_Z: case ESortAxisOrder::Axis_Y_Z_X: case ESortAxisOrder::Axis_Z_Y_X: PCGEX_SORT_COMPARE_TYPED(_DIR, _TYPE, _ACCESSOR, _B##_A) break; \
}

#define PCGEX_SORT_COMPARE_3_FIELDS(_DIR, _TYPE, _A, _B, _C, _ACCESSOR)\
	switch(SortOrder){\
	case ESortAxisOrder::Axis_X_Y_Z: PCGEX_SORT_COMPARE_TYPED(_DIR, _TYPE, _ACCESSOR, _A##_B##_C) break; \
	case ESortAxisOrder::Axis_X_Z_Y: PCGEX_SORT_COMPARE_TYPED(_DIR, _TYPE, _ACCESSOR, _A##_C##_B) break; \
	case ESortAxisOrder::Axis_Y_X_Z: PCGEX_SORT_COMPARE_TYPED(_DIR, _TYPE, _ACCESSOR, _B##_A##_C) break; \
	case ESortAxisOrder::Axis_Y_Z_X: PCGEX_SORT_COMPARE_TYPED(_DIR, _TYPE, _ACCESSOR, _B##_C##_A) break; \
	case ESortAxisOrder::Axis_Z_X_Y: PCGEX_SORT_COMPARE_TYPED(_DIR, _TYPE, _ACCESSOR, _C##_A##_B) break; \
	case ESortAxisOrder::Axis_Z_Y_X: PCGEX_SORT_COMPARE_TYPED(_DIR, _TYPE, _ACCESSOR, _C##_B##_A) break; \
	}

#define PCGEX_SORT_FVECTOR(_DIR, ...) \
	PCGEX_SORT_COMPARE_3_FIELDS(_DIR, FVector, X, Y, Z, __VA_ARGS__)
#define PCGEX_SORT_FROTATOR(_DIR, ...) \
	PCGEX_SORT_COMPARE_3_FIELDS(_DIR, FRotator, Roll, Pitch, Yaw, __VA_ARGS__)
#define PCGEX_SORT(MACRO, ...)\
if (SortDirection == ESortDirection::Ascending){ MACRO(Asc, __VA_ARGS__) }\
else { MACRO(Dsc, __VA_ARGS__) }
#pragma endregion

		switch (SortOver)
		{
		case EPCGPointProperties::Density:
			PCGEX_SORT(PCGEX_SORT_COMPARE, float, Density)
			break;
		case EPCGPointProperties::BoundsMin:
			PCGEX_SORT(PCGEX_SORT_FVECTOR, BoundsMin)
			break;
		case EPCGPointProperties::BoundsMax:
			PCGEX_SORT(PCGEX_SORT_FVECTOR, BoundsMax)
			break;
		case EPCGPointProperties::Extents:
			PCGEX_SORT(PCGEX_SORT_FVECTOR, GetExtents())
			break;
		case EPCGPointProperties::Color:
			PCGEX_SORT(PCGEX_SORT_FVECTOR, Color)
			break;
		case EPCGPointProperties::Transform:
		case EPCGPointProperties::Position:
			PCGEX_SORT(PCGEX_SORT_FVECTOR, Transform.GetLocation())
			break;
		case EPCGPointProperties::Rotation:
			PCGEX_SORT(PCGEX_SORT_FROTATOR, Transform.GetRotation().Rotator())
			break;
		case EPCGPointProperties::Scale:
			PCGEX_SORT(PCGEX_SORT_FVECTOR, Transform.GetScale3D())
			break;
		case EPCGPointProperties::Steepness:
			PCGEX_SORT(PCGEX_SORT_COMPARE, float, Steepness)
			break;
		case EPCGPointProperties::LocalCenter:
			PCGEX_SORT(PCGEX_SORT_FVECTOR, GetLocalCenter())
			break;
		case EPCGPointProperties::Seed:
			PCGEX_SORT(PCGEX_SORT_COMPARE, float, Seed)
			break;
		default: break;
		}

#pragma region Undef lambdas

#undef PCGEX_SORT_LAMBDA_IN
#undef PCGEX_SORT_COMPARE
#undef PCGEX_SORT_COMPARE_TYPED
#undef PCGEX_SORT_COMPARE_2_FIELDS
#undef PCGEX_SORT_COMPARE_3_FIELDS
#undef PCGEX_SORT_FVECTOR
#undef PCGEX_SORT_FROTATOR
#undef PCGEX_SORT

#pragma endregion
	}

#pragma endregion

#pragma region SORT BY ATTRIBUTES

	static bool IsSortable(const EPCGMetadataTypes& Type)
	{
		if (Type == EPCGMetadataTypes::Unknown) { return false; }
		return true;
	}

	static void Sort(TArray<FPCGPoint>& Points, TArray<FPCGExAttributeProxy>& SortableAttributes, TArray<FPCGExSortAttributeDetails>& PerAttributeDetails, ESortDirection SortDirection)
	{
#pragma region Macros

#define PCGEX_MEMBER_MAP(_ENUM, _TYPE, ...) \
TMap<int, FPCGMetadataAttribute<_TYPE>*> Map##_ENUM; \
// Map##_ENUM.Reserve(SortableAttributes.Num());

#define PCGEX_MEMBER_ATT(_ENUM, _TYPE, ...) \
FPCGMetadataAttribute<_TYPE>* Attribute##_ENUM = nullptr;
#define PCGEX_ATT_MAP_CAPTURE(_ENUM, _TYPE, ...) &Map##_ENUM,
#define PCGEX_ATT_MAP_PUSH(_ENUM, _TYPE, ...) \
case EPCGMetadataTypes::_ENUM : \
Attribute##_ENUM = static_cast<FPCGMetadataAttribute<_TYPE>*>(Proxy.Attribute); \
check(Attribute##_ENUM) \
Map##_ENUM.Add(i, Attribute##_ENUM); \
break;
#define PCGEX_ATT_MAP_FETCH(_ENUM, _TYPE, ...) Attribute##_ENUM = Map##_ENUM[i];
#define PCGEX_ATT_COMPARE_SINGLE(_ENUM, _TYPE, _DIR, ...) \
case EPCGMetadataTypes::_ENUM: \
PCGEX_ATT_MAP_FETCH(_ENUM, _TYPE) \
Result = Compare##_DIR(Attribute##_ENUM->GetValue(PtA.MetadataEntry), Attribute##_ENUM->GetValue(PtB.MetadataEntry)); \
break;
#define PCGEX_ATT_COMPARE_SINGLE_CUSTOM(_ENUM, _TYPE, _DIR, _FUNC, ...) \
case EPCGMetadataTypes::_ENUM: \
PCGEX_ATT_MAP_FETCH(_ENUM, _TYPE) \
Result = _FUNC##_DIR(Attribute##_ENUM->GetValue(PtA.MetadataEntry), Attribute##_ENUM->GetValue(PtB.MetadataEntry)); \
break;

#define PCGEX_ATT_COMPARE_2_FIELDS(_ENUM, _TYPE, _DIR, ...) \
case EPCGMetadataTypes::_ENUM: \
PCGEX_ATT_MAP_FETCH(_ENUM, _TYPE) \
break;

#define PCGEX_ATT_COMPARE_3_FIELDS(_ENUM, _TYPE, _DIR, ...) \
case EPCGMetadataTypes::_ENUM: \
PCGEX_ATT_MAP_FETCH(_ENUM, _TYPE) \
break;

#define COMPARE_LOOP_BODY(_DIR)\
for (int i = 0; i < MaxIterations; i++) { \
FPCGExSortAttributeDetails CurrentDetail = PerAttributeDetails[i]; \
const FPCGExAttributeProxy Proxy = SortableAttributes[i]; \
int Result = 0; \
switch(Proxy.Type){ \
PCGEX_FOREACH_SUPPORTEDTYPES_SINGLE_SAFE(PCGEX_ATT_COMPARE_SINGLE, _DIR) \
PCGEX_ATT_COMPARE_SINGLE_CUSTOM(Name, FName, _DIR, CompareFName) \
PCGEX_FOREACH_SUPPORTEDTYPES_2_FIELDS(PCGEX_ATT_COMPARE_2_FIELDS, _DIR) \
PCGEX_FOREACH_SUPPORTEDTYPES_3_FIELDS(PCGEX_ATT_COMPARE_3_FIELDS, _DIR) \
default:; } \
if (Result == 0) { continue; } \
return Result < 0; \
	}
#pragma endregion

		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_MEMBER_MAP)
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_MEMBER_ATT)

		const int MaxIterations = PerAttributeDetails.Num();

		for (int i = 0; i < MaxIterations; i++)
		{
			const FPCGExAttributeProxy Proxy = SortableAttributes[i];
			switch (Proxy.Type)
			{
			PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_ATT_MAP_PUSH)
			default: ;
			}
		}

		// -1 = return true early
		// 0 = move to next attribute
		// +1 = return false early

		if (SortDirection == ESortDirection::Ascending)
		{
			Points.Sort([PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_ATT_MAP_CAPTURE) &SortableAttributes, &PerAttributeDetails, &MaxIterations](const FPCGPoint& PtA, const FPCGPoint& PtB)
				{
					PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_MEMBER_ATT)
					COMPARE_LOOP_BODY(Asc)
					return false;
				}
			);
		}
		else
		{
			Points.Sort([PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_ATT_MAP_CAPTURE) &SortableAttributes, &PerAttributeDetails, &MaxIterations](const FPCGPoint& PtA, const FPCGPoint& PtB)
				{
					PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_MEMBER_ATT)
					COMPARE_LOOP_BODY(Dsc)
					return false;
				}
			);
		}

#pragma region Undef macros

#pragma endregion
	}
};

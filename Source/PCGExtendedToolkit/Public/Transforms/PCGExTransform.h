// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Data/PCGExAttributeHelpers.h"

#include "PCGExTransform.generated.h"

#define PCGEX_OUT_ATTRIBUTE(_NAME, _TYPE)\
bool bWrite##_NAME = false;\
FName OutName##_NAME = NAME_None;\
FPCGMetadataAttribute<_TYPE>* OutAttribute##_NAME = nullptr;

#define PCGEX_FORWARD_OUT_ATTRIBUTE(_NAME)\
Context->bWrite##_NAME = Settings->bWrite##_NAME;\
Context->OutName##_NAME = Settings->_NAME;

#define PCGEX_CHECK_OUT_ATTRIBUTE_NAME(_NAME)\
if(Context->bWrite##_NAME && !PCGEx::IsValidName(Context->OutName##_NAME))\
{ PCGE_LOG(Warning, GraphAndLog, LOCTEXT("InvalidName", "Invalid output attribute name " #_NAME ));\
Context->bWrite##_NAME = false; }

#define PCGEX_SET_OUT_ATTRIBUTE(_NAME, _KEY, _VALUE)\
if (Context->OutAttribute##_NAME) { Context->OutAttribute##_NAME->SetValue(_KEY, _VALUE); }

#define PCGEX_INIT_ATTRIBUTE_OUT(_NAME, _TYPE)\
Context->OutAttribute##_NAME = PCGEx::TryGetAttribute<_TYPE>(PointIO->Out, Context->OutName##_NAME, Context->bWrite##_NAME);
#define PCGEX_INIT_ATTRIBUTE_IN(_NAME, _TYPE)\
Context->OutAttribute##_NAME = PCGEx::TryGetAttribute<_TYPE>(PointIO->In, Context->OutName##_NAME, Context->bWrite##_NAME);

UENUM(BlueprintType)
enum class EPCGExSampleMethod : uint8
{
	WithinRange UMETA(DisplayName = "Within Range", ToolTip="Use RangeMax = 0 to include all targets"),
	ClosestTarget UMETA(DisplayName = "Closest Target", ToolTip="TBD"),
	FarthestTarget UMETA(DisplayName = "Farthest Target", ToolTip="TBD"),
	TargetsExtents UMETA(DisplayName = "Targets Extents", ToolTip="TBD"),
};

UENUM(BlueprintType)
enum class EPCGExWeightMethod : uint8
{
	FullRange UMETA(DisplayName = "Full Range", ToolTip="Weight is sampled using the normalized distance over the full min/max range."),
	EffectiveRange UMETA(DisplayName = "Effective Range", ToolTip="Weight is sampled using the normalized distance over the min/max of sampled points."),
};
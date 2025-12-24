// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "Details/PCGExSettingsMacros.h"
#include "Math/PCGExMathAxis.h"

#include "PCGExShapesCommon.h"
#include "Data/PCGExDataCommon.h"
#include "Fitting/PCGExFitting.h"

#include "PCGExShapeConfigBase.generated.h"

USTRUCT(BlueprintType)
struct PCGEXELEMENTSSHAPES_API FPCGExShapeConfigBase
{
	GENERATED_BODY()

	FPCGExShapeConfigBase()
	{
	}

	explicit FPCGExShapeConfigBase(const bool InThreeDimensions)
		: bThreeDimensions(InThreeDimensions)
	{
	}

	virtual ~FPCGExShapeConfigBase()
	{
	}

	UPROPERTY(meta = (PCG_NotOverridable))
	bool bThreeDimensions = false;

	/** Resolution mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Resolution", meta = (PCG_Overridable))
	EPCGExResolutionMode ResolutionMode = EPCGExResolutionMode::Fixed;

	/** Resolution input type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Resolution", meta = (PCG_NotOverridable))
	EPCGExInputValueType ResolutionInput = EPCGExInputValueType::Constant;

	/** Resolution Attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Resolution", meta=(PCG_Overridable, DisplayName="Resolution (Attr)", EditCondition="ResolutionInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector ResolutionAttribute;

	/** Resolution Constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Resolution", meta=(PCG_Overridable, DisplayName="Resolution", EditCondition="ResolutionInput == EPCGExInputValueType::Constant && !bThreeDimensions", EditConditionHides, ClampMin=0))
	double ResolutionConstant = 10;

	/** Resolution Constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Resolution", meta=(PCG_Overridable, DisplayName="Resolution (Vector)", EditCondition="ResolutionInput == EPCGExInputValueType::Constant && bThreeDimensions", EditConditionHides, ClampMin=0))
	FVector ResolutionConstantVector = FVector(10);

	PCGEX_SETTING_VALUE_DECL(Resolution, double);
	PCGEX_SETTING_VALUE_DECL(ResolutionVector, FVector);

	/** Fitting details */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExFittingDetailsHandler Fitting;

	/** Axis on the source to remap to a target axis on the shape */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Align", meta=(PCG_Overridable))
	EPCGExAxisAlign SourceAxis = EPCGExAxisAlign::Forward;

	/** Shape axis to align to the source axis */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Align", meta=(PCG_Overridable))
	EPCGExAxisAlign TargetAxis = EPCGExAxisAlign::Forward;

	/** Points look at */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Align", meta=(PCG_Overridable))
	EPCGExShapePointLookAt PointsLookAt = EPCGExShapePointLookAt::None;

	/** Axis used to align the look at rotation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Align", meta=(PCG_Overridable))
	EPCGExAxisAlign LookAtAxis = EPCGExAxisAlign::Forward;


	/** Default point extnets */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data", meta=(PCG_Overridable))
	FVector DefaultExtents = FVector::OneVector * 50;

	/** Shape ID used to identify this specific shape' points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data", meta=(PCG_Overridable))
	int32 ShapeId = 0;


	/** Don't output shape if they have less points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Pruning", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveBelow = true;

	/** Discarded if point count is less than */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Pruning", meta=(PCG_Overridable, EditCondition="bRemoveBelow", ClampMin=0))
	int32 MinPointCount = 2;

	/** Don't output shape if they have more points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Pruning", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveAbove = false;

	/** Discarded if point count is more than */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Pruning", meta=(PCG_Overridable, EditCondition="bRemoveAbove", ClampMin=0))
	int32 MaxPointCount = 500;


	FTransform LocalTransform = FTransform::Identity;

	virtual void Init();
};

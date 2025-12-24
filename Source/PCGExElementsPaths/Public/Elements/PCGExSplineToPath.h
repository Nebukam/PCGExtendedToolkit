// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExPathProcessor.h"
#include "Data/Utils/PCGExDataFilterDetails.h"
#include "Data/PCGSplineStruct.h"
#include "Details/PCGExFilterDetails.h"
#include "Filters/Points/PCGExPolyPathFilterFactory.h"
#include "Fitting/PCGExFitting.h"
#include "Sampling/PCGExSamplingCommon.h"

#include "PCGExSplineToPath.generated.h"

#define PCGEX_FOREACH_FIELD_SPLINETOPATH(MACRO)\
MACRO(ArriveTangent, FVector, FVector::ZeroVector)\
MACRO(LeaveTangent, FVector, FVector::ZeroVector)\
MACRO(LengthAtPoint, double, 0)\
MACRO(PointType, int32, 0)\
MACRO(Alpha, double, 0)

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/spline-to-path"))
class UPCGExSplineToPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SplineToPath, "Spline to Path", "Turns splines to paths.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(Path); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointProcessorSettings
public:
	//~End UPCGExPointProcessorSettings

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

	/** Point transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExLeanTransformDetails TransformDetails;

	/** Sample inputs.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExSplineSamplingIncludeMode SampleInputs = EPCGExSplineSamplingIncludeMode::All;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteArriveTangent = true;

	/** Name of the 'FVector' attribute to write Arrive tangent to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="Arrive Tangent", PCG_Overridable, EditCondition="bWriteArriveTangent"))
	FName ArriveTangentAttributeName = FName("ArriveTangent");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteLeaveTangent = true;

	/** Name of the 'FVector' attribute to write Leave tangent to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="Leave Tangent", PCG_Overridable, EditCondition="bWriteLeaveTangent"))
	FName LeaveTangentAttributeName = FName("LeaveTangent");

	/** Tag handling */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExTagsToDataAction TagsToData = EPCGExTagsToDataAction::ToData;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteLengthAtPoint = false;

	/** Name of the 'double' attribute to write the length at point to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="Length at Point", PCG_Overridable, EditCondition="bWriteLengthAtPoint"))
	FName LengthAtPointAttributeName = FName("LengthAtPoint");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteAlpha = false;

	/** Name of the 'double' attribute to write the length at point to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="Alpha", PCG_Overridable, EditCondition="bWriteAlpha"))
	FName AlphaAttributeName = FName("Alpha");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWritePointType = false;

	/** Name of the 'int32' attribute that store the point type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(DisplayName="Point Type", PCG_Overridable, EditCondition="bWritePointType"))
	FName PointTypeAttributeName = FName("PointType");

	/** Tags to be forwarded from source splines */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable))
	FPCGExNameFiltersDetails TagForwarding;

	/** Meta filter settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails CarryOverDetails;
};

struct FPCGExSplineToPathContext final : FPCGExPathProcessorContext
{
	friend class FPCGExSplineToPathElement;

	PCGEX_FOREACH_FIELD_SPLINETOPATH(PCGEX_OUTPUT_DECL_TOGGLE)

	FPCGExNameFiltersDetails TagForwarding;
	FPCGExCarryOverDetails CarryOverDetails;

	TArray<const UPCGSplineData*> Targets;
	TArray<TArray<FString>> Tags;
	TArray<FPCGSplineStruct> Splines;

	int64 NumTargets = 0;
};

class FPCGExSplineToPathElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(SplineToPath)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExSplineToPath
{
	const FName SourceSplineLabel = TEXT("Splines");
}

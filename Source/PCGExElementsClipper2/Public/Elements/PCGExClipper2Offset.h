// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExClipper2Processor.h"
#include "Details/PCGExInputShorthandsDetails.h"
#include "Paths/PCGExPath.h"

#include "PCGExClipper2Offset.generated.h"

namespace PCGExClipper2
{
	class FPolyline;
}

UENUM(BlueprintType)
enum class EPCGExClipper2OffsetType : uint8
{
	Offset  = 0 UMETA(DisplayName = "Offset", SearchHints = "Offset"),
	Inflate = 1 UMETA(DisplayName = "Inflate", SearchHints = "Inflate")
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/clipper2/clipper2-offset"))
class UPCGExClipper2OffsetSettings : public UPCGExClipper2ProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(Clipper2Offset, "Clipper2 : Offset", "Does a Clipper2 offset operation with optional dual (inward+outward) offset.", FName(GetDisplayName()));
	virtual TArray<FPCGPreConfiguredSettingsInfo> GetPreconfiguredInfo() const override;
#endif

	virtual void ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo) override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExClipper2OffsetType OffsetType = EPCGExClipper2OffsetType::Offset;

	/** If enabled, generates both positive and negative offsets */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="OffsetType == EPCGExClipper2OffsetType::Offset"))
	bool bDualOffset = false;

	/** Offset amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExInputShorthandSelectorDouble Offset = FPCGExInputShorthandSelectorDouble(FName("Offset"), 10, false);

	/** Offset Scale (mostly useful when using attributes) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Scale"))
	double OffsetScale = 1.0;

	/** Number of iterations to apply */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExInputShorthandNameInteger32Abs Iterations = FPCGExInputShorthandNameInteger32Abs(FName("@Data.Iterations"), 1, false);

	/** Join type for corners */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExClipper2JoinType JoinType = EPCGExClipper2JoinType::Round;

	/** End type for open paths */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExClipper2EndType EndType = EPCGExClipper2EndType::Round;

	/** Miter limit (only used with Miter join type) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=1.0))
	double MiterLimit = 2.0;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bWriteIteration = false;

	/** Write the iteration index to a data attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bWriteIteration"))
	FString IterationAttributeName = TEXT("Iteration");


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIteration = false;

	/** Write the iteration index to a tag */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIteration"))
	FString IterationTag = TEXT("OffsetNum");


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagDual = false;

	/** Write this tag on the dual (negative) offsets */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagDual"))
	FString DualTag = TEXT("Dual");

	virtual FPCGExGeo2DProjectionDetails GetProjectionDetails() const override;
	virtual bool SupportOpenMainPaths() const override;

#if WITH_EDITOR
	FString GetDisplayName() const;
#endif
};

struct FPCGExClipper2OffsetContext final : FPCGExClipper2ProcessorContext
{
	friend class FPCGExClipper2OffsetElement;

	TArray<TSharedPtr<PCGExDetails::TSettingValue<double>>> OffsetValues;
	TArray<TSharedPtr<PCGExDetails::TSettingValue<int32>>> IterationValues;

	virtual void Process(const TSharedPtr<PCGExClipper2::FProcessingGroup>& Group) override;
};

class FPCGExClipper2OffsetElement final : public FPCGExClipper2ProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(Clipper2Offset)

	virtual bool PostBoot(FPCGExContext* InContext) const override;
};

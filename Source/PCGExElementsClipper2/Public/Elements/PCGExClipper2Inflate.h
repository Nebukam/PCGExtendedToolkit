// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExClipper2Processor.h"
#include "Details/PCGExInputShorthandsDetails.h"
#include "Paths/PCGExPath.h"

#include "PCGExClipper2Inflate.generated.h"

namespace PCGExClipper2
{
	class FPolyline;
}


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/clipper2/clipper2-inflate"))
class UPCGExClipper2InflateSettings : public UPCGExClipper2ProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Clipper2Inflate, "Clipper2 : Inflate", "Does a Clipper2 inflate/deflate operation.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails;

	/** Offset amount. Positive values inflate, negative values deflate. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExInputShorthandNameDouble Offset = FPCGExInputShorthandNameDouble(FName("@Data.Offset"), 10, false);

	/** Number of iterations to apply */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExInputShorthandNameInteger32Abs Iterations = FPCGExInputShorthandNameInteger32Abs(FName("@Data.Iterations"), 1, false);

	/** If enabled, performs a union of all paths in the group before inflating */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bUnionBeforeInflate = false;

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

	virtual FPCGExGeo2DProjectionDetails GetProjectionDetails() const override;
};

struct FPCGExClipper2InflateContext final : FPCGExClipper2ProcessorContext
{
	friend class FPCGExClipper2InflateElement;

	TArray<TSharedPtr<PCGExDetails::TSettingValue<double>>> OffsetValues;
	TArray<TSharedPtr<PCGExDetails::TSettingValue<int32>>> IterationValues;

	virtual void Process(const TSharedPtr<PCGExClipper2::FProcessingGroup>& Group) override;
};

class FPCGExClipper2InflateElement final : public FPCGExClipper2ProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(Clipper2Inflate)

	virtual bool PostBoot(FPCGExContext* InContext) const override;
};

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Factories/PCGExFactories.h"
#include "Core/PCGExClipper2Processor.h"
#include "Details/PCGExInputShorthandsDetails.h"
#include "Paths/PCGExPath.h"

#include "PCGExClipper2Offset.generated.h"

namespace PCGExClipper2
{
	class FPolyline;
}

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
	PCGEX_NODE_INFOS(PathOffset, "Clipper2 : Offset", "Does a Clipper2 offset operation.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	//PCGEX_NODE_POINT_FILTER(PCGExFilters::Labels::SourceFiltersLabel, "Filters which points will be offset", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExInputShorthandNameBoolean DualOffset = FPCGExInputShorthandNameBoolean(FName("@Data.DualOffset"), true, false);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExInputShorthandNameDouble Offset = FPCGExInputShorthandNameDouble(FName("@Data.Offset"), 10, false);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExInputShorthandNameInteger32Abs Iterations = FPCGExInputShorthandNameInteger32Abs(FName("@Data.Iterations"), 1, false);

	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bWriteIteration = false;

	/** Write the iteration index to a data attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bWriteIteration"))
	FString IterationAttributeName = TEXT("@Data.Iteration");


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIteration = false;

	/** Write the iteration index to a tag */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIteration"))
	FString IterationTag = TEXT("OffsetNum");


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagDual = false;

	/** Write this tag on the dual offsets */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIteration"))
	FString DualTag = TEXT("Dual");

	virtual FPCGExGeo2DProjectionDetails GetProjectionDetails() const override;
};

struct FPCGExClipper2OffsetContext final : FPCGExClipper2ProcessorContext
{
	friend class FPCGExClipper2OffsetElement;

protected:
	//PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExClipper2OffsetElement final : public FPCGExClipper2ProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(Clipper2Offset)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
	
};
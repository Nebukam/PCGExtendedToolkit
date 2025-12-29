// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Factories/PCGExFactories.h"
#include "Core/PCGExCavalierProcessor.h"
#include "Details/PCGExCCDetails.h"
#include "Details/PCGExInputShorthandsDetails.h"
#include "Paths/PCGExPath.h"

#include "PCGExCavalierOffset.generated.h"

namespace PCGExCavalier
{
	class FPolyline;
}

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/cavalier-contours/cavalier-offset"))
class UPCGExCavalierOffsetSettings : public UPCGExCavalierProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathOffset, "Cavalier : Offset", "Applies a cavalier offset to paths.");
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

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExCCOffsetOptions OffsetOptions;

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

struct FPCGExCavalierOffsetContext final : FPCGExCavalierProcessorContext
{
	friend class FPCGExCavalierOffsetElement;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExCavalierOffsetElement final : public FPCGExCavalierProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(CavalierOffset)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
	
	virtual bool WantsRootPathsFromMainInput() const override;
};

namespace PCGExCavalierOffset
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExCavalierOffsetContext, UPCGExCavalierOffsetSettings>
	{
		double OffsetValue = 1;
		int32 NumIterations = 1;

		TMap<int32, PCGExCavalier::FRootPath> RootPathsMap;
		FPCGExGeo2DProjectionDetails ProjectionDetails;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		void ProcessOutput(const TSharedPtr<PCGExData::FPointIO>& IO, const int32 Iteration, const bool bDual) const;
	};
}

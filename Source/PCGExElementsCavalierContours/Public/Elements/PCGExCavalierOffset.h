// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Factories/PCGExFactories.h"
#include "Core/PCGExPathProcessor.h"
#include "Details/PCGExCCDetails.h"
#include "Details/PCGExInputShorthandsDetails.h"
#include "Details/PCGExSettingsMacros.h"
#include "Paths/PCGExPath.h"

#include "PCGExCavalierOffset.generated.h"

namespace PCGExCavalier
{
	class FPolyline;
}

namespace PCGExPaths
{
	class FPathEdgeHalfAngle;
	class FPath;
	struct FPathEdgeCrossings;
}

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/offset"))
class UPCGExCavalierOffsetSettings : public UPCGExPathProcessorSettings
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

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bTessellateArcs = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bTessellateArcs"))
	FPCGExCCArcTessellationSettings TessellationSettings;
	
	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bBlendTransforms = true;
	
	/** When enabled, add a very small floating point value to positions in order to mitigate arithmetic edge cases that can sometimes happen. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bAddFuzzinessToPositions = false;
	
};

struct FPCGExCavalierOffsetContext final : FPCGExPathProcessorContext
{
	friend class FPCGExCavalierOffsetElement;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExCavalierOffsetElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(CavalierOffset)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExCavalierOffset
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExCavalierOffsetContext, UPCGExCavalierOffsetSettings>
	{
		double OffsetValue = 1;
		int32 NumIterations = 1;
		
		TMap<int32, PCGExCavalier::FRootPath> RootPaths;
		FPCGExGeo2DProjectionDetails ProjectionDetails;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		void OutputPolyline(PCGExCavalier::FPolyline& Polyline);
	};
}

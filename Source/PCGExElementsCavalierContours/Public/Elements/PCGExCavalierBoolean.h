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

#include "PCGExCavalierBoolean.generated.h"

namespace PCGExCavalier
{
	class FPolyline;
}

/**
 * 
 */
UCLASS(Hidden, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/cavalier-contours/cavalier-boolean"))
class UPCGExCavalierBooleanSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathOffset, "Cavalier : Boolean", "Does a cavalier boolean operation on paths.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	
	//~Begin UPCGExPointsProcessorSettings
public:
	//PCGEX_NODE_POINT_FILTER(PCGExFilters::Labels::SourceFiltersLabel, "Filters which points will be offset", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings
	
	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails = FPCGExGeo2DProjectionDetails(false);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExCCBooleanOp Operation = EPCGExCCBooleanOp::Union;
	
	/** When enabled, add a very small floating point value to positions in order to mitigate arithmetic edge cases that can sometimes happen. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bAddFuzzinessToPositions = false;
	
};

struct FPCGExCavalierBooleanContext final : FPCGExPathProcessorContext
{
	friend class FPCGExCavalierBooleanElement;

	TMap<int32, PCGExCavalier::FRootPath> RootPaths;
	TArray<PCGExCavalier::FPolyline> RootPolylines;
	
protected:
	TArray<PCGExCavalier::FRootPath> TempRootPaths;
	//PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExCavalierBooleanElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(CavalierBoolean)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

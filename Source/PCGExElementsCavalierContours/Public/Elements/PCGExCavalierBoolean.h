// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Factories/PCGExFactories.h"
#include "Core/PCGExPathProcessor.h"
#include "Core/PCGExCCBoolean.h"
#include "Core/PCGExCCPolyline.h"
#include "Details/PCGExCCDetails.h"
#include "Details/PCGExMatchingDetails.h"
#include "Details/PCGExSettingsMacros.h"
#include "Paths/PCGExPath.h"

#include "PCGExCavalierBoolean.generated.h"

namespace PCGExCavalier
{
	class FPolyline;
}

namespace PCGExMatching
{
	class FDataMatcher;
}

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/cavalier-contours/cavalier-boolean"))
class UPCGExCavalierBooleanSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(CavalierBoolean, "Cavalier : Boolean", "Performs boolean operations on closed paths.");
	virtual TArray<FPCGPreConfiguredSettingsInfo> GetPreconfiguredInfo() const override;
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual void ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo) override;
	//~End UPCGExPointsProcessorSettings
	
	/** If enabled, allows you to filter out which targets get sampled by which data */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExMatchingDetails DataMatching = FPCGExMatchingDetails(EPCGExMatchingDetailsUsage::Default);

	/** Projection settings for 2D operations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails = FPCGExGeo2DProjectionDetails(false);

	/** The boolean operation to perform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExCCBooleanOp Operation = EPCGExCCBooleanOp::Union;

	/** Boolean operation options */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExContourBooleanOptions BooleanOptions;

	/** Tessellate arcs in results */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bTessellateArcs = true;

	/** Arc tessellation settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bTessellateArcs"))
	FPCGExCCArcTessellationSettings TessellationSettings;

	/** Blend transforms from source paths for output vertices */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bBlendTransforms = true;

	/** Add small random offset to mitigate degenerate geometry issues */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bAddFuzzinessToPositions = false;

	/** Skip paths that aren't closed (required for boolean ops) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bSkipOpenPaths = true;

	/** If enabled, output negative space (holes) as separate paths */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable))
	bool bOutputNegativeSpace = false;

	/** Tag to apply to negative space outputs */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable, EditCondition="bOutputNegativeSpace"))
	FString NegativeSpaceTag = TEXT("Hole");
	
	bool NeedsOperands() const;
};

struct FPCGExCavalierBooleanContext final : FPCGExPathProcessorContext
{
	friend class FPCGExCavalierBooleanElement;

	// Source data for 3D reconstruction
	TMap<int32, PCGExCavalier::FRootPath> RootPaths;

	// Polylines built from main input
	TArray<PCGExCavalier::FPolyline> MainPolylines;

	// Polylines built from operands input
	TArray<PCGExCavalier::FPolyline> OperandPolylines;

	// Maps polyline index to its source FPointIO
	TArray<TSharedPtr<PCGExData::FPointIO>> MainPolylinesSourceIO;
	TArray<TSharedPtr<PCGExData::FPointIO>> OperandPolylinesSourceIO;

	// Operands input collection
	TSharedPtr<PCGExData::FPointIOCollection> OperandsCollection;

	// Data matcher for paired mode
	TSharedPtr<PCGExMatching::FDataMatcher> DataMatcher;

	FPCGExGeo2DProjectionDetails ProjectionDetails;

	int32 NextPathId = 0;
	FCriticalSection PathIdLock;

	int32 AllocatePathId()
	{
		FScopeLock Lock(&PathIdLock);
		return NextPathId++;
	}
};

class FPCGExCavalierBooleanElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(CavalierBoolean)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;

private:
	// Build polylines from an input collection
	void BuildPolylinesFromCollection(
		FPCGExCavalierBooleanContext* Context,
		const UPCGExCavalierBooleanSettings* Settings,
		PCGExData::FPointIOCollection* Collection,
		TArray<PCGExCavalier::FPolyline>& OutPolylines,
		TArray<TSharedPtr<PCGExData::FPointIO>>& OutSourceIOs) const;

	// Execute boolean operation in CombineAll mode
	TArray<PCGExCavalier::FPolyline> ExecuteCombineAll(
		FPCGExCavalierBooleanContext* Context,
		const UPCGExCavalierBooleanSettings* Settings) const;

	// Execute boolean operation in Matched mode
	TArray<PCGExCavalier::FPolyline> ExecuteMatched(
		FPCGExCavalierBooleanContext* Context,
		const UPCGExCavalierBooleanSettings* Settings) const;

	// Helper for multi-polyline operations
	PCGExCavalier::BooleanOps::FBooleanResult PerformMultiBoolean(
		const TArray<PCGExCavalier::FPolyline>& Polylines,
		EPCGExCCBooleanOp Operation,
		const FPCGExContourBooleanOptions& Options) const;

	// Output a result polyline
	void OutputPolyline(
		FPCGExCavalierBooleanContext* Context,
		const UPCGExCavalierBooleanSettings* Settings,
		PCGExCavalier::FPolyline& Polyline,
		bool bIsNegativeSpace) const;

	// Find source IO for a polyline based on contributing paths
	TSharedPtr<PCGExData::FPointIO> FindSourceIO(
		FPCGExCavalierBooleanContext* Context,
		const PCGExCavalier::FPolyline& Polyline) const;
};

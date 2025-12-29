// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Factories/PCGExFactories.h"
#include "Core/PCGExCavalierProcessor.h"
#include "Core/PCGExCCBoolean.h"
#include "Core/PCGExCCPolyline.h"
#include "Details/PCGExCCDetails.h"
#include "Details/PCGExMatchingDetails.h"
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
class UPCGExCavalierBooleanSettings : public UPCGExCavalierProcessorSettings
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

	virtual bool NeedsOperands() const override;
	virtual FPCGExGeo2DProjectionDetails GetProjectionDetails() const override;
};

struct FPCGExCavalierBooleanContext final : FPCGExCavalierProcessorContext
{
	friend class FPCGExCavalierBooleanElement;

	// Data matcher for paired mode
	TSharedPtr<PCGExMatching::FDataMatcher> DataMatcher;

};

class FPCGExCavalierBooleanElement final : public FPCGExCavalierProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(CavalierBoolean)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;

private:
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

};

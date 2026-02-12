// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExSettings.h"
#include "Core/PCGExElement.h"
#include "Core/PCGExContext.h"
#include "Core/PCGExValencyCommon.h"
#include "Core/PCGExValencyBondingRules.h"
#include "Core/PCGExValencySocketRules.h"
#include "Core/PCGExValencyPropertyWriter.h"
#include "Growth/PCGExValencyGenerativeCommon.h"
#include "Growth/PCGExValencyGrowthOperation.h"
#include "Fitting/PCGExFitting.h"

#include "PCGExValencyGenerative.generated.h"

class UPCGExValencyGrowthFactory;

namespace PCGExCollections
{
	class FPickPacker;
}

/**
 * Valency Generative - Grow structures from seed points using socket connections.
 * Seeds resolve to modules, modules expose sockets, sockets spawn new modules.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Valency", meta=(Keywords = "valency generative growth grow seed socket", PCGExNodeLibraryDoc="valency/valency-generative"))
class PCGEXELEMENTSVALENCY_API UPCGExValencyGenerativeSettings : public UPCGExSettings
{
	GENERATED_BODY()

public:
	//~Begin UObject
	virtual void PostInitProperties() override;
	//~End UObject

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ValencyGenerative, "Valency : Generative", "Grow structures from seed points using socket-based module connections.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(MiscAdd); }
	virtual bool CanDynamicallyTrackKeys() const override { return true; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** The bonding rules data asset (required) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExValencyBondingRules> BondingRules;

	/** Socket rules defining socket types and compatibility (required) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExValencySocketRules> SocketRules;

	/** Growth strategy algorithm */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExValencyGrowthFactory> GrowthStrategy;

	/** Growth budget controlling expansion limits */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExGrowthBudget Budget;

	/** Global bounds padding in world units (cm). Positive = gap between modules. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Bounds", meta=(PCG_Overridable))
	float BoundsInflation = 0.0f;

	/** If enabled, applies module's local transform offset to output points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bApplyLocalTransforms = true;

	/** If enabled, output the resolved module name as an attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bOutputModuleName = false;

	/** Attribute name for the resolved module name */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bOutputModuleName"))
	FName ModuleNameAttributeName = FName("ModuleName");

	/** If enabled, output tree depth as an attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bOutputDepth = true;

	/** Attribute name for growth depth */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bOutputDepth"))
	FName DepthAttributeName = FName("Depth");

	/** If enabled, output seed index as an attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bOutputSeedIndex = false;

	/** Attribute name for seed index */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bOutputSeedIndex"))
	FName SeedIndexAttributeName = FName("SeedIndex");

	/** If enabled, output seeds that couldn't be resolved to modules */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bOutputUnsolvableSeeds = false;

	/** Attribute on seed points for module name filtering (empty = no filtering) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Seed Filtering", meta=(PCG_Overridable))
	FName SeedModuleNameAttribute;

	/** Attribute on seed points for tag-based filtering (empty = no filtering) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Seed Filtering", meta=(PCG_Overridable))
	FName SeedTagAttribute;

	/** Properties output configuration */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	FPCGExValencyPropertyOutputSettings PropertiesOutput;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta=(PCG_Overridable))
	FPCGExScaleToFitDetails ScaleToFit = FPCGExScaleToFitDetails(EPCGExFitMode::None);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta=(PCG_Overridable))
	FPCGExJustificationDetails Justification = FPCGExJustificationDetails(false);
};

struct PCGEXELEMENTSVALENCY_API FPCGExValencyGenerativeContext final : FPCGExContext
{
	friend class FPCGExValencyGenerativeElement;

	virtual void RegisterAssetDependencies() override;

	/** Loaded bonding rules */
	TObjectPtr<UPCGExValencyBondingRules> BondingRules;

	/** Loaded socket rules */
	TObjectPtr<UPCGExValencySocketRules> SocketRules;

	/** Registered growth factory */
	UPCGExValencyGrowthFactory* GrowthFactory = nullptr;

	/** Pick packer for collection entry hash writing */
	TSharedPtr<PCGExCollections::FPickPacker> PickPacker;

	UPCGExMeshCollection* MeshCollection = nullptr;
	UPCGExActorCollection* ActorCollection = nullptr;
};

class PCGEXELEMENTSVALENCY_API FPCGExValencyGenerativeElement final : public IPCGExElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ValencyGenerative)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual void PostLoadAssetsDependencies(FPCGExContext* InContext) const override;
	virtual bool PostBoot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

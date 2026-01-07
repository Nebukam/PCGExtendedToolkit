// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExFillControlOperation.h"
#include "PCGExFillControlsFactoryProvider.h"

#include "PCGExFillControlVtxFilters.generated.h"

USTRUCT(BlueprintType)
struct FPCGExFillControlConfigVtxFilters : public FPCGExFillControlConfigBase
{
	GENERATED_BODY()

	FPCGExFillControlConfigVtxFilters()
		: FPCGExFillControlConfigBase()
	{
		bSupportSource = false;
	}
};

/**
 * 
 */
class FPCGExFillControlVtxFilters : public FPCGExFillControlOperation
{
	friend class UPCGExFillControlsFactoryVtxFilters;

public:
	virtual bool PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler) override;

	virtual bool IsValidCapture(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate) override;
	virtual bool IsValidProbe(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate) override;
	virtual bool IsValidCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, const PCGExFloodFill::FCandidate& Candidate) override;

protected:
	TSharedPtr<PCGExClusterFilter::FManager> VtxFilterManager;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExFillControlsFactoryVtxFilters : public UPCGExFillControlsFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExFillControlConfigVtxFilters Config;

	UPROPERTY()
	TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> FilterFactories;

	virtual TSharedPtr<FPCGExFillControlOperation> CreateOperation(FPCGExContext* InContext) const override;

	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
	virtual bool RegisterConsumableAttributes(FPCGExContext* InContext) const override;
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params", meta=(PCGExNodeLibraryDoc="flood-fill/fc-vtx-filters"))
class UPCGExFillControlsVtxFiltersProviderSettings : public UPCGExFillControlsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(FillControlsVtxFilters, "Fill Control : Vtx Filters", "Filter that check Vtxs.", FName(GetDisplayName()))
#endif
	//~End UPCGSettings

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

public:
	/** Control Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExFillControlConfigVtxFilters Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

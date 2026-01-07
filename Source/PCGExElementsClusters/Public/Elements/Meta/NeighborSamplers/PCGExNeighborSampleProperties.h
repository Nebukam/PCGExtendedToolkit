// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "Factories/PCGExFactoryProvider.h"
#include "Details/PCGExBlendingDetails.h"

#include "PCGExNeighborSampleFactoryProvider.h"


#include "PCGExNeighborSampleProperties.generated.h"

///


USTRUCT(BlueprintType, meta=(Hidden=true))
struct FPCGExPropertiesSamplerConfigBase
{
	GENERATED_BODY()

	FPCGExPropertiesSamplerConfigBase()
	{
	}

	/** Properties blending */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExPropertiesBlendingDetails Blending = FPCGExPropertiesBlendingDetails(EPCGExBlendingType::None);
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExNeighborSamplerFactoryProperties : public UPCGExNeighborSamplerFactoryData
{
	GENERATED_BODY()

public:
	FPCGExPropertiesSamplerConfigBase Config;
	virtual TSharedPtr<FPCGExNeighborSampleOperation> CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(Hidden, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|NeighborSample")
class UPCGExNeighborSamplePropertiesSettings : public UPCGExNeighborSampleProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(NeighborSamplerProperties, "Sampler : Vtx Properties", "Create a single neighbor attribute sampler, to be used by a Sample Neighbors node.", PCGEX_FACTORY_NAME_PRIORITY)

#endif
	//~End UPCGSettings

	//~Begin UPCGExNeighborSampleProviderSettings
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
	//~End UPCGExNeighborSampleProviderSettings

	/** Sampler Settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExPropertiesSamplerConfigBase Config;
};

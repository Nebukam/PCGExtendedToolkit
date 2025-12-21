// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "Factories/PCGExFactoryProvider.h"
#include "Details/PCGExBlendingDetails.h"

#include "PCGExNeighborSampleFactoryProvider.h"

#include "PCGExNeighborSampleAttribute.generated.h"

///

USTRUCT(BlueprintType, meta=(Hidden=true))
struct FPCGExAttributeSamplerConfigBase
{
	GENERATED_BODY()

	FPCGExAttributeSamplerConfigBase()
	{
	}

	/** Unique blendmode applied to all specified attributes. For different blendmodes, create multiple sampler nodes. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExBlendingType Blending = EPCGExBlendingType::Average;

	/** Attribute to sample & optionally remap. Leave it to None to overwrite the source attribute.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExAttributeSourceToTargetList SourceAttributes;
};

UCLASS(Hidden, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExNeighborSamplerFactoryAttribute : public UPCGExNeighborSamplerFactoryData
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExNeighborSampleOperation> CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(Hidden, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|NeighborSample")
class UPCGExNeighborSampleAttributeSettings : public UPCGExNeighborSampleProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(NeighborSamplerAttribute, "Sampler : Vtx Attributes", "Create a single neighbor attribute sampler, to be used by a Sample Neighbors node.", PCGEX_FACTORY_NAME_PRIORITY)

#endif
	//~End UPCGSettings

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Sampler Settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExAttributeSamplerConfigBase Config;
};

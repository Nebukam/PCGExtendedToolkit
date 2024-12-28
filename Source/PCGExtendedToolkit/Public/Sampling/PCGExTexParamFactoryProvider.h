// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "Graph/PCGExGraph.h"

#include "Graph/Filters/PCGExClusterFilter.h"

#include "PCGExTexParamFactoryProvider.generated.h"

class UPCGBaseTextureData;

namespace PCGExTexParam
{
	const FName SourceTexLabel = TEXT("TexParams");
	const FName OutputTexLabel = TEXT("TexParam");
	const FName OutputTextureDataLabel = TEXT("Texture Data");
	const FName OutputTexTagLabel = TEXT("TexTag");

	const FString TexTag_Str = TEXT("TEX:");
}

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTexParamConfig
{
	GENERATED_BODY()

	FPCGExTexParamConfig()
	{
	}

	/** Name of the texture parameter to look for */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName ParameterName = FName("TextureParameter");

	/** Name of the attribute to output the path to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName PathAttributeName = FName("AssetPath");

	/** Resolution input type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExInputValueType TextureIndexInput = EPCGExInputValueType::Constant;

	/** Texture Index Constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Texture Index", EditCondition="TextureIndexInput == EPCGExInputValueType::Constant", EditConditionHides))
	int32 TextureIndex = -1;

	/** Texture Index Attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Texture Index", EditCondition="TextureIndexInput == EPCGExInputValueType::Attribute", EditConditionHides))
	FName TextureIndexAttribute = FName("TextureIndex");

	/** Name of the attribute to output the sampled value to, when used with a sampler node. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName OutputSampleAttributeName = FName("SampledValue");
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTexParamFactoryBase : public UPCGExParamFactoryBase
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::TexParam; }

	FPCGExTexParamConfig Config;
	FHashedMaterialParameterInfo Infos;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Sampling")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTexParamProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(TexParamAttribute, "Texture Param", "A simple texture parameter definition.", Config.PathAttributeName)
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorConstant; }
#endif
	//~End UPCGSettings

	//~Begin UPCGExFactoryProviderSettings
public:
	virtual FName GetMainOutputPin() const override { return PCGExTexParam::OutputTexLabel; }
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const override;
	//~End UPCGExFactoryProviderSettings

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1, ShowOnlyInnerProperties))
	FPCGExTexParamConfig Config;
};

namespace PCGExTexParam
{
	struct /*PCGEXTENDEDTOOLKIT_API*/ FReference
	{
		FSoftObjectPath TexturePath = FSoftObjectPath();
		int32 TextureIndex = -1;

		FReference()
		{
		}

		explicit FReference(const FSoftObjectPath& InTexturePath, const int32 InTextureIndex = -1):
			TexturePath(InTexturePath), TextureIndex(InTextureIndex)
		{
		}

		FString GetTag() const;

		bool operator==(const FReference& Other) const { return TexturePath == Other.TexturePath && TextureIndex == Other.TextureIndex; }
		FORCEINLINE friend uint32 GetTypeHash(const FReference& Key) { return HashCombineFast(GetTypeHash(Key.TexturePath), Key.TextureIndex); }
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FLookup : public TSharedFromThis<FLookup>
	{
		TMap<FString, const UPCGBaseTextureData*> TextureDataMap;

	public:
		FLookup()
		{
		}

		TArray<TObjectPtr<const UPCGExTexParamFactoryBase>> Factories;

#if PCGEX_ENGINE_VERSION <= 503
		TArray<TSharedPtr<PCGExData::TBuffer<FString>>> Buffers;
#else
		TArray<TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>>> Buffers;
#endif

		bool BuildFrom(FPCGExContext* InContext, const FName InPin);
		bool BuildFrom(const TArray<TObjectPtr<const UPCGExTexParamFactoryBase>>& InFactories);
		void PrepareForWrite(FPCGExContext* InContext, TSharedRef<PCGExData::FFacade> InDataFacade);
		void PrepareForRead(FPCGExContext* InContext, TSharedRef<PCGExData::FFacade> InDataFacade);

		void ExtractParams(const int32 PointIndex, const UMaterialInterface* InMaterial) const;
		void ExtractReferences(const UMaterialInterface* InMaterial, TSet<FReference>& References) const;

		void BuildMapFrom(FPCGExContext* InContext, const FName InPin);
		const UPCGBaseTextureData* TryGetTextureData(const FString& InPath) const;
	};
}

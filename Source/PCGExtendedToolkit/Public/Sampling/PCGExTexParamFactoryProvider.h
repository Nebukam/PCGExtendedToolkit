// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "Graph/PCGExGraph.h"

#include "Graph/Filters/PCGExClusterFilter.h"

#include "PCGExTexParamFactoryProvider.generated.h"

class UPCGBaseTextureData;

UENUM()
enum class EPCGExTexSampleAttributeType : uint8
{
	Auto    = 0 UMETA(DisplayName = "Auto", ToolTip="Output type will be driven by selected channels."),
	Float   = 1 UMETA(Hidden, DisplayName = "Float", ToolTip="Output sample attribute type will be Float"),
	Double  = 2 UMETA(DisplayName = "Double", ToolTip="Output sample attribute type will be Double"),
	Integer = 3 UMETA(Hidden, DisplayName = "Double", ToolTip="Output sample attribute type will be Integer"),
	Vector4 = 4 UMETA(DisplayName = "Vector4", ToolTip="Output sample attribute type will be Vector4"),
	Vector  = 5 UMETA(DisplayName = "Vector", ToolTip="Output sample attribute type will be Vector"),
	Vector2 = 6 UMETA(DisplayName = "Vector2", ToolTip="Output sample attribute type will be Vector2"),
	Invalid = 10 UMETA(Hidden)
};

UENUM(meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] GUID Uniqueness Flags"))
enum class EPCGExTexChannelsFlags : uint8
{
	None = 0,
	R    = 1 << 0 UMETA(DisplayName = "R", ToolTip="Red Channel"),
	G    = 1 << 1 UMETA(DisplayName = "G", ToolTip="Green Channel"),
	B    = 1 << 2 UMETA(DisplayName = "B", ToolTip="Blue Channel"),
	A    = 1 << 3 UMETA(DisplayName = "A", ToolTip="Alpha Channel"),
	RGB  = R | G | B UMETA(DisplayName = "RGB", ToolTip="RGB Channels, omits alpha"),
	All  = R | G | B | A UMETA(DisplayName = "RGBA"),
};

ENUM_CLASS_FLAGS(EPCGExTexChannelsFlags)
using EPCGExTexChannelsFlagsBitmask = TEnumAsByte<EPCGExTexChannelsFlags>;

namespace PCGExTexture
{
	const FName SourceTexLabel = TEXT("TextureParams");
	const FName OutputTexLabel = TEXT("TextureParam");

	const FName SourceTextureDataLabel = TEXT("TextureData");
	const FName OutputTextureDataLabel = TEXT("TextureData");

	const FName OutputTexTagLabel = TEXT("TexTag");

	const FString TexTag_Str = TEXT("TEX:");
}

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTextureParamConfig
{
	GENERATED_BODY()

	FPCGExTextureParamConfig()
	{
	}

	/** Name of the texture parameter to look for, when used in node that are set up to require this info. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName MaterialParameterName = FName("TextureParameter");

	/** Name of the attribute to output the path to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName TextureIDAttributeName = FName("TextureId");

	/** Name of the attribute to output the sampled value to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	FName SampleAttributeName = FName("Sample");

	/** Type of the attribute to output the sampled value to */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	EPCGExTexSampleAttributeType OutputType = EPCGExTexSampleAttributeType::Auto;

	/** What components will be sampled. Note that output will be truncated or sparse depending on the selected output type.  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExTexChannelsFlags"))
	uint8 SampledChannels = static_cast<uint8>(EPCGExTexChannelsFlags::All);

	/** Apply a scale factor to the output value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Sampling", meta=(PCG_Overridable))
	double Scale = 1;


	/** Resolution input type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Texture Array", meta = (PCG_NotOverridable))
	EPCGExInputValueType TextureIndexInput = EPCGExInputValueType::Constant;

	/** Texture Index Attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Texture Array", meta=(PCG_Overridable, DisplayName="Texture Index (Attr)", EditCondition="TextureIndexInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName TextureIndexAttribute = FName("TextureIndex");

	/** Texture Index Constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Texture Array", meta=(PCG_Overridable, DisplayName="Texture Index", EditCondition="TextureIndexInput == EPCGExInputValueType::Constant", EditConditionHides))
	int32 TextureIndex = -1;

	TArray<int32> OutChannels;

	EPCGMetadataTypes MetadataType = EPCGMetadataTypes::Unknown;

	void Init();
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTexParamFactoryData : public UPCGExFactoryData
{
	GENERATED_BODY()

public:
	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::TexParam; }

	FPCGExTextureParamConfig Config;
	FHashedMaterialParameterInfo Infos;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Sampling")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExTexParamProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		TexParamAttribute, "Texture Param", "A simple texture parameter definition.",
		FName(Config.TextureIDAttributeName.ToString() +" / "+Config.SampleAttributeName.ToString()))
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorTex; }
#endif
	//~End UPCGSettings

	//~Begin UPCGExFactoryProviderSettings
	virtual FName GetMainOutputPin() const override { return PCGExTexture::OutputTexLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;
	//~End UPCGExFactoryProviderSettings

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1, ShowOnlyInnerProperties))
	FPCGExTextureParamConfig Config;
};

namespace PCGExTexture
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

		TArray<TObjectPtr<const UPCGExTexParamFactoryData>> Factories;

#if PCGEX_ENGINE_VERSION <= 503
		TArray<TSharedPtr<PCGExData::TBuffer<FString>>> Buffers;
#else
		TArray<TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>>> Buffers;
#endif

		bool BuildFrom(FPCGExContext* InContext, const FName InPin);
		bool BuildFrom(const TArray<TObjectPtr<const UPCGExTexParamFactoryData>>& InFactories);
		void PrepareForWrite(FPCGExContext* InContext, TSharedRef<PCGExData::FFacade> InDataFacade);

		void ExtractParams(const int32 PointIndex, const UMaterialInterface* InMaterial) const;
		void ExtractReferences(const UMaterialInterface* InMaterial, TSet<FReference>& References) const;
		void ExtractParamsAndReferences(const int32 PointIndex, const UMaterialInterface* InMaterial, TSet<FReference>& References) const;

		void BuildMapFrom(FPCGExContext* InContext, const FName InPin);
		const UPCGBaseTextureData* TryGetTextureData(const FString& InPath) const;
	};
}

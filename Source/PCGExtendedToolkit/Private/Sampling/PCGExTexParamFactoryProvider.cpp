// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExTexParamFactoryProvider.h"

#include "PCGPin.h"
#include "Data/PCGTextureData.h"


#define LOCTEXT_NAMESPACE "PCGExCreateTexParam"
#define PCGEX_NAMESPACE PCGExCreateTexParam

void FPCGExTextureParamConfig::Init()
{
	int32 NumChannels = 0;

	if ((SampledChannels & static_cast<uint8>(EPCGExTexChannelsFlags::R)) != 0)
	{
		OutChannels.Add(0);
		NumChannels++;
	}

	if ((SampledChannels & static_cast<uint8>(EPCGExTexChannelsFlags::G)) != 0)
	{
		OutChannels.Add(1);
		NumChannels++;
	}

	if ((SampledChannels & static_cast<uint8>(EPCGExTexChannelsFlags::B)) != 0)
	{
		OutChannels.Add(2);
		NumChannels++;
	}

	if ((SampledChannels & static_cast<uint8>(EPCGExTexChannelsFlags::A)) != 0)
	{
		OutChannels.Add(3);
		NumChannels++;
	}

	if (OutputType == EPCGExTexSampleAttributeType::Auto)
	{
		if (NumChannels == 0) { OutputType = EPCGExTexSampleAttributeType::Invalid; }
		else if (NumChannels == 1) { OutputType = EPCGExTexSampleAttributeType::Double; }
		else if (NumChannels == 2) { OutputType = EPCGExTexSampleAttributeType::Vector2; }
		else if (NumChannels == 3) { OutputType = EPCGExTexSampleAttributeType::Vector; }
		else { OutputType = EPCGExTexSampleAttributeType::Vector4; }
	}

	switch (OutputType)
	{
	default:
	case EPCGExTexSampleAttributeType::Invalid:
		OutChannels.Empty();
		break;
	case EPCGExTexSampleAttributeType::Vector4:
		MetadataType = EPCGMetadataTypes::Vector4;
		break;
	case EPCGExTexSampleAttributeType::Float:
		MetadataType = EPCGMetadataTypes::Float;
		if (OutChannels.Num() > 1) { OutChannels.SetNum(1); }
		break;
	case EPCGExTexSampleAttributeType::Double:
		MetadataType = EPCGMetadataTypes::Double;
		if (OutChannels.Num() > 1) { OutChannels.SetNum(1); }
		break;
	case EPCGExTexSampleAttributeType::Integer:
		MetadataType = EPCGMetadataTypes::Integer32;
		if (OutChannels.Num() > 1) { OutChannels.SetNum(1); }
		break;
	case EPCGExTexSampleAttributeType::Vector:
		MetadataType = EPCGMetadataTypes::Vector;
		if (OutChannels.Num() > 3) { OutChannels.SetNum(3); }
		break;
	case EPCGExTexSampleAttributeType::Vector2:
		MetadataType = EPCGMetadataTypes::Vector2;
		if (OutChannels.Num() > 2) { OutChannels.SetNum(2); }
		break;
	}
}

UPCGExParamFactoryBase* UPCGExTexParamProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExTexParamFactoryBase* TexFactory = InContext->ManagedObjects->New<UPCGExTexParamFactoryBase>();

	TexFactory->Config = Config;
	TexFactory->Config.Init();
	TexFactory->Infos = FHashedMaterialParameterInfo(Config.MaterialParameterName);

	return Super::CreateFactory(InContext, TexFactory);
}

namespace PCGExTexture
{
	FString FReference::GetTag() const
	{
		/*
		if (TextureIndex < 0) { return FString::Printf(TEXT("%ls%ls"), *PCGExTexture::TexTag_Str, *TexturePath.ToString()); }
		return FString::Printf(TEXT("%ls%ls:%d"), *PCGExTexture::TexTag_Str, *TexturePath.ToString(), TextureIndex);
		*/
		if (TextureIndex < 0) { return FString::Printf(TEXT("%ls"), *TexturePath.ToString()); }
		return FString::Printf(TEXT("%ls:%d"), *TexturePath.ToString(), TextureIndex);
	}

	bool FLookup::BuildFrom(FPCGExContext* InContext, const FName InPin)
	{
		if (!PCGExFactories::GetInputFactories(InContext, InPin, Factories, {PCGExFactories::EType::TexParam}, true)) { return false; }
		for (const TObjectPtr<const UPCGExTexParamFactoryBase>& Factory : Factories) { PCGEX_VALIDATE_NAME_C(InContext, Factory->Config.TextureIDAttributeName) }
		return true;
	}

	bool FLookup::BuildFrom(const TArray<TObjectPtr<const UPCGExTexParamFactoryBase>>& InFactories)
	{
		if (InFactories.IsEmpty()) { return false; }
		Factories.Reserve(InFactories.Num());
		for (const TObjectPtr<const UPCGExTexParamFactoryBase>& Factory : InFactories) { Factories.Add(Factory); }
		return true;
	}

	void FLookup::PrepareForWrite(FPCGExContext* InContext, TSharedRef<PCGExData::FFacade> InDataFacade)
	{
		Buffers.Reserve(Factories.Num());

		for (const TObjectPtr<const UPCGExTexParamFactoryBase>& Factory : Factories)
		{
#if PCGEX_ENGINE_VERSION <= 503
			TSharedPtr<PCGExData::TBuffer<FString>> Buffer = InDataFacade->GetWritable<FString>(Factory->Config.TextureIDAttributeName, TEXT(""), true, PCGExData::EBufferInit::Inherit);
#else
			TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> Buffer = InDataFacade->GetWritable<FSoftObjectPath>(Factory->Config.TextureIDAttributeName, FSoftObjectPath(), true, PCGExData::EBufferInit::Inherit);
#endif
			Buffers.Add(Buffer);
		}
	}

	void FLookup::ExtractParams(const int32 PointIndex, const UMaterialInterface* InMaterial) const
	{
		if (!InMaterial)
		{
			for (int i = 0; i < Factories.Num(); i++)
			{
				if (!Buffers[i]) { continue; }
				Buffers[i]->GetMutable(PointIndex) = TEXT("");
			}

			return;
		}

		for (int i = 0; i < Factories.Num(); i++)
		{
			if (!Buffers[i]) { continue; }

			const TObjectPtr<const UPCGExTexParamFactoryBase>& Factory = Factories[i];

			if (UTexture* FoundTexture = nullptr; InMaterial->GetTextureParameterValue(Factory->Infos, FoundTexture))
			{
				Buffers[i]->GetMutable(PointIndex) = FoundTexture->GetPathName();
			}
		}
	}

	void FLookup::ExtractReferences(const UMaterialInterface* InMaterial, TSet<FReference>& References) const
	{
		if (!InMaterial) { return; }

		for (int i = 0; i < Factories.Num(); i++)
		{
			const TObjectPtr<const UPCGExTexParamFactoryBase>& Factory = Factories[i];
			if (UTexture* FoundTexture = nullptr; InMaterial->GetTextureParameterValue(Factory->Infos, FoundTexture))
			{
				References.Add(FReference(FoundTexture->GetPathName(), Factory->Config.TextureIndex));
			}
		}
	}

	void FLookup::ExtractParamsAndReferences(const int32 PointIndex, const UMaterialInterface* InMaterial, TSet<FReference>& References) const
	{
		if (!InMaterial)
		{
			for (int i = 0; i < Factories.Num(); i++)
			{
				if (!Buffers[i]) { continue; }
				Buffers[i]->GetMutable(PointIndex) = TEXT("");
			}

			return;
		}

		for (int i = 0; i < Factories.Num(); i++)
		{
			if (!Buffers[i]) { continue; }

			const TObjectPtr<const UPCGExTexParamFactoryBase>& Factory = Factories[i];

			if (UTexture* FoundTexture = nullptr; InMaterial->GetTextureParameterValue(Factory->Infos, FoundTexture))
			{
				Buffers[i]->GetMutable(PointIndex) = FoundTexture->GetPathName();
				References.Add(FReference(FoundTexture->GetPathName(), Factory->Config.TextureIndex));
			}
		}
	}

	void FLookup::BuildMapFrom(FPCGExContext* InContext, const FName InPin)
	{
		// Process all input texture data
		// and store as a map. Take all data tags & texture path
		// This is a very blind approach, but also allows a degree of flexibility as to what can be used to re-associate attribute value with a texture data

		TArray<FPCGTaggedData> TaggedTexData = InContext->InputData.GetInputsByPin(InPin);
		for (const FPCGTaggedData& TaggedData : TaggedTexData)
		{
			const UPCGBaseTextureData* BaseTextureData = Cast<UPCGBaseTextureData>(TaggedData.Data);
			if (!BaseTextureData) { continue; }

			if (const UPCGTextureData* TextureData = Cast<UPCGTextureData>(BaseTextureData))
			{
				if (TextureData->Texture.IsValid(false, true))
				{
					// Use existing texture path as lookup, since we can.
					TextureDataMap.Add(TextureData->Texture->GetPathName(), BaseTextureData);
				}
			}

			// Resort to parsing tags
			for (const FString& Tag : TaggedData.Tags)
			{
				if (Tag.StartsWith(TexTag_Str))
				{
					FString Path = Tag.Mid(TexTag_Str.Len());
					TextureDataMap.Add(Path, BaseTextureData);
				}
				else
				{
					TextureDataMap.Add(Tag, BaseTextureData);
				}
			}
		}
	}

	const UPCGBaseTextureData* FLookup::TryGetTextureData(const FString& InPath) const
	{
		const UPCGBaseTextureData* const* Ptr = TextureDataMap.Find(InPath);
		return Ptr ? *Ptr : nullptr;
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

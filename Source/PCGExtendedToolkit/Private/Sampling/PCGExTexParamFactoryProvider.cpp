// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExTexParamFactoryProvider.h"

#include "PCGPin.h"
#include "Data/PCGTextureData.h"


#define LOCTEXT_NAMESPACE "PCGExCreateTexParam"
#define PCGEX_NAMESPACE PCGExCreateTexParam

UPCGExParamFactoryBase* UPCGExTexParamProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExTexParamFactoryBase* TexFactory = InContext->ManagedObjects->New<UPCGExTexParamFactoryBase>();

	TexFactory->Config = Config;
	TexFactory->Infos = FHashedMaterialParameterInfo(Config.ParameterName);

	return Super::CreateFactory(InContext, TexFactory);
}

namespace PCGExTexParam
{
	FString FReference::GetTag() const
	{
		if (TextureIndex < 0) { return FString::Printf(TEXT("%ls%ls"), *PCGExTexParam::TexTag_Str, *TexturePath.ToString()); }
		return FString::Printf(TEXT("%ls%ls:%d"), *PCGExTexParam::TexTag_Str, *TexturePath.ToString(), TextureIndex);
	}

	bool FLookup::BuildFrom(FPCGExContext* InContext, const FName InPin)
	{
		if (!PCGExFactories::GetInputFactories(InContext, InPin, Factories, {PCGExFactories::EType::TexParam}, true)) { return false; }
		if (Factories.IsEmpty()) { return false; }
		for (const TObjectPtr<const UPCGExTexParamFactoryBase>& Factory : Factories) { PCGEX_VALIDATE_NAME_C(InContext, Factory->Config.PathAttributeName) }
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
			TSharedPtr<PCGExData::TBuffer<FString>> Buffer = InDataFacade->GetWritable<FString>(Factory->Config.PathAttributeName, TEXT(""), true, PCGExData::EBufferInit::Inherit);
#else
			TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> Buffer = InDataFacade->GetWritable<FSoftObjectPath>(Factory->Config.PathAttributeName, FSoftObjectPath(), true, PCGExData::EBufferInit::Inherit);
#endif
			Buffers.Add(Buffer);
		}
	}

	void FLookup::PrepareForRead(FPCGExContext* InContext, TSharedRef<PCGExData::FFacade> InDataFacade)
	{
		Buffers.Reserve(Factories.Num());

		for (const TObjectPtr<const UPCGExTexParamFactoryBase>& Factory : Factories)
		{
#if PCGEX_ENGINE_VERSION <= 503
			TSharedPtr<PCGExData::TBuffer<FString>> Buffer = InDataFacade->GetReadable<FString>(Factory->Config.PathAttributeName);
#else
			TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> Buffer = InDataFacade->GetReadable<FSoftObjectPath>(Factory->Config.PathAttributeName);
#endif

			if (!Buffer)
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Input missing texture attribute: \"{0}\"."), FText::FromName(Factory->Config.PathAttributeName)));
			}

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
			// Don't overwrite existing value if any. Last valid value takes precedence, buffer is initialized with an empty path anyway
			//else { Buffers[i]->GetMutable(PointIndex) = TEXT(""); }
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

	void FLookup::BuildMapFrom(FPCGExContext* InContext, const FName InPin)
	{
		// Process all input texture data
		// and store as a map

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
					continue;
				}
			}

			// Resort to parsing tags
			for (const FString& Tag : TaggedData.Tags)
			{
				if (Tag.StartsWith(TexTag_Str))
				{
					FString Path = Tag.Mid(TexTag_Str.Len());
					TextureDataMap.Add(Path, BaseTextureData);
					break;
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

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExTexParamFactoryProvider.h"

#include "Materials/MaterialInterface.h"
#include "TextureResource.h"
#include "Containers/PCGExManagedObjects.h"
#include "Data/PCGExData.h"

#define LOCTEXT_NAMESPACE "PCGExCreateTexParam"
#define PCGEX_NAMESPACE PCGExCreateTexParam

PCG_DEFINE_TYPE_INFO(FPCGExDataTypeInfoTexParam, UPCGExTexParamFactoryData)

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
	default: case EPCGExTexSampleAttributeType::Invalid: OutChannels.Empty();
		break;
	case EPCGExTexSampleAttributeType::Vector4: MetadataType = EPCGMetadataTypes::Vector4;
		break;
	case EPCGExTexSampleAttributeType::Float: MetadataType = EPCGMetadataTypes::Float;
		if (OutChannels.Num() > 1) { OutChannels.SetNum(1); }
		break;
	case EPCGExTexSampleAttributeType::Double: MetadataType = EPCGMetadataTypes::Double;
		if (OutChannels.Num() > 1) { OutChannels.SetNum(1); }
		break;
	case EPCGExTexSampleAttributeType::Integer: MetadataType = EPCGMetadataTypes::Integer32;
		if (OutChannels.Num() > 1) { OutChannels.SetNum(1); }
		break;
	case EPCGExTexSampleAttributeType::Vector: MetadataType = EPCGMetadataTypes::Vector;
		if (OutChannels.Num() > 3) { OutChannels.SetNum(3); }
		break;
	case EPCGExTexSampleAttributeType::Vector2: MetadataType = EPCGMetadataTypes::Vector2;
		if (OutChannels.Num() > 2) { OutChannels.SetNum(2); }
		break;
	}
}

UPCGExFactoryData* UPCGExTexParamProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExTexParamFactoryData* TexFactory = InContext->ManagedObjects->New<UPCGExTexParamFactoryData>();

	TexFactory->Config = Config;
	TexFactory->Config.Init();
	TexFactory->Infos = FHashedMaterialParameterInfo(Config.MaterialParameterName);

	return Super::CreateFactory(InContext, TexFactory);
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

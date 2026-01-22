// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExValencyPropertyWriter.h"
#include "Core/PCGExCagePropertyCompiled.h"
#include "Core/PCGExCagePropertyCompiledTypes.h"
#include "Core/PCGExValencyLog.h"

FPCGExValencyPropertyWriter::FPCGExValencyPropertyWriter(const FPCGExValencyPropertyWriterConfig& InConfig)
	: Config(InConfig)
{
}

bool FPCGExValencyPropertyWriter::Initialize(
	const FPCGExValencyBondingRulesCompiled* InCompiledRules,
	const TSharedRef<PCGExData::FFacade>& OutputFacade)
{
	if (!InCompiledRules)
	{
		return false;
	}

	CompiledRules = InCompiledRules;

	// Create metadata writers by scanning all modules for unique metadata keys
	if (Config.bOutputMetadata)
	{
		TSet<FName> UniqueMetadataKeys;

		for (int32 ModuleIndex = 0; ModuleIndex < CompiledRules->ModuleCount; ++ModuleIndex)
		{
			TConstArrayView<FInstancedStruct> Properties = CompiledRules->GetModuleProperties(ModuleIndex);
			for (const FInstancedStruct& Prop : Properties)
			{
				if (const FPCGExCagePropertyCompiled_Metadata* MetaProp = Prop.GetPtr<FPCGExCagePropertyCompiled_Metadata>())
				{
					for (const auto& KV : MetaProp->Metadata)
					{
						UniqueMetadataKeys.Add(KV.Key);
					}
				}
			}
		}

		// Create a writer for each unique metadata key
		for (const FName& Key : UniqueMetadataKeys)
		{
			TSharedPtr<PCGExData::TBuffer<FString>> Writer = OutputFacade->GetWritable<FString>(
				Key, FString(), true, PCGExData::EBufferInit::Inherit);
			MetadataWriters.Add(Key, Writer);
			PCGEX_VALENCY_VERBOSE(Staging, "Created metadata writer for key '%s'", *Key.ToString());
		}

		PCGEX_VALENCY_INFO(Staging, "Created %d metadata property writers", MetadataWriters.Num());
	}

	// Create tags writer
	if (Config.bOutputTags)
	{
		TagsWriter = OutputFacade->GetWritable<FString>(
			Config.TagsAttributeName, FString(), true, PCGExData::EBufferInit::Inherit);
		PCGEX_VALENCY_VERBOSE(Staging, "Created tags writer '%s'", *Config.TagsAttributeName.ToString());
	}

	return true;
}

void FPCGExValencyPropertyWriter::WriteModuleProperties(int32 PointIndex, int32 ModuleIndex)
{
	if (!CompiledRules || ModuleIndex < 0)
	{
		return;
	}

	// Write metadata properties
	if (MetadataWriters.Num() > 0)
	{
		TConstArrayView<FInstancedStruct> ModuleProperties = CompiledRules->GetModuleProperties(ModuleIndex);

		// Collect all metadata from this module's properties
		TMap<FName, FString> AllMetadata;
		for (const FInstancedStruct& Prop : ModuleProperties)
		{
			if (const FPCGExCagePropertyCompiled_Metadata* MetaProp = Prop.GetPtr<FPCGExCagePropertyCompiled_Metadata>())
			{
				for (const auto& KV : MetaProp->Metadata)
				{
					AllMetadata.Add(KV.Key, KV.Value);
				}
			}
		}

		// Write to each metadata writer
		for (const auto& WriterKV : MetadataWriters)
		{
			if (const FString* Value = AllMetadata.Find(WriterKV.Key))
			{
				WriterKV.Value->SetValue(PointIndex, *Value);
			}
			// Note: if key not found, keeps default empty string
		}
	}

	// Write module tags as comma-separated string
	if (TagsWriter)
	{
		const TArray<FName>& Tags = CompiledRules->ModuleTags[ModuleIndex];
		if (Tags.Num() > 0)
		{
			FString TagString;
			for (int32 TagIdx = 0; TagIdx < Tags.Num(); ++TagIdx)
			{
				if (TagIdx > 0) { TagString += TEXT(","); }
				TagString += Tags[TagIdx].ToString();
			}
			TagsWriter->SetValue(PointIndex, TagString);
		}
	}
}

bool FPCGExValencyPropertyWriter::HasOutputs() const
{
	return MetadataWriters.Num() > 0 || TagsWriter.IsValid();
}

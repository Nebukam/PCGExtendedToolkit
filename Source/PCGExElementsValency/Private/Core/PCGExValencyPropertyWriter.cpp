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
	const TSharedRef<PCGExData::FFacade>& OutputFacade,
	const TArray<FPCGExValencyPropertyOutputConfig>& OutputConfigs)
{
	if (!InCompiledRules)
	{
		return false;
	}

	CompiledRules = InCompiledRules;

	// Initialize property writers from configs
	for (const FPCGExValencyPropertyOutputConfig& OutputConfig : OutputConfigs)
	{
		if (!OutputConfig.IsValid())
		{
			PCGEX_VALENCY_VERBOSE(Staging, "Skipping invalid output config for property '%s'", *OutputConfig.PropertyName.ToString());
			continue;
		}

		FName OutputName = OutputConfig.GetEffectiveOutputName();

		// Find prototype property from any module
		const FInstancedStruct* Prototype = FindPrototypeProperty(OutputConfig.PropertyName);
		if (!Prototype)
		{
			PCGEX_VALENCY_VERBOSE(Staging, "Property '%s' not found in bonding rules", *OutputConfig.PropertyName.ToString());
			continue;
		}

		// Check if property supports output
		const FPCGExCagePropertyCompiled* ProtoBase = Prototype->GetPtr<FPCGExCagePropertyCompiled>();
		if (!ProtoBase || !ProtoBase->SupportsOutput())
		{
			PCGEX_VALENCY_VERBOSE(Staging, "Property '%s' does not support output", *OutputConfig.PropertyName.ToString());
			continue;
		}

		// Clone as writer instance
		FInstancedStruct WriterInstance = *Prototype;

		// Initialize output buffers
		FPCGExCagePropertyCompiled* Writer = WriterInstance.GetMutablePtr<FPCGExCagePropertyCompiled>();
		if (!Writer || !Writer->InitializeOutput(OutputFacade, OutputName))
		{
			PCGEX_VALENCY_VERBOSE(Staging, "Failed to initialize output for property '%s'", *OutputConfig.PropertyName.ToString());
			continue;
		}

		WriterInstances.Add(OutputConfig.PropertyName, MoveTemp(WriterInstance));
		PCGEX_VALENCY_VERBOSE(Staging, "Initialized property output '%s' -> attribute '%s'",
			*OutputConfig.PropertyName.ToString(), *OutputName.ToString());
	}

	// Create tags writer if configured
	if (Config.bOutputTags)
	{
		TagsWriter = OutputFacade->GetWritable<FString>(
			Config.TagsAttributeName, FString(), true, PCGExData::EBufferInit::Inherit);
		PCGEX_VALENCY_VERBOSE(Staging, "Created tags writer '%s'", *Config.TagsAttributeName.ToString());
	}

	PCGEX_VALENCY_INFO(Staging, "Initialized %d property outputs", WriterInstances.Num());

	return HasOutputs();
}

bool FPCGExValencyPropertyWriter::Initialize(
	const FPCGExValencyBondingRulesCompiled* InCompiledRules,
	const TSharedRef<PCGExData::FFacade>& OutputFacade)
{
	// Legacy initialize - tags only, no property outputs
	TArray<FPCGExValencyPropertyOutputConfig> EmptyConfigs;
	return Initialize(InCompiledRules, OutputFacade, EmptyConfigs);
}

void FPCGExValencyPropertyWriter::WriteModuleProperties(int32 PointIndex, int32 ModuleIndex)
{
	if (!CompiledRules || ModuleIndex < 0)
	{
		return;
	}

	// Write properties using property-owned output
	if (WriterInstances.Num() > 0)
	{
		TConstArrayView<FInstancedStruct> ModuleProperties = CompiledRules->GetModuleProperties(ModuleIndex);

		for (auto& KV : WriterInstances)
		{
			const FName& PropName = KV.Key;
			FPCGExCagePropertyCompiled* Writer = KV.Value.GetMutablePtr<FPCGExCagePropertyCompiled>();
			if (!Writer) { continue; }

			// Find actual property value for this module
			if (const FInstancedStruct* SourceProp = PCGExValency::GetPropertyByName(ModuleProperties, PropName))
			{
				if (const FPCGExCagePropertyCompiled* Source = SourceProp->GetPtr<FPCGExCagePropertyCompiled>())
				{
					Writer->CopyValueFrom(Source);
				}
			}

			Writer->WriteOutput(PointIndex);
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
	return WriterInstances.Num() > 0 || TagsWriter.IsValid();
}

const FInstancedStruct* FPCGExValencyPropertyWriter::FindPrototypeProperty(FName PropertyName) const
{
	if (!CompiledRules || PropertyName.IsNone())
	{
		return nullptr;
	}

	// Search through all modules for a property with matching name
	for (int32 ModuleIndex = 0; ModuleIndex < CompiledRules->ModuleCount; ++ModuleIndex)
	{
		TConstArrayView<FInstancedStruct> Properties = CompiledRules->GetModuleProperties(ModuleIndex);
		if (const FInstancedStruct* Found = PCGExValency::GetPropertyByName(Properties, PropertyName))
		{
			return Found;
		}
	}

	return nullptr;
}

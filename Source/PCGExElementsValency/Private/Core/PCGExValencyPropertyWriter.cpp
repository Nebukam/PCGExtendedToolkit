// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExValencyPropertyWriter.h"
#include "Core/PCGExCagePropertyCompiled.h"
#include "Core/PCGExCagePropertyCompiledTypes.h"
#include "Core/PCGExValencyLog.h"

int32 FPCGExValencyPropertyOutputSettings::AutoPopulateFromRules(const FPCGExValencyBondingRulesCompiled* CompiledRules)
{
	if (!CompiledRules)
	{
		return 0;
	}

	// Use the pre-built module property registry
	if (CompiledRules->ModulePropertyRegistry.Num() == 0)
	{
		return 0;
	}

	// Collect existing enabled property names
	TSet<FName> ExistingNames;
	for (const FPCGExValencyPropertyOutputConfig& Config : Configs)
	{
		if (Config.bEnabled && !Config.PropertyName.IsNone())
		{
			ExistingNames.Add(Config.PropertyName);
		}
	}

	// Add new configs for each registry entry that supports output and isn't already configured
	int32 AddedCount = 0;
	for (const FPCGExPropertyRegistryEntry& Entry : CompiledRules->ModulePropertyRegistry)
	{
		if (Entry.bSupportsOutput && !ExistingNames.Contains(Entry.PropertyName))
		{
			FPCGExValencyPropertyOutputConfig& NewConfig = Configs.AddDefaulted_GetRef();
			NewConfig.bEnabled = true;
			NewConfig.PropertyName = Entry.PropertyName;
			// OutputAttributeName left empty - will use PropertyName as default
			AddedCount++;
		}
	}

	return AddedCount;
}

bool FPCGExValencyPropertyWriter::Initialize(
	const FPCGExValencyBondingRulesCompiled* InCompiledRules,
	const TSharedRef<PCGExData::FFacade>& OutputFacade,
	const FPCGExValencyPropertyOutputSettings& OutputSettings)
{
	if (!InCompiledRules)
	{
		return false;
	}

	CompiledRules = InCompiledRules;
	Settings = OutputSettings;

	// Initialize property writers from configs
	for (const FPCGExValencyPropertyOutputConfig& OutputConfig : Settings.Configs)
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
	if (Settings.bOutputModuleTags)
	{
		TagsWriter = OutputFacade->GetWritable<FString>(
			Settings.ModuleTagsAttributeName, FString(), true, PCGExData::EBufferInit::Inherit);
		PCGEX_VALENCY_VERBOSE(Staging, "Created tags writer '%s'", *Settings.ModuleTagsAttributeName.ToString());
	}

	PCGEX_VALENCY_INFO(Staging, "Initialized %d property outputs", WriterInstances.Num());

	return HasOutputs();
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
		const TArray<FName>& Tags = CompiledRules->ModuleTags[ModuleIndex].Tags;
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

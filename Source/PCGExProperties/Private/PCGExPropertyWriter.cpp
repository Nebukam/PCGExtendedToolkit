// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPropertyWriter.h"
#include "PCGExProperty.h"
#include "Helpers/PCGExMetaHelpers.h"

FName FPCGExPropertyOutputConfig::GetEffectiveOutputName() const
{
	FName Name = OutputAttributeName.IsNone() ? PropertyName : OutputAttributeName;
	if (Name.IsNone() || !PCGExMetaHelpers::IsWritableAttributeName(Name))
	{
		return NAME_None;
	}
	return Name;
}

// Initialize creates a writer instance for each configured property output.
// For each output config:
//   1. Find the prototype property from the provider (by PropertyName)
//   2. Deep-copy it as a "writer instance" that will own the output buffer
//   3. Call InitializeOutput() on the clone to create the buffer on the facade
//   4. Store the clone in WriterInstances keyed by PropertyName
//
// After this, WriteProperties() can be called per-point to write values.
bool FPCGExPropertyWriter::Initialize(
	const IPCGExPropertyProvider* InProvider,
	const TSharedRef<PCGExData::FFacade>& OutputFacade,
	const FPCGExPropertyOutputSettings& OutputSettings)
{
	if (!InProvider)
	{
		return false;
	}

	Provider = InProvider;
	Settings = OutputSettings;

	// Initialize property writers from configs
	for (const FPCGExPropertyOutputConfig& OutputConfig : Settings.Configs)
	{
		if (!OutputConfig.IsValid())
		{
			continue;
		}

		FName OutputName = OutputConfig.GetEffectiveOutputName();

		// Find prototype property from provider
		const FInstancedStruct* Prototype = Provider->FindPrototypeProperty(OutputConfig.PropertyName);
		if (!Prototype)
		{
			continue;
		}

		// Check if property supports output
		const FPCGExProperty* ProtoBase = Prototype->GetPtr<FPCGExProperty>();
		if (!ProtoBase || !ProtoBase->SupportsOutput())
		{
			continue;
		}

		// Clone as writer instance
		FInstancedStruct WriterInstance = *Prototype;

		// Initialize output buffers
		FPCGExProperty* Writer = WriterInstance.GetMutablePtr<FPCGExProperty>();
		if (!Writer || !Writer->InitializeOutput(OutputFacade, OutputName))
		{
			continue;
		}

		WriterInstances.Add(OutputConfig.PropertyName, MoveTemp(WriterInstance));
	}

	return HasOutputs();
}

// WriteProperties copies values from the provider's source properties into the
// writer instances, then writes those values to the output buffers.
//
// WARNING: This uses CopyValueFrom() which mutates the writer instance's Value field.
// This is NOT safe for parallel processing. If you need parallel writes,
// use the property's WriteOutputFrom() method directly, which reads from
// source and writes to buffer without mutating any shared state.
void FPCGExPropertyWriter::WriteProperties(int32 PointIndex, int32 SourceIndex)
{
	if (!Provider || SourceIndex < 0)
	{
		return;
	}

	// Write properties using property-owned output
	if (WriterInstances.Num() > 0)
	{
		// Get the source property array for this index (e.g., collection entry, row)
		TConstArrayView<FInstancedStruct> SourceProperties = Provider->GetProperties(SourceIndex);

		for (auto& KV : WriterInstances)
		{
			const FName& PropName = KV.Key;
			FPCGExProperty* Writer = KV.Value.GetMutablePtr<FPCGExProperty>();
			if (!Writer) { continue; }

			// Find the source property by name and copy its value into the writer
			if (const FInstancedStruct* SourceProp = PCGExProperties::GetPropertyByName(SourceProperties, PropName))
			{
				if (const FPCGExProperty* Source = SourceProp->GetPtr<FPCGExProperty>())
				{
					Writer->CopyValueFrom(Source);
				}
			}

			// Write the (possibly updated) value to the output buffer
			Writer->WriteOutput(PointIndex);
		}
	}
}

bool FPCGExPropertyWriter::HasOutputs() const
{
	return WriterInstances.Num() > 0;
}

// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExPropertyWriter.h"
#include "PCGExPropertyCompiled.h"
#include "PCGExPropertyTypes.h"

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
		const FPCGExPropertyCompiled* ProtoBase = Prototype->GetPtr<FPCGExPropertyCompiled>();
		if (!ProtoBase || !ProtoBase->SupportsOutput())
		{
			continue;
		}

		// Clone as writer instance
		FInstancedStruct WriterInstance = *Prototype;

		// Initialize output buffers
		FPCGExPropertyCompiled* Writer = WriterInstance.GetMutablePtr<FPCGExPropertyCompiled>();
		if (!Writer || !Writer->InitializeOutput(OutputFacade, OutputName))
		{
			continue;
		}

		WriterInstances.Add(OutputConfig.PropertyName, MoveTemp(WriterInstance));
	}

	return HasOutputs();
}

void FPCGExPropertyWriter::WriteProperties(int32 PointIndex, int32 SourceIndex)
{
	if (!Provider || SourceIndex < 0)
	{
		return;
	}

	// Write properties using property-owned output
	if (WriterInstances.Num() > 0)
	{
		TConstArrayView<FInstancedStruct> SourceProperties = Provider->GetProperties(SourceIndex);

		for (auto& KV : WriterInstances)
		{
			const FName& PropName = KV.Key;
			FPCGExPropertyCompiled* Writer = KV.Value.GetMutablePtr<FPCGExPropertyCompiled>();
			if (!Writer) { continue; }

			// Find actual property value for this source
			if (const FInstancedStruct* SourceProp = PCGExProperties::GetPropertyByName(SourceProperties, PropName))
			{
				if (const FPCGExPropertyCompiled* Source = SourceProp->GetPtr<FPCGExPropertyCompiled>())
				{
					Writer->CopyValueFrom(Source);
				}
			}

			Writer->WriteOutput(PointIndex);
		}
	}
}

bool FPCGExPropertyWriter::HasOutputs() const
{
	return WriterInstances.Num() > 0;
}

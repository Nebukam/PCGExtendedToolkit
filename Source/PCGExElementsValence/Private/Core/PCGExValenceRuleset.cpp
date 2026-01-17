// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExValenceRuleset.h"

void UPCGExValenceRulesetCompiled::BuildCandidateLookup()
{
	MaskToCandidates.Empty();

	// For each unique socket mask combination, collect candidate modules
	// This is most useful for single-layer rulesets
	if (Layers.Num() == 1)
	{
		for (int32 ModuleIndex = 0; ModuleIndex < ModuleCount; ++ModuleIndex)
		{
			const int64 Mask = ModuleSocketMasks[ModuleIndex];
			TArray<int32>& Candidates = MaskToCandidates.FindOrAdd(Mask);
			Candidates.Add(ModuleIndex);
		}
	}
	// For multi-layer, we could build more complex lookup but it's less beneficial
	// Runtime will iterate modules and check masks directly
}

bool UPCGExValenceRuleset::Compile()
{
	// Validate layers
	for (FPCGExValenceSocketRegistry& Layer : Layers)
	{
		if (!Layer.Compile())
		{
			UE_LOG(LogTemp, Error, TEXT("Valence Ruleset: Layer '%s' has more than 64 sockets"), *Layer.LayerName.ToString());
			return false;
		}
	}

	// Assign module indices
	for (int32 i = 0; i < Modules.Num(); ++i)
	{
		Modules[i].ModuleIndex = i;
	}

	// Create compiled data
	if (!CompiledData)
	{
		CompiledData = NewObject<UPCGExValenceRulesetCompiled>(this);
	}

	CompiledData->ModuleCount = Modules.Num();
	CompiledData->Layers.SetNum(Layers.Num());

	// Initialize parallel arrays
	CompiledData->ModuleWeights.SetNum(Modules.Num());
	CompiledData->ModuleMinSpawns.SetNum(Modules.Num());
	CompiledData->ModuleMaxSpawns.SetNum(Modules.Num());
	CompiledData->ModuleAssets.SetNum(Modules.Num());
	CompiledData->ModuleSocketMasks.SetNum(Modules.Num() * Layers.Num());

	// Populate module data
	for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
	{
		const FPCGExValenceModuleDefinition& Module = Modules[ModuleIndex];

		CompiledData->ModuleWeights[ModuleIndex] = Module.Weight;
		CompiledData->ModuleMinSpawns[ModuleIndex] = Module.MinSpawns;
		CompiledData->ModuleMaxSpawns[ModuleIndex] = Module.MaxSpawns;
		CompiledData->ModuleAssets[ModuleIndex] = Module.Asset;

		// Socket masks per layer
		for (int32 LayerIndex = 0; LayerIndex < Layers.Num(); ++LayerIndex)
		{
			const FName& LayerName = Layers[LayerIndex].LayerName;
			const int32 MaskIndex = ModuleIndex * Layers.Num() + LayerIndex;

			if (const FPCGExValenceModuleLayerConfig* LayerConfig = Module.Layers.Find(LayerName))
			{
				CompiledData->ModuleSocketMasks[MaskIndex] = LayerConfig->SocketMask;
			}
			else
			{
				CompiledData->ModuleSocketMasks[MaskIndex] = 0;
			}
		}
	}

	// Compile each layer's neighbor data
	for (int32 LayerIndex = 0; LayerIndex < Layers.Num(); ++LayerIndex)
	{
		const FPCGExValenceSocketRegistry& Layer = Layers[LayerIndex];
		FPCGExValenceLayerCompiled& CompiledLayer = CompiledData->Layers[LayerIndex];

		CompiledLayer.LayerName = Layer.LayerName;
		CompiledLayer.SocketCount = Layer.Num();

		// Prepare header array (ModuleCount * SocketCount entries)
		const int32 HeaderCount = Modules.Num() * Layer.Num();
		CompiledLayer.NeighborHeaders.SetNum(HeaderCount);
		CompiledLayer.AllNeighbors.Empty();

		// Flatten neighbor data
		for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
		{
			const FPCGExValenceModuleDefinition& Module = Modules[ModuleIndex];
			const FPCGExValenceModuleLayerConfig* LayerConfig = Module.Layers.Find(Layer.LayerName);

			for (int32 SocketIndex = 0; SocketIndex < Layer.Num(); ++SocketIndex)
			{
				const int32 HeaderIndex = ModuleIndex * Layer.Num() + SocketIndex;
				const FName& SocketName = Layer.Sockets[SocketIndex].SocketName;

				int32 NeighborStart = CompiledLayer.AllNeighbors.Num();
				int32 NeighborCount = 0;

				if (LayerConfig)
				{
					if (const FPCGExValenceNeighborIndices* Neighbors = LayerConfig->SocketNeighbors.Find(SocketName))
					{
						NeighborCount = Neighbors->Num();
						CompiledLayer.AllNeighbors.Append(Neighbors->Indices);
					}
				}

				CompiledLayer.NeighborHeaders[HeaderIndex] = FIntPoint(NeighborStart, NeighborCount);
			}
		}
	}

	// Build fast lookup
	CompiledData->BuildCandidateLookup();

	UE_LOG(LogTemp, Log, TEXT("Valence Ruleset compiled: %d modules, %d layers"), Modules.Num(), Layers.Num());
	return true;
}

#if WITH_EDITOR
void UPCGExValenceRuleset::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Auto-compile when properties change in editor
	// This could be made optional via a flag if compilation is expensive
	// Compile();
}
#endif

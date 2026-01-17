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
	// Load and validate socket collections
	LoadedSocketCollections.Empty();
	LoadedSocketCollections.SetNum(SocketCollections.Num());

	for (int32 i = 0; i < SocketCollections.Num(); ++i)
	{
		if (SocketCollections[i].IsNull())
		{
			UE_LOG(LogTemp, Error, TEXT("Valence Ruleset: Socket collection at index %d is null"), i);
			return false;
		}

		LoadedSocketCollections[i] = SocketCollections[i].LoadSynchronous();
		if (!LoadedSocketCollections[i])
		{
			UE_LOG(LogTemp, Error, TEXT("Valence Ruleset: Failed to load socket collection at index %d"), i);
			return false;
		}

		// Validate collection
		TArray<FText> ValidationErrors;
		if (!LoadedSocketCollections[i]->Validate(ValidationErrors))
		{
			for (const FText& Error : ValidationErrors)
			{
				UE_LOG(LogTemp, Error, TEXT("Valence Ruleset: %s"), *Error.ToString());
			}
			return false;
		}

		// Check for more than 64 sockets
		if (LoadedSocketCollections[i]->Num() > 64)
		{
			UE_LOG(LogTemp, Error, TEXT("Valence Ruleset: Layer '%s' has more than 64 sockets"), *LoadedSocketCollections[i]->LayerName.ToString());
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

	const int32 LayerCount = LoadedSocketCollections.Num();

	CompiledData->ModuleCount = Modules.Num();
	CompiledData->Layers.SetNum(LayerCount);

	// Initialize parallel arrays
	CompiledData->ModuleWeights.SetNum(Modules.Num());
	CompiledData->ModuleMinSpawns.SetNum(Modules.Num());
	CompiledData->ModuleMaxSpawns.SetNum(Modules.Num());
	CompiledData->ModuleAssets.SetNum(Modules.Num());
	CompiledData->ModuleSocketMasks.SetNum(Modules.Num() * LayerCount);

	// Populate module data
	for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
	{
		const FPCGExValenceModuleDefinition& Module = Modules[ModuleIndex];

		CompiledData->ModuleWeights[ModuleIndex] = Module.Weight;
		CompiledData->ModuleMinSpawns[ModuleIndex] = Module.MinSpawns;
		CompiledData->ModuleMaxSpawns[ModuleIndex] = Module.MaxSpawns;
		CompiledData->ModuleAssets[ModuleIndex] = Module.Asset;

		// Socket masks per layer
		for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
		{
			const FName& LayerName = LoadedSocketCollections[LayerIndex]->LayerName;
			const int32 MaskIndex = ModuleIndex * LayerCount + LayerIndex;

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
	for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
	{
		const UPCGExValenceSocketCollection* Collection = LoadedSocketCollections[LayerIndex];
		FPCGExValenceLayerCompiled& CompiledLayer = CompiledData->Layers[LayerIndex];

		CompiledLayer.LayerName = Collection->LayerName;
		CompiledLayer.SocketCount = Collection->Num();

		// Prepare header array (ModuleCount * SocketCount entries)
		const int32 HeaderCount = Modules.Num() * Collection->Num();
		CompiledLayer.NeighborHeaders.SetNum(HeaderCount);
		CompiledLayer.AllNeighbors.Empty();

		// Flatten neighbor data
		for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
		{
			const FPCGExValenceModuleDefinition& Module = Modules[ModuleIndex];
			const FPCGExValenceModuleLayerConfig* LayerConfig = Module.Layers.Find(Collection->LayerName);

			for (int32 SocketIndex = 0; SocketIndex < Collection->Num(); ++SocketIndex)
			{
				const int32 HeaderIndex = ModuleIndex * Collection->Num() + SocketIndex;
				const FName SocketName = Collection->Sockets[SocketIndex].GetSocketName();

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

	UE_LOG(LogTemp, Log, TEXT("Valence Ruleset compiled: %d modules, %d layers"), Modules.Num(), LayerCount);
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

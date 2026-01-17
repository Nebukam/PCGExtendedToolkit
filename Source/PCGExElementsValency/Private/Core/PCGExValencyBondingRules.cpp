// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExValencyBondingRules.h"

void UPCGExValencyBondingRulesCompiled::BuildCandidateLookup()
{
	MaskToCandidates.Empty();

	// For each unique orbital mask combination, collect candidate modules
	// This is most useful for single-layer bonding rules
	if (Layers.Num() == 1)
	{
		for (int32 ModuleIndex = 0; ModuleIndex < ModuleCount; ++ModuleIndex)
		{
			const int64 Mask = ModuleOrbitalMasks[ModuleIndex];
			TArray<int32>& Candidates = MaskToCandidates.FindOrAdd(Mask);
			Candidates.Add(ModuleIndex);
		}
	}
	// For multi-layer, we could build more complex lookup but it's less beneficial
	// Runtime will iterate modules and check masks directly
}

bool UPCGExValencyBondingRules::Compile()
{
	// Load and validate orbital sets
	LoadedOrbitalSets.Empty();
	LoadedOrbitalSets.SetNum(OrbitalSets.Num());

	for (int32 i = 0; i < OrbitalSets.Num(); ++i)
	{
		if (OrbitalSets[i].IsNull())
		{
			UE_LOG(LogTemp, Error, TEXT("Valency Bonding Rules: Orbital set at index %d is null"), i);
			return false;
		}

		// TODO : LoadBLocking, we're not in game thread!
		LoadedOrbitalSets[i] = OrbitalSets[i].LoadSynchronous();
		if (!LoadedOrbitalSets[i])
		{
			UE_LOG(LogTemp, Error, TEXT("Valency Bonding Rules: Failed to load orbital set at index %d"), i);
			return false;
		}

		// Validate orbital set
		TArray<FText> ValidationErrors;
		if (!LoadedOrbitalSets[i]->Validate(ValidationErrors))
		{
			for (const FText& Error : ValidationErrors)
			{
				UE_LOG(LogTemp, Error, TEXT("Valency Bonding Rules: %s"), *Error.ToString());
			}
			return false;
		}

		// Check for more than 64 orbitals
		if (LoadedOrbitalSets[i]->Num() > 64)
		{
			UE_LOG(LogTemp, Error, TEXT("Valency Bonding Rules: Layer '%s' has more than 64 orbitals"), *LoadedOrbitalSets[i]->LayerName.ToString());
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
		CompiledData = NewObject<UPCGExValencyBondingRulesCompiled>(this);
	}

	const int32 LayerCount = LoadedOrbitalSets.Num();

	CompiledData->ModuleCount = Modules.Num();
	CompiledData->Layers.SetNum(LayerCount);

	// Initialize parallel arrays
	CompiledData->ModuleWeights.SetNum(Modules.Num());
	CompiledData->ModuleMinSpawns.SetNum(Modules.Num());
	CompiledData->ModuleMaxSpawns.SetNum(Modules.Num());
	CompiledData->ModuleAssets.SetNum(Modules.Num());
	CompiledData->ModuleOrbitalMasks.SetNum(Modules.Num() * LayerCount);

	// Populate module data
	for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
	{
		const FPCGExValencyModuleDefinition& Module = Modules[ModuleIndex];

		CompiledData->ModuleWeights[ModuleIndex] = Module.Weight;
		CompiledData->ModuleMinSpawns[ModuleIndex] = Module.MinSpawns;
		CompiledData->ModuleMaxSpawns[ModuleIndex] = Module.MaxSpawns;
		CompiledData->ModuleAssets[ModuleIndex] = Module.Asset;

		// Orbital masks per layer
		for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
		{
			const FName& LayerName = LoadedOrbitalSets[LayerIndex]->LayerName;
			const int32 MaskIndex = ModuleIndex * LayerCount + LayerIndex;

			if (const FPCGExValencyModuleLayerConfig* LayerConfig = Module.Layers.Find(LayerName))
			{
				CompiledData->ModuleOrbitalMasks[MaskIndex] = LayerConfig->OrbitalMask;
			}
			else
			{
				CompiledData->ModuleOrbitalMasks[MaskIndex] = 0;
			}
		}
	}

	// Compile each layer's neighbor data
	for (int32 LayerIndex = 0; LayerIndex < LayerCount; ++LayerIndex)
	{
		const UPCGExValencyOrbitalSet* OrbitalSet = LoadedOrbitalSets[LayerIndex];
		FPCGExValencyLayerCompiled& CompiledLayer = CompiledData->Layers[LayerIndex];

		CompiledLayer.LayerName = OrbitalSet->LayerName;
		CompiledLayer.OrbitalCount = OrbitalSet->Num();

		// Prepare header array (ModuleCount * OrbitalCount entries)
		const int32 HeaderCount = Modules.Num() * OrbitalSet->Num();
		CompiledLayer.NeighborHeaders.SetNum(HeaderCount);
		CompiledLayer.AllNeighbors.Empty();

		// Flatten neighbor data
		for (int32 ModuleIndex = 0; ModuleIndex < Modules.Num(); ++ModuleIndex)
		{
			const FPCGExValencyModuleDefinition& Module = Modules[ModuleIndex];
			const FPCGExValencyModuleLayerConfig* LayerConfig = Module.Layers.Find(OrbitalSet->LayerName);

			for (int32 OrbitalIndex = 0; OrbitalIndex < OrbitalSet->Num(); ++OrbitalIndex)
			{
				const int32 HeaderIndex = ModuleIndex * OrbitalSet->Num() + OrbitalIndex;
				const FName OrbitalName = OrbitalSet->Orbitals[OrbitalIndex].GetOrbitalName();

				int32 NeighborStart = CompiledLayer.AllNeighbors.Num();
				int32 NeighborCount = 0;

				if (LayerConfig)
				{
					if (const FPCGExValencyNeighborIndices* Neighbors = LayerConfig->OrbitalNeighbors.Find(OrbitalName))
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

	UE_LOG(LogTemp, Log, TEXT("Valency Bonding Rules compiled: %d modules, %d layers"), Modules.Num(), LayerCount);
	return true;
}

#if WITH_EDITOR
void UPCGExValencyBondingRules::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Auto-compile when properties change in editor
	// This could be made optional via a flag if compilation is expensive
	// Compile();
}
#endif

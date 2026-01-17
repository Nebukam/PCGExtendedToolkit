// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Builder/PCGExValencyBondingRulesBuilder.h"

#include "Cages/PCGExValencyCage.h"
#include "Cages/PCGExValencyCageNull.h"
#include "Volumes/ValencyContextVolume.h"

#define LOCTEXT_NAMESPACE "PCGExValencyBuilder"

FPCGExValencyBuildResult UPCGExValencyBondingRulesBuilder::BuildFromVolume(AValencyContextVolume* Volume)
{
	FPCGExValencyBuildResult Result;

	if (!Volume)
	{
		Result.Errors.Add(LOCTEXT("NoVolume", "No volume provided."));
		return Result;
	}

	UPCGExValencyBondingRules* TargetRules = Volume->GetBondingRules();
	if (!TargetRules)
	{
		Result.Errors.Add(LOCTEXT("NoBondingRules", "Volume has no BondingRules asset assigned."));
		return Result;
	}

	UPCGExValencyOrbitalSet* OrbitalSet = Volume->GetEffectiveOrbitalSet();
	if (!OrbitalSet)
	{
		Result.Errors.Add(LOCTEXT("NoOrbitalSet", "Volume has no OrbitalSet (check BondingRules or override)."));
		return Result;
	}

	// Collect cages from volume
	TArray<APCGExValencyCageBase*> AllCages;
	Volume->CollectContainedCages(AllCages);

	// Filter to regular cages (not null cages)
	TArray<APCGExValencyCage*> RegularCages;
	for (APCGExValencyCageBase* CageBase : AllCages)
	{
		if (APCGExValencyCage* Cage = Cast<APCGExValencyCage>(CageBase))
		{
			RegularCages.Add(Cage);
		}
	}

	return BuildFromCages(RegularCages, TargetRules, OrbitalSet);
}

FPCGExValencyBuildResult UPCGExValencyBondingRulesBuilder::BuildFromCages(
	const TArray<APCGExValencyCage*>& Cages,
	UPCGExValencyBondingRules* TargetRules,
	UPCGExValencyOrbitalSet* OrbitalSet)
{
	FPCGExValencyBuildResult Result;

	if (!TargetRules)
	{
		Result.Errors.Add(LOCTEXT("NoTargetRules", "No target BondingRules asset provided."));
		return Result;
	}

	if (!OrbitalSet)
	{
		Result.Errors.Add(LOCTEXT("NoOrbitalSetBuild", "No OrbitalSet provided."));
		return Result;
	}

	if (Cages.Num() == 0)
	{
		Result.Warnings.Add(LOCTEXT("NoCages", "No cages to process."));
		Result.bSuccess = true;
		return Result;
	}

	// Clear existing if requested
	if (bClearExistingModules)
	{
		TargetRules->Modules.Empty();
	}

	// Ensure orbital set is in the bonding rules
	if (!TargetRules->OrbitalSets.Contains(OrbitalSet))
	{
		TargetRules->OrbitalSets.Add(OrbitalSet);
	}

	// Step 1: Collect and preprocess cage data
	TArray<FPCGExValencyCageData> CageData;
	CollectCageData(Cages, OrbitalSet, CageData, Result);

	if (CageData.Num() == 0)
	{
		Result.Warnings.Add(LOCTEXT("NoValidCages", "No cages with registered assets found."));
		Result.bSuccess = true;
		return Result;
	}

	// Step 2: Build asset-to-module mapping
	TMap<FSoftObjectPath, int32> AssetToModule;
	BuildAssetToModuleMap(CageData, TargetRules, AssetToModule, Result);

	// Step 3: Build neighbor relationships
	BuildNeighborRelationships(CageData, AssetToModule, TargetRules, OrbitalSet, Result);

	// Step 4: Validate if requested
	if (bValidateCompleteness)
	{
		ValidateRules(TargetRules, OrbitalSet, Result);
	}

	// Compile the rules
	if (!TargetRules->Compile())
	{
		Result.Errors.Add(LOCTEXT("CompileFailed", "Failed to compile BondingRules after building."));
		return Result;
	}

	// Mark asset as modified
	TargetRules->Modify();

	Result.bSuccess = Result.Errors.Num() == 0;
	Result.CageCount = CageData.Num();
	Result.ModuleCount = TargetRules->Modules.Num();

	return Result;
}

void UPCGExValencyBondingRulesBuilder::CollectCageData(
	const TArray<APCGExValencyCage*>& Cages,
	const UPCGExValencyOrbitalSet* OrbitalSet,
	TArray<FPCGExValencyCageData>& OutCageData,
	FPCGExValencyBuildResult& OutResult)
{
	OutCageData.Empty();
	OutCageData.Reserve(Cages.Num());

	for (APCGExValencyCage* Cage : Cages)
	{
		if (!Cage)
		{
			continue;
		}

		// Get effective assets (resolving mirrors)
		TArray<TSoftObjectPtr<UObject>> Assets = GetEffectiveAssets(Cage);
		if (Assets.Num() == 0)
		{
			// Skip cages with no assets
			continue;
		}

		FPCGExValencyCageData Data;
		Data.Cage = Cage;
		Data.Assets = MoveTemp(Assets);

		// Compute orbital mask from connections
		const TArray<FPCGExValencyCageOrbital>& Orbitals = Cage->GetOrbitals();
		for (const FPCGExValencyCageOrbital& Orbital : Orbitals)
		{
			if (!Orbital.bEnabled)
			{
				continue;
			}

			// Only count connected orbitals (or null cage connections if enabled)
			if (Orbital.ConnectedCage.IsValid())
			{
				const APCGExValencyCageBase* ConnectedCage = Orbital.ConnectedCage.Get();

				// Check if it's a null cage (boundary)
				if (ConnectedCage->IsNullCage())
				{
					if (bIncludeNullBoundaries)
					{
						// Still set the orbital bit - this is a valid "null" connection
						Data.OrbitalMask |= (1LL << Orbital.OrbitalIndex);
					}
				}
				else
				{
					// Regular connection
					Data.OrbitalMask |= (1LL << Orbital.OrbitalIndex);
				}
			}
		}

		OutCageData.Add(MoveTemp(Data));
	}
}

void UPCGExValencyBondingRulesBuilder::BuildAssetToModuleMap(
	const TArray<FPCGExValencyCageData>& CageData,
	UPCGExValencyBondingRules* TargetRules,
	TMap<FSoftObjectPath, int32>& OutAssetToModule,
	FPCGExValencyBuildResult& OutResult)
{
	OutAssetToModule.Empty();

	// First, index existing modules
	for (int32 i = 0; i < TargetRules->Modules.Num(); ++i)
	{
		const FPCGExValencyModuleDefinition& Module = TargetRules->Modules[i];
		if (!Module.Asset.IsNull())
		{
			OutAssetToModule.Add(Module.Asset.ToSoftObjectPath(), i);
		}
	}

	// Collect all unique assets from cages
	TSet<FSoftObjectPath> UniqueAssets;
	for (const FPCGExValencyCageData& Data : CageData)
	{
		for (const TSoftObjectPtr<UObject>& Asset : Data.Assets)
		{
			if (!Asset.IsNull())
			{
				UniqueAssets.Add(Asset.ToSoftObjectPath());
			}
		}
	}

	// Create modules for new assets
	for (const FSoftObjectPath& AssetPath : UniqueAssets)
	{
		if (OutAssetToModule.Contains(AssetPath))
		{
			continue; // Already exists
		}

		// Create new module
		FPCGExValencyModuleDefinition& NewModule = TargetRules->Modules.AddDefaulted_GetRef();
		NewModule.Asset = TSoftObjectPtr<UObject>(AssetPath);
		NewModule.ModuleIndex = TargetRules->Modules.Num() - 1;

		OutAssetToModule.Add(AssetPath, NewModule.ModuleIndex);
	}
}

void UPCGExValencyBondingRulesBuilder::BuildNeighborRelationships(
	const TArray<FPCGExValencyCageData>& CageData,
	const TMap<FSoftObjectPath, int32>& AssetToModule,
	UPCGExValencyBondingRules* TargetRules,
	const UPCGExValencyOrbitalSet* OrbitalSet,
	FPCGExValencyBuildResult& OutResult)
{
	const FName LayerName = OrbitalSet->LayerName;

	// Build a cage pointer to cage data index map for fast lookup
	TMap<APCGExValencyCage*, int32> CageToDataIndex;
	for (int32 i = 0; i < CageData.Num(); ++i)
	{
		if (APCGExValencyCage* Cage = CageData[i].Cage.Get())
		{
			CageToDataIndex.Add(Cage, i);
		}
	}

	// For each cage, update its modules' neighbor info
	for (const FPCGExValencyCageData& Data : CageData)
	{
		APCGExValencyCage* Cage = Data.Cage.Get();
		if (!Cage)
		{
			continue;
		}

		// Get module indices for this cage's assets
		TArray<int32> CageModuleIndices;
		for (const TSoftObjectPtr<UObject>& Asset : Data.Assets)
		{
			if (const int32* ModuleIndex = AssetToModule.Find(Asset.ToSoftObjectPath()))
			{
				CageModuleIndices.AddUnique(*ModuleIndex);
			}
		}

		const TArray<FPCGExValencyCageOrbital>& Orbitals = Cage->GetOrbitals();

		for (const FPCGExValencyCageOrbital& Orbital : Orbitals)
		{
			if (!Orbital.bEnabled || Orbital.OrbitalIndex < 0)
			{
				continue;
			}

			// Get orbital name
			FName OrbitalName = Orbital.OrbitalName;
			if (OrbitalName.IsNone() && OrbitalSet->IsValidIndex(Orbital.OrbitalIndex))
			{
				OrbitalName = OrbitalSet->Orbitals[Orbital.OrbitalIndex].GetOrbitalName();
			}

			// Get neighbor modules
			TArray<int32> NeighborModuleIndices;

			if (Orbital.ConnectedCage.IsValid())
			{
				const APCGExValencyCageBase* ConnectedBase = Orbital.ConnectedCage.Get();

				if (ConnectedBase->IsNullCage())
				{
					// Null cage = boundary, no neighbor modules
					// We might want to add a special "null" module index here
				}
				else if (const APCGExValencyCage* ConnectedCage = Cast<APCGExValencyCage>(ConnectedBase))
				{
					// Get the connected cage's data
					if (const int32* ConnectedDataIndex = CageToDataIndex.Find(const_cast<APCGExValencyCage*>(ConnectedCage)))
					{
						const FPCGExValencyCageData& ConnectedData = CageData[*ConnectedDataIndex];

						// Add all of connected cage's assets as valid neighbors
						for (const TSoftObjectPtr<UObject>& ConnectedAsset : ConnectedData.Assets)
						{
							if (const int32* NeighborModuleIndex = AssetToModule.Find(ConnectedAsset.ToSoftObjectPath()))
							{
								NeighborModuleIndices.AddUnique(*NeighborModuleIndex);
							}
						}
					}
				}
			}

			// Update each of this cage's modules with the neighbor info
			for (int32 ModuleIndex : CageModuleIndices)
			{
				if (!TargetRules->Modules.IsValidIndex(ModuleIndex))
				{
					continue;
				}

				FPCGExValencyModuleDefinition& Module = TargetRules->Modules[ModuleIndex];

				// Get or create layer config
				FPCGExValencyModuleLayerConfig& LayerConfig = Module.Layers.FindOrAdd(LayerName);

				// Set orbital mask
				LayerConfig.OrbitalMask = Data.OrbitalMask;

				// Add neighbor modules for this orbital
				for (int32 NeighborModuleIndex : NeighborModuleIndices)
				{
					LayerConfig.AddValidNeighbor(OrbitalName, NeighborModuleIndex);
				}
			}
		}
	}
}

void UPCGExValencyBondingRulesBuilder::ValidateRules(
	UPCGExValencyBondingRules* Rules,
	const UPCGExValencyOrbitalSet* OrbitalSet,
	FPCGExValencyBuildResult& OutResult)
{
	if (!Rules || !OrbitalSet)
	{
		return;
	}

	const FName LayerName = OrbitalSet->LayerName;

	// Check for modules without any neighbors defined
	for (const FPCGExValencyModuleDefinition& Module : Rules->Modules)
	{
		const FPCGExValencyModuleLayerConfig* LayerConfig = Module.Layers.Find(LayerName);
		if (!LayerConfig)
		{
			OutResult.Warnings.Add(FText::Format(
				LOCTEXT("ModuleNoLayerConfig", "Module '{0}' has no configuration for layer '{1}'."),
				FText::FromString(Module.Asset.GetAssetName()),
				FText::FromName(LayerName)
			));
			continue;
		}

		// Check if any orbitals have no neighbors
		for (int32 i = 0; i < OrbitalSet->Num(); ++i)
		{
			if (LayerConfig->HasOrbital(i))
			{
				const FName OrbitalName = OrbitalSet->Orbitals[i].GetOrbitalName();
				const FPCGExValencyNeighborIndices* Neighbors = LayerConfig->OrbitalNeighbors.Find(OrbitalName);

				if (!Neighbors || Neighbors->Num() == 0)
				{
					OutResult.Warnings.Add(FText::Format(
						LOCTEXT("OrbitalNoNeighbors", "Module '{0}', orbital '{1}' has no valid neighbors defined."),
						FText::FromString(Module.Asset.GetAssetName()),
						FText::FromName(OrbitalName)
					));
				}
			}
		}
	}

	// Check for orphan modules (no cages reference them)
	// This would require tracking which modules came from which cages
}

TArray<TSoftObjectPtr<UObject>> UPCGExValencyBondingRulesBuilder::GetEffectiveAssets(const APCGExValencyCage* Cage)
{
	if (!Cage)
	{
		return {};
	}

	// Check for mirror source
	if (Cage->MirrorSource.IsValid())
	{
		const APCGExValencyCage* Source = Cage->MirrorSource.Get();
		if (Source && Source != Cage)
		{
			// Recursively get source's assets (handles chained mirrors)
			return GetEffectiveAssets(Source);
		}
	}

	return Cage->GetRegisteredAssets();
}

#undef LOCTEXT_NAMESPACE

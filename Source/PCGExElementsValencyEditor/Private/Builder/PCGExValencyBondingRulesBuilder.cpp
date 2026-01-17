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

	// Step 2: Build module mapping (keyed by Asset + OrbitalMask)
	TMap<FString, int32> ModuleKeyToIndex;
	BuildModuleMap(CageData, TargetRules, OrbitalSet, ModuleKeyToIndex, Result);

	// Step 3: Build neighbor relationships
	BuildNeighborRelationships(CageData, ModuleKeyToIndex, TargetRules, OrbitalSet, Result);

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

	// Generate synthetic collections from modules
	TargetRules->RebuildGeneratedCollections();

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

		// Trigger asset scan for cages with auto-registration enabled
		if (Cage->bAutoRegisterContainedAssets)
		{
			Cage->ScanAndRegisterContainedAssets();
		}

		// Get effective asset entries (resolving mirrors)
		TArray<FPCGExValencyAssetEntry> AssetEntries = GetEffectiveAssetEntries(Cage);
		if (AssetEntries.Num() == 0)
		{
			// Skip cages with no assets
			continue;
		}

		FPCGExValencyCageData Data;
		Data.Cage = Cage;
		Data.AssetEntries = MoveTemp(AssetEntries);
		Data.Settings = Cage->ModuleSettings;
		Data.bPreserveLocalTransforms = Cage->bPreserveLocalTransforms;

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

void UPCGExValencyBondingRulesBuilder::BuildModuleMap(
	const TArray<FPCGExValencyCageData>& CageData,
	UPCGExValencyBondingRules* TargetRules,
	const UPCGExValencyOrbitalSet* OrbitalSet,
	TMap<FString, int32>& OutModuleKeyToIndex,
	FPCGExValencyBuildResult& OutResult)
{
	OutModuleKeyToIndex.Empty();

	const FName LayerName = OrbitalSet->LayerName;

	// Collect all unique Asset + OrbitalMask (+ LocalTransform) combinations from cages
	// Each cage data already has its computed OrbitalMask
	for (const FPCGExValencyCageData& Data : CageData)
	{
		for (const FPCGExValencyAssetEntry& Entry : Data.AssetEntries)
		{
			if (!Entry.IsValid())
			{
				continue;
			}

			// Include local transform in key if cage preserves transforms
			const FTransform* TransformPtr = Data.bPreserveLocalTransforms ? &Entry.LocalTransform : nullptr;
			const FString ModuleKey = FPCGExValencyCageData::MakeModuleKey(
				Entry.Asset.ToSoftObjectPath(), Data.OrbitalMask, TransformPtr);

			if (OutModuleKeyToIndex.Contains(ModuleKey))
			{
				continue; // Already have a module for this combo
			}

			// Create new module
			const int32 NewModuleIndex = TargetRules->Modules.Num();
			FPCGExValencyModuleDefinition& NewModule = TargetRules->Modules.AddDefaulted_GetRef();
			NewModule.Asset = Entry.Asset;
			NewModule.AssetType = Entry.AssetType;
			NewModule.Settings = Data.Settings; // Copy settings from cage

			// Set local transform if cage preserves them
			if (Data.bPreserveLocalTransforms)
			{
				NewModule.LocalTransform = Entry.LocalTransform;
				NewModule.bHasLocalTransform = !Entry.LocalTransform.Equals(FTransform::Identity, 0.01f);
			}

#if WITH_EDITORONLY_DATA
			// Generate variant name for editor review
			NewModule.VariantName = GenerateVariantName(Entry, Data.OrbitalMask, NewModule.bHasLocalTransform);
#endif

			// Set up the layer config with the orbital mask
			FPCGExValencyModuleLayerConfig& LayerConfig = NewModule.Layers.FindOrAdd(LayerName);
			LayerConfig.OrbitalMask = Data.OrbitalMask;

			OutModuleKeyToIndex.Add(ModuleKey, NewModuleIndex);
		}
	}
}

void UPCGExValencyBondingRulesBuilder::BuildNeighborRelationships(
	const TArray<FPCGExValencyCageData>& CageData,
	const TMap<FString, int32>& ModuleKeyToIndex,
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

		// Get module indices for this cage's asset entries
		TArray<int32> CageModuleIndices;
		for (const FPCGExValencyAssetEntry& Entry : Data.AssetEntries)
		{
			const FTransform* TransformPtr = Data.bPreserveLocalTransforms ? &Entry.LocalTransform : nullptr;
			const FString ModuleKey = FPCGExValencyCageData::MakeModuleKey(
				Entry.Asset.ToSoftObjectPath(), Data.OrbitalMask, TransformPtr);
			if (const int32* ModuleIndex = ModuleKeyToIndex.Find(ModuleKey))
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

			// Get neighbor modules from connected cage
			TArray<int32> NeighborModuleIndices;

			if (Orbital.ConnectedCage.IsValid())
			{
				const APCGExValencyCageBase* ConnectedBase = Orbital.ConnectedCage.Get();

				if (ConnectedBase->IsNullCage())
				{
					// Null cage = boundary, mark this orbital as boundary for all modules from this cage
					for (int32 ModuleIndex : CageModuleIndices)
					{
						if (TargetRules->Modules.IsValidIndex(ModuleIndex))
						{
							FPCGExValencyModuleLayerConfig& LayerConfig = TargetRules->Modules[ModuleIndex].Layers.FindOrAdd(LayerName);
							LayerConfig.SetBoundaryOrbital(Orbital.OrbitalIndex);
						}
					}
				}
				else if (const APCGExValencyCage* ConnectedCage = Cast<APCGExValencyCage>(ConnectedBase))
				{
					// Get the connected cage's data
					if (const int32* ConnectedDataIndex = CageToDataIndex.Find(const_cast<APCGExValencyCage*>(ConnectedCage)))
					{
						const FPCGExValencyCageData& ConnectedData = CageData[*ConnectedDataIndex];

						// Add all of connected cage's modules as valid neighbors
						for (const FPCGExValencyAssetEntry& ConnectedEntry : ConnectedData.AssetEntries)
						{
							const FTransform* ConnectedTransformPtr = ConnectedData.bPreserveLocalTransforms
								? &ConnectedEntry.LocalTransform : nullptr;
							const FString NeighborKey = FPCGExValencyCageData::MakeModuleKey(
								ConnectedEntry.Asset.ToSoftObjectPath(), ConnectedData.OrbitalMask, ConnectedTransformPtr);
							if (const int32* NeighborModuleIndex = ModuleKeyToIndex.Find(NeighborKey))
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

				// Get layer config (already created in BuildModuleMap)
				FPCGExValencyModuleLayerConfig& LayerConfig = Module.Layers.FindOrAdd(LayerName);

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
					// Skip warning if this orbital connects to a boundary (null cage)
					if (!LayerConfig->IsBoundaryOrbital(i))
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
	}

	// Check for orphan modules (no cages reference them)
	// This would require tracking which modules came from which cages
}

TArray<FPCGExValencyAssetEntry> UPCGExValencyBondingRulesBuilder::GetEffectiveAssetEntries(const APCGExValencyCage* Cage)
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
			// Recursively get source's asset entries (handles chained mirrors)
			return GetEffectiveAssetEntries(Source);
		}
	}

	return Cage->GetRegisteredAssetEntries();
}

FString UPCGExValencyBondingRulesBuilder::GenerateVariantName(
	const FPCGExValencyAssetEntry& Entry,
	int64 OrbitalMask,
	bool bHasLocalTransform)
{
	// Get asset name
	FString AssetName = Entry.Asset.GetAssetName();
	if (AssetName.IsEmpty())
	{
		AssetName = TEXT("Unknown");
	}

	// Count connected orbitals for connectivity info
	int32 ConnectionCount = 0;
	for (int32 i = 0; i < 64; ++i)
	{
		if (OrbitalMask & (1LL << i))
		{
			++ConnectionCount;
		}
	}

	FString VariantName = FString::Printf(TEXT("%s_%dconn"), *AssetName, ConnectionCount);

	// Add transform indicator if present
	if (bHasLocalTransform)
	{
		const FVector Loc = Entry.LocalTransform.GetLocation();
		// Add simplified position indicator (e.g., "NE" for northeast corner)
		FString PosIndicator;
		if (FMath::Abs(Loc.X) > 1.0f || FMath::Abs(Loc.Y) > 1.0f)
		{
			if (Loc.X > 1.0f) PosIndicator += TEXT("E");
			else if (Loc.X < -1.0f) PosIndicator += TEXT("W");
			if (Loc.Y > 1.0f) PosIndicator += TEXT("N");
			else if (Loc.Y < -1.0f) PosIndicator += TEXT("S");
			if (Loc.Z > 1.0f) PosIndicator += TEXT("U");
			else if (Loc.Z < -1.0f) PosIndicator += TEXT("D");
		}
		if (!PosIndicator.IsEmpty())
		{
			VariantName += TEXT("_") + PosIndicator;
		}
		else
		{
			VariantName += TEXT("_offset");
		}
	}

	return VariantName;
}

#undef LOCTEXT_NAMESPACE

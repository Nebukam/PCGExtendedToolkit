// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Builder/PCGExValencyBondingRulesBuilder.h"

#include "Cages/PCGExValencyCage.h"
#include "Cages/PCGExValencyCageNull.h"
#include "Cages/PCGExValencyAssetPalette.h"
#include "Volumes/ValencyContextVolume.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Core/PCGExValencyLog.h"

#define LOCTEXT_NAMESPACE "PCGExValencyBuilder"

FPCGExValencyBuildResult UPCGExValencyBondingRulesBuilder::BuildFromVolume(AValencyContextVolume* Volume)
{
	if (!Volume)
	{
		FPCGExValencyBuildResult Result;
		Result.Errors.Add(LOCTEXT("NoVolume", "No volume provided."));
		return Result;
	}

	// Delegate to multi-volume version with single volume
	return BuildFromVolumes({Volume}, Volume);
}

FPCGExValencyBuildResult UPCGExValencyBondingRulesBuilder::BuildFromVolumes(const TArray<AValencyContextVolume*>& Volumes, AValencyContextVolume* TriggeringVolume)
{
	FPCGExValencyBuildResult Result;

	if (Volumes.Num() == 0)
	{
		Result.Errors.Add(LOCTEXT("NoVolumes", "No volumes provided."));
		return Result;
	}

	// Use first volume to get shared resources, or triggering volume if provided
	AValencyContextVolume* PrimaryVolume = TriggeringVolume ? TriggeringVolume : Volumes[0];

	UPCGExValencyBondingRules* TargetRules = PrimaryVolume->GetBondingRules();
	if (!TargetRules)
	{
		Result.Errors.Add(LOCTEXT("NoBondingRules", "Primary volume has no BondingRules asset assigned."));
		return Result;
	}

	UPCGExValencyOrbitalSet* OrbitalSet = PrimaryVolume->GetEffectiveOrbitalSet();
	if (!OrbitalSet)
	{
		Result.Errors.Add(LOCTEXT("NoOrbitalSet", "Primary volume has no OrbitalSet (check BondingRules or override)."));
		return Result;
	}

	// Verify all volumes reference the same BondingRules
	for (AValencyContextVolume* Volume : Volumes)
	{
		if (Volume && Volume->GetBondingRules() != TargetRules)
		{
			Result.Warnings.Add(FText::Format(
				LOCTEXT("MismatchedRules", "Volume '{0}' references different BondingRules - skipping."),
				FText::FromString(Volume->GetName())
			));
		}
	}

	// Collect cages from ALL volumes that share the same BondingRules
	TArray<APCGExValencyCage*> AllRegularCages;
	for (AValencyContextVolume* Volume : Volumes)
	{
		if (!Volume || Volume->GetBondingRules() != TargetRules)
		{
			continue;
		}

		// Ensure cage relationships are up-to-date before building
		Volume->RefreshCageRelationships();

		// Collect cages from this volume
		TArray<APCGExValencyCageBase*> VolumeCages;
		Volume->CollectContainedCages(VolumeCages);

		// Filter to regular cages (not null cages)
		for (APCGExValencyCageBase* CageBase : VolumeCages)
		{
			if (APCGExValencyCage* Cage = Cast<APCGExValencyCage>(CageBase))
			{
				AllRegularCages.AddUnique(Cage);
			}
		}
	}

	// Build from all collected cages
	Result = BuildFromCages(AllRegularCages, TargetRules, OrbitalSet);

	// Update build metadata on success
	if (Result.bSuccess && PrimaryVolume)
	{
		if (UWorld* World = PrimaryVolume->GetWorld())
		{
			TargetRules->LastBuildLevelPath = World->GetMapName();
		}
		TargetRules->LastBuildVolumeName = PrimaryVolume->GetName();
		TargetRules->LastBuildTime = FDateTime::Now();
	}

	return Result;
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

	// Step 1.5: Discover material variants from mesh components
	DiscoverMaterialVariants(CageData, TargetRules);

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

	TargetRules->Modify();

	// Generate synthetic collections from modules
	TargetRules->RebuildGeneratedCollections();

	// Mark asset as modified
	(void)TargetRules->MarkPackageDirty();

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

	VALENCY_LOG_SECTION(Building, "COLLECTING CAGE DATA");
	PCGEX_VALENCY_INFO(Building, "Processing %d cages", Cages.Num());

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
			PCGEX_VALENCY_VERBOSE(Building, "  Cage '%s': NO ASSETS - skipping", *Cage->GetCageDisplayName());
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
		PCGEX_VALENCY_VERBOSE(Building, "  Cage '%s': %d assets, %d orbitals",
			*Cage->GetCageDisplayName(), Data.AssetEntries.Num(), Orbitals.Num());

		for (const FPCGExValencyCageOrbital& Orbital : Orbitals)
		{
			if (!Orbital.bEnabled)
			{
				PCGEX_VALENCY_VERBOSE(Building, "    Orbital[%d] '%s': DISABLED", Orbital.OrbitalIndex, *Orbital.OrbitalName.ToString());
				continue;
			}

			// Only count connected orbitals (or null cage connections if enabled)
			if (const APCGExValencyCageBase* ConnectedCage = Orbital.GetDisplayConnection())
			{

				// Check if it's a null cage (boundary)
				if (ConnectedCage->IsNullCage())
				{
					// Null cage = boundary. Do NOT set orbital bit here.
					// The boundary is tracked separately via BoundaryMask in BuildNeighborRelationships.
					// OrbitalMask should only include REAL connections.
					PCGEX_VALENCY_VERBOSE(Building, "    Orbital[%d] '%s': NULL CAGE (boundary) - tracked as boundary, not in OrbitalMask",
						Orbital.OrbitalIndex, *Orbital.OrbitalName.ToString());
				}
				else
				{
					// Regular connection - set the orbital bit
					Data.OrbitalMask |= (1LL << Orbital.OrbitalIndex);
					PCGEX_VALENCY_VERBOSE(Building, "    Orbital[%d] '%s': Connected to '%s' - bit set",
						Orbital.OrbitalIndex, *Orbital.OrbitalName.ToString(), *ConnectedCage->GetCageDisplayName());
				}
			}
			else
			{
				PCGEX_VALENCY_VERBOSE(Building, "    Orbital[%d] '%s': NO CONNECTION", Orbital.OrbitalIndex, *Orbital.OrbitalName.ToString());
			}
		}

		// Log final orbital mask
		FString MaskBits;
		for (int32 Bit = 0; Bit < OrbitalSet->Num(); ++Bit)
		{
			MaskBits += (Data.OrbitalMask & (1LL << Bit)) ? TEXT("1") : TEXT("0");
		}
		PCGEX_VALENCY_VERBOSE(Building, "    -> Final OrbitalMask: %s (0x%llX)", *MaskBits, Data.OrbitalMask);

		OutCageData.Add(MoveTemp(Data));
	}

	VALENCY_LOG_SECTION(Building, "CAGE DATA COLLECTION COMPLETE");
	PCGEX_VALENCY_INFO(Building, "Valid cages: %d", OutCageData.Num());
}

void UPCGExValencyBondingRulesBuilder::BuildModuleMap(
	const TArray<FPCGExValencyCageData>& CageData,
	UPCGExValencyBondingRules* TargetRules,
	const UPCGExValencyOrbitalSet* OrbitalSet,
	TMap<FString, int32>& OutModuleKeyToIndex,
	FPCGExValencyBuildResult& OutResult)
{
	OutModuleKeyToIndex.Empty();

	VALENCY_LOG_SECTION(Building, "BUILDING MODULE MAP");

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
				PCGEX_VALENCY_VERBOSE(Building, "  Module key '%s' already exists", *ModuleKey);
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

			// Log mask as binary
			FString MaskBits;
			for (int32 Bit = 0; Bit < OrbitalSet->Num(); ++Bit)
			{
				MaskBits += (Data.OrbitalMask & (1LL << Bit)) ? TEXT("1") : TEXT("0");
			}

			PCGEX_VALENCY_VERBOSE(Building, "  Module[%d]: Asset='%s', OrbitalMask=%s (0x%llX)",
				NewModuleIndex, *Entry.Asset.GetAssetName(), *MaskBits, Data.OrbitalMask);

			OutModuleKeyToIndex.Add(ModuleKey, NewModuleIndex);
		}
	}

	VALENCY_LOG_SECTION(Building, "MODULE MAP COMPLETE");
	PCGEX_VALENCY_INFO(Building, "Total modules: %d", OutModuleKeyToIndex.Num());
}

void UPCGExValencyBondingRulesBuilder::BuildNeighborRelationships(
	const TArray<FPCGExValencyCageData>& CageData,
	const TMap<FString, int32>& ModuleKeyToIndex,
	UPCGExValencyBondingRules* TargetRules,
	const UPCGExValencyOrbitalSet* OrbitalSet,
	FPCGExValencyBuildResult& OutResult)
{
	VALENCY_LOG_SECTION(Building, "BUILDING NEIGHBOR RELATIONSHIPS");

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

		PCGEX_VALENCY_VERBOSE(Building, "  Processing cage '%s':", *Cage->GetCageDisplayName());

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

		PCGEX_VALENCY_VERBOSE(Building, "    Cage modules: [%s]",
			*FString::JoinBy(CageModuleIndices, TEXT(", "), [](int32 Idx) { return FString::FromInt(Idx); }));

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

			if (const APCGExValencyCageBase* ConnectedBase = Orbital.GetDisplayConnection())
			{
				if (ConnectedBase->IsNullCage())
				{
					PCGEX_VALENCY_VERBOSE(Building, "    Orbital[%d] '%s': BOUNDARY (null cage)",
						Orbital.OrbitalIndex, *OrbitalName.ToString());

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
								                                          ? &ConnectedEntry.LocalTransform
								                                          : nullptr;
							const FString NeighborKey = FPCGExValencyCageData::MakeModuleKey(
								ConnectedEntry.Asset.ToSoftObjectPath(), ConnectedData.OrbitalMask, ConnectedTransformPtr);
							if (const int32* NeighborModuleIndex = ModuleKeyToIndex.Find(NeighborKey))
							{
								NeighborModuleIndices.AddUnique(*NeighborModuleIndex);
							}
						}
					}

					PCGEX_VALENCY_VERBOSE(Building, "    Orbital[%d] '%s': Connected to '%s', neighbor modules: [%s]",
						Orbital.OrbitalIndex, *OrbitalName.ToString(), *ConnectedCage->GetCageDisplayName(),
						*FString::JoinBy(NeighborModuleIndices, TEXT(", "), [](int32 Idx) { return FString::FromInt(Idx); }));
				}
			}
			else
			{
				PCGEX_VALENCY_VERBOSE(Building, "    Orbital[%d] '%s': NO CONNECTION",
					Orbital.OrbitalIndex, *OrbitalName.ToString());
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

	VALENCY_LOG_SECTION(Building, "NEIGHBOR RELATIONSHIPS COMPLETE");
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

	TArray<FPCGExValencyAssetEntry> AllEntries;

	// Start with cage's own assets
	AllEntries.Append(Cage->GetAllAssetEntries());

	// If no mirror sources, return early
	if (Cage->MirrorSources.Num() == 0)
	{
		return AllEntries;
	}

	// Get this cage's rotation for applying to mirrored local transforms
	const FQuat CageRotation = Cage->GetActorQuat();

	// Track visited sources to prevent infinite recursion
	TSet<const AActor*> VisitedSources;
	VisitedSources.Add(Cage);

	// Lambda to collect entries from a source (with optional recursion)
	TFunction<void(AActor*, bool)> CollectFromSource = [&](AActor* Source, bool bRecursive)
	{
		if (!Source || VisitedSources.Contains(Source))
		{
			return;
		}
		VisitedSources.Add(Source);

		TArray<FPCGExValencyAssetEntry> SourceEntries;

		// Check if it's a cage
		if (const APCGExValencyCage* SourceCage = Cast<APCGExValencyCage>(Source))
		{
			SourceEntries = SourceCage->GetAllAssetEntries();

			// Recursively collect from cage's mirror sources
			if (bRecursive)
			{
				for (const TObjectPtr<AActor>& NestedSource : SourceCage->MirrorSources)
				{
					CollectFromSource(NestedSource, SourceCage->bRecursiveMirror);
				}
			}
		}
		// Check if it's an asset palette
		else if (const APCGExValencyAssetPalette* SourcePalette = Cast<APCGExValencyAssetPalette>(Source))
		{
			SourceEntries = SourcePalette->GetAllAssetEntries();
		}

		// Apply cage rotation to mirrored local transforms and add to results
		for (FPCGExValencyAssetEntry& Entry : SourceEntries)
		{
			if (!Entry.LocalTransform.Equals(FTransform::Identity, 0.1f))
			{
				// Rotate the source's local offset by this cage's rotation
				const FVector RotatedOffset = CageRotation.RotateVector(Entry.LocalTransform.GetTranslation());
				const FQuat CombinedRotation = CageRotation * Entry.LocalTransform.GetRotation();

				Entry.LocalTransform.SetTranslation(RotatedOffset);
				Entry.LocalTransform.SetRotation(CombinedRotation);
			}

			AllEntries.Add(Entry);
		}
	};

	// Collect from all mirror sources
	for (const TObjectPtr<AActor>& Source : Cage->MirrorSources)
	{
		CollectFromSource(Source, Cage->bRecursiveMirror);
	}

	return AllEntries;
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

void UPCGExValencyBondingRulesBuilder::DiscoverMaterialVariants(
	const TArray<FPCGExValencyCageData>& CageData,
	UPCGExValencyBondingRules* TargetRules)
{
	if (!TargetRules)
	{
		return;
	}

	// Clear previous discoveries
	TargetRules->DiscoveredMaterialVariants.Empty();

	// Collect material variants from all cages
	// Variants are discovered during cage scanning, we just need to merge them
	for (const FPCGExValencyCageData& Data : CageData)
	{
		APCGExValencyCage* Cage = Data.Cage.Get();
		if (!Cage)
		{
			continue;
		}

		const TMap<FSoftObjectPath, TArray<FPCGExValencyMaterialVariant>>& CageVariants = Cage->GetDiscoveredMaterialVariants();

		// Merge cage variants into target rules
		for (const auto& Pair : CageVariants)
		{
			const FSoftObjectPath& MeshPath = Pair.Key;
			const TArray<FPCGExValencyMaterialVariant>& CageVariantList = Pair.Value;

			TArray<FPCGExValencyMaterialVariant>& TargetVariants = TargetRules->DiscoveredMaterialVariants.FindOrAdd(MeshPath);

			for (const FPCGExValencyMaterialVariant& CageVariant : CageVariantList)
			{
				// Check if this exact configuration already exists in target
				bool bFound = false;
				for (FPCGExValencyMaterialVariant& ExistingVariant : TargetVariants)
				{
					if (ExistingVariant == CageVariant)
					{
						// Merge discovery counts
						ExistingVariant.DiscoveryCount += CageVariant.DiscoveryCount;
						bFound = true;
						break;
					}
				}

				if (!bFound)
				{
					TargetVariants.Add(CageVariant);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

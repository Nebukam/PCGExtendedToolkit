// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/PCGExValencyReferenceTracker.h"

#include "Cages/PCGExValencyCageBase.h"
#include "Cages/PCGExValencyCage.h"
#include "Cages/PCGExValencyAssetPalette.h"
#include "Volumes/ValencyContextVolume.h"

void FValencyReferenceTracker::Initialize(
	const TArray<TWeakObjectPtr<APCGExValencyCageBase>>* InCachedCages,
	const TArray<TWeakObjectPtr<AValencyContextVolume>>* InCachedVolumes,
	const TArray<TWeakObjectPtr<APCGExValencyAssetPalette>>* InCachedPalettes)
{
	CachedCages = InCachedCages;
	CachedVolumes = InCachedVolumes;
	CachedPalettes = InCachedPalettes;

	// Build the dependency graph for fast lookups
	RebuildDependencyGraph();
}

void FValencyReferenceTracker::Reset()
{
	CachedCages = nullptr;
	CachedVolumes = nullptr;
	CachedPalettes = nullptr;
	DependentsMap.Empty();
}

void FValencyReferenceTracker::RebuildDependencyGraph()
{
	DependentsMap.Empty();

	if (!CachedCages)
	{
		return;
	}

	// For each cage, register its MirrorSources as dependencies
	// This builds the reverse lookup: Source → [Cages that mirror it]
	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : *CachedCages)
	{
		APCGExValencyCage* Cage = Cast<APCGExValencyCage>(CagePtr.Get());
		if (!Cage)
		{
			continue;
		}

		for (const TObjectPtr<AActor>& Source : Cage->MirrorSources)
		{
			if (Source)
			{
				// Source is depended upon by Cage
				TArray<TWeakObjectPtr<AActor>>& Dependents = DependentsMap.FindOrAdd(Source);
				Dependents.AddUnique(Cage);
			}
		}
	}

	// Future: Add other reference types here
	// - Nested volumes
	// - Pattern rules
}

void FValencyReferenceTracker::OnActorReferencesChanged(AActor* Actor)
{
	// Rebuild the graph when references change
	RebuildDependencyGraph();
}

bool FValencyReferenceTracker::PropagateContentChange(AActor* ChangedActor, bool bRefreshGhosts, bool bTriggerRebuild)
{
	if (!ChangedActor)
	{
		return false;
	}

	// Only process when Valency mode is active
	if (!AValencyContextVolume::IsValencyModeActive())
	{
		return false;
	}

	// Collect all affected actors in one pass (iterative, not recursive)
	TArray<AActor*> AffectedActors;
	CollectAffectedActors(ChangedActor, AffectedActors);

	if (AffectedActors.Num() == 0)
	{
		return false;
	}

	// First pass: refresh ghost meshes for all affected actors
	if (bRefreshGhosts)
	{
		for (AActor* Affected : AffectedActors)
		{
			RefreshDependentVisuals(Affected);
		}
	}

	// Second pass: collect unique volumes that need rebuilding (avoid redundant rebuilds)
	bool bAnyRebuilt = false;
	if (bTriggerRebuild)
	{
		TSet<AValencyContextVolume*> VolumesToRebuild;

		for (AActor* Affected : AffectedActors)
		{
			if (APCGExValencyCageBase* Cage = Cast<APCGExValencyCageBase>(Affected))
			{
				// Collect volumes this cage belongs to
				for (const TWeakObjectPtr<AValencyContextVolume>& VolumePtr : Cage->GetContainingVolumes())
				{
					if (AValencyContextVolume* Volume = VolumePtr.Get())
					{
						if (Volume->bAutoRebuildOnChange)
						{
							VolumesToRebuild.Add(Volume);
						}
					}
				}
			}
		}

		// Trigger one rebuild per unique volume
		for (AValencyContextVolume* Volume : VolumesToRebuild)
		{
			if (Volume)
			{
				Volume->BuildRulesFromCages();
				bAnyRebuilt = true;
			}
		}
	}

	return bAnyRebuilt || AffectedActors.Num() > 0;
}

const TArray<TWeakObjectPtr<AActor>>* FValencyReferenceTracker::GetDependents(AActor* Actor) const
{
	return DependentsMap.Find(Actor);
}

bool FValencyReferenceTracker::DependsOn(AActor* ActorA, AActor* ActorB) const
{
	if (!ActorA || !ActorB || ActorA == ActorB)
	{
		return false;
	}

	// BFS to check if ActorA transitively depends on ActorB
	TSet<AActor*> Visited;
	TArray<AActor*> ToCheck;

	// Start from ActorA's dependencies (what it references)
	if (APCGExValencyCage* Cage = Cast<APCGExValencyCage>(ActorA))
	{
		for (const TObjectPtr<AActor>& Source : Cage->MirrorSources)
		{
			if (Source)
			{
				ToCheck.Add(Source);
			}
		}
	}

	while (ToCheck.Num() > 0)
	{
		AActor* Current = ToCheck.Pop(EAllowShrinking::No);
		if (!Current || Visited.Contains(Current))
		{
			continue;
		}
		Visited.Add(Current);

		if (Current == ActorB)
		{
			return true;
		}

		// Add Current's dependencies
		if (APCGExValencyCage* Cage = Cast<APCGExValencyCage>(Current))
		{
			for (const TObjectPtr<AActor>& Source : Cage->MirrorSources)
			{
				if (Source && !Visited.Contains(Source))
				{
					ToCheck.Add(Source);
				}
			}
		}
	}

	return false;
}

void FValencyReferenceTracker::CollectAffectedActors(AActor* StartActor, TArray<AActor*>& OutAffected)
{
	OutAffected.Empty();

	if (!StartActor)
	{
		return;
	}

	// BFS to collect all dependents (iterative to avoid stack overflow on deep chains)
	TSet<AActor*> Visited;
	TArray<AActor*> ToProcess;
	ToProcess.Add(StartActor);
	Visited.Add(StartActor);

	while (ToProcess.Num() > 0)
	{
		AActor* Current = ToProcess.Pop(EAllowShrinking::No);

		// Get direct dependents from pre-built map (O(1))
		if (const TArray<TWeakObjectPtr<AActor>>* Dependents = DependentsMap.Find(Current))
		{
			for (const TWeakObjectPtr<AActor>& DepPtr : *Dependents)
			{
				AActor* Dependent = DepPtr.Get();
				if (Dependent && !Visited.Contains(Dependent))
				{
					Visited.Add(Dependent);
					OutAffected.Add(Dependent);
					ToProcess.Add(Dependent); // Continue BFS to find transitive dependents
				}
			}
		}
	}
}

void FValencyReferenceTracker::RefreshDependentVisuals(AActor* Dependent)
{
	if (!Dependent)
	{
		return;
	}

	// Refresh ghost meshes for cages
	if (APCGExValencyCage* Cage = Cast<APCGExValencyCage>(Dependent))
	{
		Cage->RefreshMirrorGhostMeshes();
	}

	// Future: other visual refreshes for new types
}

bool FValencyReferenceTracker::TriggerDependentRebuild(AActor* Dependent)
{
	if (!Dependent)
	{
		return false;
	}

	// For cages, use existing rebuild mechanism
	if (APCGExValencyCage* Cage = Cast<APCGExValencyCage>(Dependent))
	{
		return Cage->TriggerAutoRebuildIfNeeded();
	}

	return false;
}

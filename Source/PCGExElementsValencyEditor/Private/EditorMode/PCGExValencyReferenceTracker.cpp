// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "EditorMode/PCGExValencyReferenceTracker.h"

#include "Cages/PCGExValencyCageBase.h"
#include "Cages/PCGExValencyCage.h"
#include "Cages/PCGExValencyCagePattern.h"
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

	// For each cage, register its dependencies
	// This builds the reverse lookup: Source → [Cages that depend on it]
	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : *CachedCages)
	{
		// Regular cages: MirrorSources dependencies
		if (APCGExValencyCage* Cage = Cast<APCGExValencyCage>(CagePtr.Get()))
		{
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
		// Pattern cages: ProxiedCages dependencies
		else if (APCGExValencyCagePattern* PatternCage = Cast<APCGExValencyCagePattern>(CagePtr.Get()))
		{
			for (const TObjectPtr<APCGExValencyCage>& ProxiedCage : PatternCage->ProxiedCages)
			{
				if (ProxiedCage)
				{
					// ProxiedCage is depended upon by PatternCage
					TArray<TWeakObjectPtr<AActor>>& Dependents = DependentsMap.FindOrAdd(ProxiedCage);
					Dependents.AddUnique(PatternCage);
				}
			}
		}
	}
}

void FValencyReferenceTracker::OnActorReferencesChanged(AActor* Actor)
{
	// Rebuild the graph when references change
	RebuildDependencyGraph();
}

void FValencyReferenceTracker::OnMirrorSourcesChanged(APCGExValencyCage* Cage)
{
	if (!Cage)
	{
		return;
	}

	// Remove existing edges from this cage
	RemoveAllEdgesFrom(Cage);

	// Add new edges based on current MirrorSources
	for (const TObjectPtr<AActor>& Source : Cage->MirrorSources)
	{
		if (Source)
		{
			AddDependency(Cage, Source);
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("Valency: Updated dependency graph for cage '%s' MirrorSources change"),
		*Cage->GetCageDisplayName());
}

void FValencyReferenceTracker::OnProxiedCagesChanged(APCGExValencyCagePattern* PatternCage)
{
	if (!PatternCage)
	{
		return;
	}

	// Remove existing edges from this pattern cage
	RemoveAllEdgesFrom(PatternCage);

	// Add new edges based on current ProxiedCages
	for (const TObjectPtr<APCGExValencyCage>& ProxiedCage : PatternCage->ProxiedCages)
	{
		if (ProxiedCage)
		{
			AddDependency(PatternCage, ProxiedCage);
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("Valency: Updated dependency graph for pattern cage '%s' ProxiedCages change"),
		*PatternCage->GetCageDisplayName());
}

void FValencyReferenceTracker::OnActorRemoved(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	// Remove this actor from being a dependency source (remove from map keys)
	DependentsMap.Remove(Actor);

	// Remove this actor from all dependent lists (remove from map values)
	for (auto& Pair : DependentsMap)
	{
		Pair.Value.RemoveAll([Actor](const TWeakObjectPtr<AActor>& DepPtr)
		{
			return DepPtr.Get() == Actor;
		});
	}

	UE_LOG(LogTemp, Verbose, TEXT("Valency: Removed actor from dependency graph"));
}

void FValencyReferenceTracker::RemoveAllEdgesFrom(AActor* Dependent)
{
	if (!Dependent)
	{
		return;
	}

	// Remove Dependent from all dependents lists in the map
	// (This is O(n) but happens rarely - only when references change)
	for (auto& Pair : DependentsMap)
	{
		Pair.Value.RemoveAll([Dependent](const TWeakObjectPtr<AActor>& DepPtr)
		{
			return DepPtr.Get() == Dependent;
		});
	}
}

void FValencyReferenceTracker::AddDependency(AActor* Dependent, AActor* DependsOn)
{
	if (!Dependent || !DependsOn || Dependent == DependsOn)
	{
		return;
	}

	// DependsOn is depended upon by Dependent
	TArray<TWeakObjectPtr<AActor>>& Dependents = DependentsMap.FindOrAdd(DependsOn);
	Dependents.AddUnique(Dependent);
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

	// Second pass: mark affected cages dirty (dirty state system handles rebuild)
	bool bAnyMarkedDirty = false;
	if (bTriggerRebuild)
	{
		for (AActor* Affected : AffectedActors)
		{
			if (APCGExValencyCageBase* Cage = Cast<APCGExValencyCageBase>(Affected))
			{
				Cage->RequestRebuild(EValencyRebuildReason::ExternalCascade);
				bAnyMarkedDirty = true;
			}
		}
	}

	return bAnyMarkedDirty || AffectedActors.Num() > 0;
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
	else if (APCGExValencyCagePattern* PatternCage = Cast<APCGExValencyCagePattern>(Dependent))
	{
		PatternCage->RefreshProxyGhostMesh();
	}
}

bool FValencyReferenceTracker::TriggerDependentRebuild(AActor* Dependent)
{
	if (!Dependent)
	{
		return false;
	}

	// For cages, use unified rebuild mechanism
	if (APCGExValencyCageBase* Cage = Cast<APCGExValencyCageBase>(Dependent))
	{
		Cage->RequestRebuild(EValencyRebuildReason::ExternalCascade);
		return true;
	}

	return false;
}

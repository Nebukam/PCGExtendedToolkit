// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyCageSpatialRegistry.h"

#include "EngineUtils.h"
#include "Cages/PCGExValencyCageBase.h"

TMap<TWeakObjectPtr<UWorld>, TSharedPtr<FPCGExValencyCageSpatialRegistry>> FPCGExValencyCageSpatialRegistry::WorldRegistries;

FPCGExValencyCageSpatialRegistry::FPCGExValencyCageSpatialRegistry()
{
}

FPCGExValencyCageSpatialRegistry& FPCGExValencyCageSpatialRegistry::Get(UWorld* World)
{
	if (!World)
	{
		static FPCGExValencyCageSpatialRegistry DummyRegistry;
		return DummyRegistry;
	}

	TWeakObjectPtr<UWorld> WeakWorld = World;
	TSharedPtr<FPCGExValencyCageSpatialRegistry>* Found = WorldRegistries.Find(WeakWorld);

	if (!Found || !Found->IsValid())
	{
		TSharedPtr<FPCGExValencyCageSpatialRegistry> NewRegistry = MakeShared<FPCGExValencyCageSpatialRegistry>();
		NewRegistry->RebuildFromWorld(World);
		WorldRegistries.Add(WeakWorld, NewRegistry);
		return *NewRegistry;
	}

	return **Found;
}

void FPCGExValencyCageSpatialRegistry::Clear(UWorld* World)
{
	if (World)
	{
		WorldRegistries.Remove(World);
	}
}

void FPCGExValencyCageSpatialRegistry::RegisterCage(APCGExValencyCageBase* Cage)
{
	if (!Cage)
	{
		return;
	}

	// Add to all cages set
	AllCages.Add(Cage);

	// Add to spatial hash
	const FVector Position = Cage->GetActorLocation();
	const FIntVector Cell = PositionToCell(Position);
	const uint64 Key = GetCellKey(Cell);

	TArray<TWeakObjectPtr<APCGExValencyCageBase>>& CellCages = SpatialHash.FindOrAdd(Key);
	CellCages.AddUnique(Cage);

	// Update max probe radius
	const float ProbeRadius = Cage->GetEffectiveProbeRadius();
	if (ProbeRadius > MaxProbeRadius)
	{
		MaxProbeRadius = ProbeRadius;
	}
}

void FPCGExValencyCageSpatialRegistry::UnregisterCage(APCGExValencyCageBase* Cage)
{
	if (!Cage)
	{
		return;
	}

	// Remove from all cages
	AllCages.Remove(Cage);

	// Remove from spatial hash
	const FVector Position = Cage->GetActorLocation();
	const FIntVector Cell = PositionToCell(Position);
	const uint64 Key = GetCellKey(Cell);

	if (TArray<TWeakObjectPtr<APCGExValencyCageBase>>* CellCages = SpatialHash.Find(Key))
	{
		CellCages->Remove(Cage);
		if (CellCages->Num() == 0)
		{
			SpatialHash.Remove(Key);
		}
	}

	// Recalculate max probe radius if needed
	if (FMath::IsNearlyEqual(Cage->GetEffectiveProbeRadius(), MaxProbeRadius))
	{
		RecalculateMaxProbeRadius();
	}
}

void FPCGExValencyCageSpatialRegistry::UpdateCagePosition(APCGExValencyCageBase* Cage, const FVector& OldPosition, const FVector& NewPosition)
{
	if (!Cage)
	{
		return;
	}

	const FIntVector OldCell = PositionToCell(OldPosition);
	const FIntVector NewCell = PositionToCell(NewPosition);

	// Only update hash if cell changed
	if (OldCell != NewCell)
	{
		// Remove from old cell
		const uint64 OldKey = GetCellKey(OldCell);
		if (TArray<TWeakObjectPtr<APCGExValencyCageBase>>* OldCellCages = SpatialHash.Find(OldKey))
		{
			OldCellCages->Remove(Cage);
			if (OldCellCages->Num() == 0)
			{
				SpatialHash.Remove(OldKey);
			}
		}

		// Add to new cell
		const uint64 NewKey = GetCellKey(NewCell);
		TArray<TWeakObjectPtr<APCGExValencyCageBase>>& NewCellCages = SpatialHash.FindOrAdd(NewKey);
		NewCellCages.AddUnique(Cage);
	}
}

void FPCGExValencyCageSpatialRegistry::FindCagesNearPosition(const FVector& Position, float MaxQueryRadius, TArray<APCGExValencyCageBase*>& OutCages, APCGExValencyCageBase* ExcludeCage) const
{
	OutCages.Empty();

	// Use the larger of query radius or max probe radius (for bidirectional detection)
	const float EffectiveRadius = FMath::Max(MaxQueryRadius, MaxProbeRadius);

	// Get all cells that could contain relevant cages
	TArray<FIntVector> OverlappingCells;
	GetOverlappingCells(Position, EffectiveRadius, OverlappingCells);

	// Collect cages from those cells
	TSet<APCGExValencyCageBase*> UniqueResults;

	for (const FIntVector& Cell : OverlappingCells)
	{
		const uint64 Key = GetCellKey(Cell);
		if (const TArray<TWeakObjectPtr<APCGExValencyCageBase>>* CellCages = SpatialHash.Find(Key))
		{
			for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : *CellCages)
			{
				APCGExValencyCageBase* Cage = CagePtr.Get();
				if (!Cage || Cage == ExcludeCage)
				{
					continue;
				}

				// Check actual distance
				const float Distance = FVector::Dist(Position, Cage->GetActorLocation());
				const float CageProbeRadius = Cage->GetEffectiveProbeRadius();

				// Include if either:
				// - We're within the query radius of the cage
				// - The cage's probe radius reaches us
				if (Distance <= MaxQueryRadius || Distance <= CageProbeRadius)
				{
					UniqueResults.Add(Cage);
				}
			}
		}
	}

	OutCages = UniqueResults.Array();
}

void FPCGExValencyCageSpatialRegistry::FindAffectedCages(APCGExValencyCageBase* MovingCage, const FVector& OldPosition, const FVector& NewPosition, TSet<APCGExValencyCageBase*>& OutAffectedCages) const
{
	OutAffectedCages.Empty();

	if (!MovingCage)
	{
		return;
	}

	const float MovingCageRadius = MovingCage->GetEffectiveProbeRadius();

	// Query radius is max of moving cage's radius and max probe radius (for reverse detection)
	const float QueryRadius = FMath::Max(MovingCageRadius, MaxProbeRadius);

	// Find cages near old position
	TArray<APCGExValencyCageBase*> OldNeighbors;
	FindCagesNearPosition(OldPosition, QueryRadius, OldNeighbors, MovingCage);

	// Find cages near new position
	TArray<APCGExValencyCageBase*> NewNeighbors;
	FindCagesNearPosition(NewPosition, QueryRadius, NewNeighbors, MovingCage);

	// Union of both sets - all potentially affected
	for (APCGExValencyCageBase* Cage : OldNeighbors)
	{
		OutAffectedCages.Add(Cage);
	}
	for (APCGExValencyCageBase* Cage : NewNeighbors)
	{
		OutAffectedCages.Add(Cage);
	}
}

void FPCGExValencyCageSpatialRegistry::RebuildFromWorld(UWorld* World)
{
	SpatialHash.Empty();
	AllCages.Empty();
	MaxProbeRadius = 0.0f;

	if (!World)
	{
		return;
	}

	for (TActorIterator<APCGExValencyCageBase> It(World); It; ++It)
	{
		RegisterCage(*It);
	}
}

void FPCGExValencyCageSpatialRegistry::SetCellSize(float NewCellSize)
{
	if (!FMath::IsNearlyEqual(CellSize, NewCellSize) && NewCellSize > 0.0f)
	{
		CellSize = NewCellSize;

		// Rebuild hash with new cell size
		TArray<APCGExValencyCageBase*> CagesToReregister;
		for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : AllCages)
		{
			if (APCGExValencyCageBase* Cage = CagePtr.Get())
			{
				CagesToReregister.Add(Cage);
			}
		}

		SpatialHash.Empty();
		for (APCGExValencyCageBase* Cage : CagesToReregister)
		{
			const FVector Position = Cage->GetActorLocation();
			const FIntVector Cell = PositionToCell(Position);
			const uint64 Key = GetCellKey(Cell);
			TArray<TWeakObjectPtr<APCGExValencyCageBase>>& CellCages = SpatialHash.FindOrAdd(Key);
			CellCages.AddUnique(Cage);
		}
	}
}

FIntVector FPCGExValencyCageSpatialRegistry::PositionToCell(const FVector& Position) const
{
	return FIntVector(
		FMath::FloorToInt(Position.X / CellSize),
		FMath::FloorToInt(Position.Y / CellSize),
		FMath::FloorToInt(Position.Z / CellSize)
	);
}

uint64 FPCGExValencyCageSpatialRegistry::GetCellKey(const FIntVector& Cell) const
{
	// Pack cell coordinates into a 64-bit key
	// This works for cells in range [-2097152, 2097151] per axis (21 bits each)
	const int64 X = (static_cast<int64>(Cell.X) + 2097152) & 0x1FFFFF;
	const int64 Y = (static_cast<int64>(Cell.Y) + 2097152) & 0x1FFFFF;
	const int64 Z = (static_cast<int64>(Cell.Z) + 2097152) & 0x1FFFFF;
	return (X << 42) | (Y << 21) | Z;
}

void FPCGExValencyCageSpatialRegistry::GetOverlappingCells(const FVector& Center, float Radius, TArray<FIntVector>& OutCells) const
{
	OutCells.Empty();

	const FVector MinBound = Center - FVector(Radius);
	const FVector MaxBound = Center + FVector(Radius);

	const FIntVector MinCell = PositionToCell(MinBound);
	const FIntVector MaxCell = PositionToCell(MaxBound);

	for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
	{
		for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
		{
			for (int32 Z = MinCell.Z; Z <= MaxCell.Z; ++Z)
			{
				OutCells.Add(FIntVector(X, Y, Z));
			}
		}
	}
}

void FPCGExValencyCageSpatialRegistry::RecalculateMaxProbeRadius()
{
	MaxProbeRadius = 0.0f;

	for (const TWeakObjectPtr<APCGExValencyCageBase>& CagePtr : AllCages)
	{
		if (const APCGExValencyCageBase* Cage = CagePtr.Get())
		{
			const float ProbeRadius = Cage->GetEffectiveProbeRadius();
			if (ProbeRadius > MaxProbeRadius)
			{
				MaxProbeRadius = ProbeRadius;
			}
		}
	}
}

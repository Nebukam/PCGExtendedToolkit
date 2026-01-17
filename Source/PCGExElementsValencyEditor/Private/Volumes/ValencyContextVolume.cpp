// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Volumes/ValencyContextVolume.h"

#include "EngineUtils.h"
#include "Components/BrushComponent.h"
#include "Cages/PCGExValencyCageBase.h"

AValencyContextVolume::AValencyContextVolume()
{
	// Configure as editor-only
	bNetLoadOnClient = false;

	// Set up brush component defaults
	if (UBrushComponent* BrushComp = GetBrushComponent())
	{
		BrushComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		BrushComp->SetMobility(EComponentMobility::Static);
	}
}

void AValencyContextVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	// Notify cages if relevant properties changed
	if (PropertyName == GET_MEMBER_NAME_CHECKED(AValencyContextVolume, BondingRules) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AValencyContextVolume, OrbitalSetOverride) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(AValencyContextVolume, DefaultProbeRadius))
	{
		NotifyContainedCages();
	}
}

void AValencyContextVolume::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (bFinished)
	{
		// Volume moved - cages may need to recalculate their containing volumes
		NotifyContainedCages();
	}
}

UPCGExValencyOrbitalSet* AValencyContextVolume::GetEffectiveOrbitalSet() const
{
	// Use override if set
	if (OrbitalSetOverride)
	{
		return OrbitalSetOverride;
	}

	// Otherwise use first orbital set from BondingRules
	if (BondingRules && BondingRules->OrbitalSets.Num() > 0)
	{
		return BondingRules->OrbitalSets[0];
	}

	return nullptr;
}

bool AValencyContextVolume::ContainsPoint(const FVector& WorldLocation, float Tolerance) const
{
	// Use volume's built-in EncompassesPoint
	float OutDistanceToPoint = 0.0f;
	return EncompassesPoint(WorldLocation, Tolerance, &OutDistanceToPoint);
}

void AValencyContextVolume::CollectContainedCages(TArray<APCGExValencyCageBase*>& OutCages) const
{
	OutCages.Empty();

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<APCGExValencyCageBase> It(World); It; ++It)
		{
			APCGExValencyCageBase* Cage = *It;
			if (Cage && ContainsPoint(Cage->GetActorLocation()))
			{
				OutCages.Add(Cage);
			}
		}
	}
}

void AValencyContextVolume::RebuildBondingRules()
{
	if (!BondingRules)
	{
		return;
	}

	TArray<APCGExValencyCageBase*> ContainedCages;
	CollectContainedCages(ContainedCages);

	// TODO: Implement rule extraction from cages
	// This will be implemented when the cage asset registration is complete

	// Recompile after changes
	BondingRules->Compile();
}

void AValencyContextVolume::NotifyContainedCages()
{
	TArray<APCGExValencyCageBase*> ContainedCages;
	CollectContainedCages(ContainedCages);

	for (APCGExValencyCageBase* Cage : ContainedCages)
	{
		if (Cage)
		{
			Cage->OnContainingVolumeChanged(this);
		}
	}
}

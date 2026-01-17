// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Volumes/ValencyContextVolume.h"

#include "EngineUtils.h"
#include "Components/BrushComponent.h"
#include "Cages/PCGExValencyCageBase.h"
#include "Builder/PCGExValencyBondingRulesBuilder.h"

DEFINE_LOG_CATEGORY_STATIC(LogValencyVolume, Log, All);

namespace PCGExValencyFolders
{
	const FName VolumesFolder = FName(TEXT("Valency/Volumes"));
}

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

void AValencyContextVolume::PostActorCreated()
{
	Super::PostActorCreated();

	// Auto-organize into Valency/Volumes folder
	SetFolderPath(PCGExValencyFolders::VolumesFolder);
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
	BuildRulesFromCages();
}

void AValencyContextVolume::BuildRulesFromCages()
{
	if (!BondingRules)
	{
		UE_LOG(LogValencyVolume, Error, TEXT("Cannot build rules: No BondingRules asset assigned to volume."));
		return;
	}

	UPCGExValencyBondingRulesBuilder* Builder = NewObject<UPCGExValencyBondingRulesBuilder>(GetTransientPackage());
	FPCGExValencyBuildResult Result = Builder->BuildFromVolume(this);

	// Log results
	if (Result.bSuccess)
	{
		UE_LOG(LogValencyVolume, Log, TEXT("Build succeeded: %d modules from %d cages."), Result.ModuleCount, Result.CageCount);
	}
	else
	{
		UE_LOG(LogValencyVolume, Error, TEXT("Build failed."));
	}

	for (const FText& Warning : Result.Warnings)
	{
		UE_LOG(LogValencyVolume, Warning, TEXT("%s"), *Warning.ToString());
	}

	for (const FText& Error : Result.Errors)
	{
		UE_LOG(LogValencyVolume, Error, TEXT("%s"), *Error.ToString());
	}
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

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

// Some of it Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGExManagedResource.h"

#include "PCGComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Engine/Level.h"
#include "Helpers/PCGHelpers.h"
#include "Paths/PCGExPaths.h"
#include "UObject/Package.h"

void UPCGExManagedSplineMeshComponent::ForgetComponent()
{
	Super::ForgetComponent();
	CachedRawComponentPtr = nullptr;
}

USplineMeshComponent* UPCGExManagedSplineMeshComponent::GetComponent() const
{
	if (!CachedRawComponentPtr)
	{
		USplineMeshComponent* GeneratedComponentPtr = Cast<USplineMeshComponent>(GeneratedComponent.Get());

		// Implementation note:
		// There is no surefire way to make sure that we can use the raw pointer UNLESS it is from the same owner
		if (GeneratedComponentPtr && Cast<UPCGComponent>(GetOuter()) && GeneratedComponentPtr->GetOwner() == Cast<UPCGComponent>(GetOuter())->GetOwner())
		{
			CachedRawComponentPtr = GeneratedComponentPtr;
		}

		return GeneratedComponentPtr;
	}

	return CachedRawComponentPtr;
}

void UPCGExManagedSplineMeshComponent::SetComponent(USplineMeshComponent* InComponent)
{
	GeneratedComponent = InComponent;
	CachedRawComponentPtr = InComponent;
}

void UPCGExManagedSplineMeshComponent::AttachTo(AActor* InTargetActor, const UPCGComponent* InSourceComponent) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExManagedSplineMeshComponent::AttachTo);

	check(CachedRawComponentPtr)

	bool bIsPreviewMode = false;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
	bIsPreviewMode = InSourceComponent->IsInPreviewMode();
#endif

	InTargetActor->Modify(!bIsPreviewMode);

	CachedRawComponentPtr->RegisterComponent();
	InTargetActor->AddInstanceComponent(CachedRawComponentPtr);
	CachedRawComponentPtr->AttachToComponent(InTargetActor->GetRootComponent(), FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));
}

USplineMeshComponent* UPCGExManagedSplineMeshComponent::CreateComponentOnly(AActor* InOuter, UPCGComponent* InSourceComponent, const PCGExPaths::FSplineMeshSegment& InParams)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExManagedSplineMeshComponent::CreateComponentOnly);

	if (!InParams.AssetStaging) { return nullptr; } // Booh

	bool bIsPreviewMode = false;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
	bIsPreviewMode = InSourceComponent->IsInPreviewMode();
#endif

	const FString ComponentName = TEXT("PCGSplineMeshComponent_") + InParams.AssetStaging->Path.GetAssetName();
	const EObjectFlags ObjectFlags = (bIsPreviewMode ? RF_Transient : RF_NoFlags);
	USplineMeshComponent* SplineMeshComponent = NewObject<USplineMeshComponent>(InOuter, MakeUniqueObjectName(InOuter, USplineMeshComponent::StaticClass(), FName(ComponentName)), ObjectFlags);

	SplineMeshComponent->ComponentTags.Add(InSourceComponent->GetFName());
	SplineMeshComponent->ComponentTags.Add(PCGHelpers::DefaultPCGTag);


	SplineMeshComponent->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	SplineMeshComponent->SetMobility(EComponentMobility::Static);
	SplineMeshComponent->SetSimulatePhysics(false);
	SplineMeshComponent->SetMassOverrideInKg(NAME_None, 0.0f);
	SplineMeshComponent->SetUseCCD(false);
	SplineMeshComponent->CanCharacterStepUpOn = ECB_No;
	SplineMeshComponent->bUseDefaultCollision = false;
	SplineMeshComponent->bNavigationRelevant = false;
	SplineMeshComponent->SetbNeverNeedsCookedCollisionData(true);

	InParams.ApplySettings(SplineMeshComponent); // Init Component

	return SplineMeshComponent;
}

UPCGExManagedSplineMeshComponent* UPCGExManagedSplineMeshComponent::RegisterAndAttachComponent(AActor* InOuter, USplineMeshComponent* InSMC, UPCGComponent* InSourceComponent, uint64 SettingsUID)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExManagedSplineMeshComponent::RegisterAndAttachComponent);

	UPCGExManagedSplineMeshComponent* Resource = PCGExManagedRessource::CreateResource<UPCGExManagedSplineMeshComponent>(InSourceComponent, SettingsUID);
	Resource->SetComponent(InSMC);
	//Resource->SetDescriptor(InParams.Descriptor);
	//Resource->SetSplineMeshParams(InParams.SplineMeshParams);

	Resource->AttachTo(InOuter, InSourceComponent);
	return Resource;
}

UPCGExManagedSplineMeshComponent* UPCGExManagedSplineMeshComponent::GetOrCreate(AActor* InOuter, UPCGComponent* InSourceComponent, uint64 SettingsUID, const PCGExPaths::FSplineMeshSegment& InParams, const bool bForceNew)
{
	check(InOuter && InSourceComponent);

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExManagedSplineMeshComponent::GetOrCreate);

	/*
	if (!bForceNew)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExManagedSplineMeshComponent::GetOrCreate::FindMatchingManagedSplineMeshComponent);

		UPCGExManagedSplineMeshComponent* MatchingResource = nullptr;
		InSourceComponent->ForEachManagedResource(
			[&MatchingResource, &InParams, &InTargetActor, SettingsUID](UPCGManagedResource* InResource)
			{
				// Early out if already found a match
				if (MatchingResource)
				{
					return;
				}

				if (UPCGExManagedSplineMeshComponent* Resource = Cast<UPCGExManagedSplineMeshComponent>(InResource))
				{
					if (Resource->GetSettingsUID() != SettingsUID || !Resource->CanBeUsed())
					{
						return;
					}

					if (USplineMeshComponent* SplineMeshComponent = Resource->GetComponent())
					{
						// TODO better check
						if (IsValid(SplineMeshComponent)
							&& SplineMeshComponent->GetOwner() == InTargetActor)
						//&& Resource->GetDescriptor() == InParams.Descriptor
						//&& Resource->GetSplineMeshParams() == InParams.SplineMeshParams)
						{
							MatchingResource = Resource;
						}
					}
				}
			});

		if (MatchingResource)
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(UPCGActorHelpers::GetOrCreateManagedSplineMeshComponent::MarkAsUsed);
			MatchingResource->MarkAsUsed();

			return MatchingResource;
		}
	}
	*/

	return RegisterAndAttachComponent(InOuter, CreateComponentOnly(InOuter, InSourceComponent, InParams), InSourceComponent, SettingsUID);
}

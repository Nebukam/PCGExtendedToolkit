// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
// Copyright Epic Games, Inc. All Rights Reserved.

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

UPCGExManagedSplineMeshComponent* UPCGExManagedSplineMeshComponent::GetOrCreate(
	AActor* InTargetActor,
	UPCGComponent* InSourceComponent,
	uint64 SettingsUID,
	const PCGExPaths::FSplineMeshSegment& InParams,
	const bool bForceNew)
{
	check(InTargetActor && InSourceComponent);

	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExManagedSplineMeshComponent::GetOrCreate);

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

	// No matching component found, let's create a new one.
	InTargetActor->Modify(!InSourceComponent->IsInPreviewMode());

	FString ComponentName = TEXT("PCGSplineMeshComponent_") + InParams.AssetStaging->Path.GetAssetName();
	const EObjectFlags ObjectFlags = (InSourceComponent->IsInPreviewMode() ? RF_Transient : RF_NoFlags);
	USplineMeshComponent* SplineMeshComponent = NewObject<USplineMeshComponent>(InTargetActor, MakeUniqueObjectName(InTargetActor, USplineMeshComponent::StaticClass(), FName(ComponentName)), ObjectFlags);

	SplineMeshComponent->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	SplineMeshComponent->SetMobility(EComponentMobility::Static);
	
	// Init Component
	InParams.ApplyToComponent(SplineMeshComponent);

	SplineMeshComponent->RegisterComponent();
	InTargetActor->AddInstanceComponent(SplineMeshComponent);

	SplineMeshComponent->AttachToComponent(InTargetActor->GetRootComponent(), FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));
	SplineMeshComponent->ComponentTags.Add(InSourceComponent->GetFName());
	SplineMeshComponent->ComponentTags.Add(PCGHelpers::DefaultPCGTag);

	// Create managed resource on source component
	UPCGExManagedSplineMeshComponent* Resource = NewObject<UPCGExManagedSplineMeshComponent>(InSourceComponent);
	Resource->SetComponent(SplineMeshComponent);
	//Resource->SetDescriptor(InParams.Descriptor);
	//Resource->SetSplineMeshParams(InParams.SplineMeshParams);
	Resource->SetSettingsUID(SettingsUID);
	InSourceComponent->AddToManagedResources(Resource);

	return Resource;
}

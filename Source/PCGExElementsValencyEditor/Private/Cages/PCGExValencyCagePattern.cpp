// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyCagePattern.h"

#include "EngineUtils.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Cages/PCGExValencyCage.h"
#include "Volumes/ValencyContextVolume.h"
#include "PCGExValencyEditorSettings.h"

APCGExValencyCagePattern::APCGExValencyCagePattern()
{
	// Create sphere for visualization and selection
	DebugSphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("DebugSphere"));
	DebugSphereComponent->SetupAttachment(RootComponent);
	DebugSphereComponent->SetSphereRadius(20.0f);
	DebugSphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DebugSphereComponent->SetLineThickness(2.0f);
	DebugSphereComponent->ShapeColor = FColor(100, 200, 255); // Blue-ish for pattern cages
	DebugSphereComponent->bHiddenInGame = true;

	// Create box component for pattern bounds (only visible on root)
	PatternBoundsComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("PatternBounds"));
	PatternBoundsComponent->SetupAttachment(RootComponent);
	PatternBoundsComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PatternBoundsComponent->SetLineThickness(1.5f);
	PatternBoundsComponent->ShapeColor = FColor(100, 200, 255, 128);
	PatternBoundsComponent->bHiddenInGame = true;
	PatternBoundsComponent->SetVisibility(false); // Hidden by default, shown only on root
}

void APCGExValencyCagePattern::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetMemberPropertyName();

	// Update bounds visualization when root status changes
	if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCagePattern, bIsPatternRoot))
	{
		UpdatePatternBoundsVisualization();
	}

	// Update ghost mesh when proxy settings change
	if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCagePattern, ProxiedCages) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCagePattern, bShowProxyGhostMesh) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCagePattern, bIsWildcard))
	{
		RefreshProxyGhostMesh();
	}

	// Update sphere color based on role
	if (DebugSphereComponent)
	{
		if (bIsWildcard)
		{
			DebugSphereComponent->ShapeColor = FColor(200, 200, 100); // Yellow for wildcard
		}
		else if (!bIsActiveInPattern)
		{
			DebugSphereComponent->ShapeColor = FColor(150, 150, 150); // Gray for constraint-only
		}
		else if (bIsPatternRoot)
		{
			DebugSphereComponent->ShapeColor = FColor(100, 255, 100); // Green for root
		}
		else
		{
			DebugSphereComponent->ShapeColor = FColor(100, 200, 255); // Blue for active
		}
	}
}

void APCGExValencyCagePattern::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (bFinished)
	{
		// Update pattern bounds if we're the root or if we need to notify the root
		if (bIsPatternRoot)
		{
			UpdatePatternBoundsVisualization();
		}
		else if (APCGExValencyCagePattern* Root = FindPatternRoot())
		{
			Root->UpdatePatternBoundsVisualization();
		}
	}
}

void APCGExValencyCagePattern::BeginDestroy()
{
	ClearProxyGhostMesh();
	Super::BeginDestroy();
}

FString APCGExValencyCagePattern::GetCageDisplayName() const
{
	FString Prefix;

	if (bIsPatternRoot)
	{
		Prefix = TEXT("PATTERN ROOT");
		if (!PatternSettings.PatternName.IsNone())
		{
			Prefix = FString::Printf(TEXT("PATTERN [%s]"), *PatternSettings.PatternName.ToString());
		}
	}
	else if (bIsWildcard)
	{
		Prefix = TEXT("PATTERN (*)");
	}
	else if (!bIsActiveInPattern)
	{
		Prefix = TEXT("PATTERN (constraint)");
	}
	else
	{
		Prefix = TEXT("PATTERN");
	}

	if (!CageName.IsEmpty())
	{
		return FString::Printf(TEXT("%s: %s"), *Prefix, *CageName);
	}

	return Prefix;
}

void APCGExValencyCagePattern::SetDebugComponentsVisible(bool bVisible)
{
	if (DebugSphereComponent)
	{
		DebugSphereComponent->SetVisibility(bVisible);
	}

	// Pattern bounds only visible on root
	if (PatternBoundsComponent)
	{
		PatternBoundsComponent->SetVisibility(bVisible && bIsPatternRoot);
	}
}

void APCGExValencyCagePattern::DetectNearbyConnections()
{
	// Call base class implementation (uses ShouldConsiderCageForConnection filter)
	Super::DetectNearbyConnections();

	// Notify pattern network that connections may have changed
	NotifyPatternNetworkChanged();
}

bool APCGExValencyCagePattern::ShouldConsiderCageForConnection(const APCGExValencyCageBase* CandidateCage) const
{
	// Pattern cages connect to other pattern cages and null cages (boundary markers)
	return CandidateCage && (CandidateCage->IsPatternCage() || CandidateCage->IsNullCage());
}

void APCGExValencyCagePattern::NotifyPatternNetworkChanged()
{
	// Find and notify the pattern root to update visualization
	if (APCGExValencyCagePattern* Root = FindPatternRoot())
	{
		Root->UpdatePatternBoundsVisualization();
	}
}

TArray<APCGExValencyCagePattern*> APCGExValencyCagePattern::GetConnectedPatternCages() const
{
	TArray<APCGExValencyCagePattern*> Connected;
	TSet<APCGExValencyCagePattern*> Visited;
	TArray<APCGExValencyCagePattern*> Stack;

	// Start from this cage
	Stack.Add(const_cast<APCGExValencyCagePattern*>(this));

	while (Stack.Num() > 0)
	{
		APCGExValencyCagePattern* Current = Stack.Pop();

		if (Visited.Contains(Current))
		{
			continue;
		}

		Visited.Add(Current);
		Connected.Add(Current);

		// Check orbital connections (auto + manual)
		for (const FPCGExValencyCageOrbital& Orbital : Current->Orbitals)
		{
			// Check auto-connected cage
			if (APCGExValencyCagePattern* AutoConnected = Cast<APCGExValencyCagePattern>(Orbital.AutoConnectedCage.Get()))
			{
				if (!Visited.Contains(AutoConnected))
				{
					Stack.Add(AutoConnected);
				}
			}

			// Check manual connections
			for (const TWeakObjectPtr<APCGExValencyCageBase>& ManualConnection : Orbital.ManualConnections)
			{
				if (APCGExValencyCagePattern* ManualConnected = Cast<APCGExValencyCagePattern>(ManualConnection.Get()))
				{
					if (!Visited.Contains(ManualConnected))
					{
						Stack.Add(ManualConnected);
					}
				}
			}
		}
	}

	return Connected;
}

APCGExValencyCagePattern* APCGExValencyCagePattern::FindPatternRoot() const
{
	TArray<APCGExValencyCagePattern*> Connected = GetConnectedPatternCages();

	for (APCGExValencyCagePattern* Cage : Connected)
	{
		if (Cage->bIsPatternRoot)
		{
			return Cage;
		}
	}

	// No root found - return nullptr
	return nullptr;
}

FBox APCGExValencyCagePattern::ComputePatternBounds() const
{
	FBox Bounds(ForceInit);

	TArray<APCGExValencyCagePattern*> Connected = GetConnectedPatternCages();

	for (const APCGExValencyCagePattern* Cage : Connected)
	{
		Bounds += Cage->GetActorLocation();
	}

	// Expand bounds slightly for visualization
	if (Bounds.IsValid)
	{
		Bounds = Bounds.ExpandBy(50.0f);
	}

	return Bounds;
}

void APCGExValencyCagePattern::UpdatePatternBoundsVisualization()
{
	if (!PatternBoundsComponent)
	{
		return;
	}

	if (!bIsPatternRoot)
	{
		PatternBoundsComponent->SetVisibility(false);
		return;
	}

	FBox Bounds = ComputePatternBounds();

	if (Bounds.IsValid)
	{
		// Position the box at the center of bounds, relative to this actor
		FVector BoundsCenter = Bounds.GetCenter();
		FVector BoundsExtent = Bounds.GetExtent();

		// Convert to local space
		FVector LocalCenter = BoundsCenter - GetActorLocation();

		PatternBoundsComponent->SetRelativeLocation(LocalCenter);
		PatternBoundsComponent->SetBoxExtent(BoundsExtent);
		PatternBoundsComponent->SetVisibility(true);
	}
	else
	{
		PatternBoundsComponent->SetVisibility(false);
	}
}

void APCGExValencyCagePattern::RefreshProxyGhostMesh()
{
	// Clear existing ghost mesh first
	ClearProxyGhostMesh();

	// Get settings
	const UPCGExValencyEditorSettings* Settings = UPCGExValencyEditorSettings::Get();

	// Early out if ghosting is disabled
	if (!Settings || !Settings->bEnableGhostMeshes || !bShowProxyGhostMesh || bIsWildcard || ProxiedCages.Num() == 0)
	{
		return;
	}

	// Get the shared ghost material from settings
	UMaterialInterface* GhostMaterial = Settings->GetGhostMaterial();

	// Find the first available mesh from proxied cages
	UStaticMesh* FirstMesh = nullptr;
	FTransform LocalTransform = FTransform::Identity;

	for (const TObjectPtr<APCGExValencyCage>& ProxiedCage : ProxiedCages)
	{
		if (!ProxiedCage)
		{
			continue;
		}

		// Get entries from the proxied cage
		TArray<FPCGExValencyAssetEntry> Entries = ProxiedCage->GetAllAssetEntries();

		for (const FPCGExValencyAssetEntry& Entry : Entries)
		{
			if (Entry.AssetType != EPCGExValencyAssetType::Mesh)
			{
				continue;
			}

			// Try to get the mesh
			UStaticMesh* Mesh = Cast<UStaticMesh>(Entry.Asset.Get());
			if (!Mesh)
			{
				// Try sync load (editor-only)
				Mesh = Cast<UStaticMesh>(Entry.Asset.LoadSynchronous());
			}

			if (Mesh)
			{
				FirstMesh = Mesh;
				LocalTransform = Entry.LocalTransform;
				break;
			}
		}

		if (FirstMesh)
		{
			break;
		}
	}

	if (!FirstMesh)
	{
		return;
	}

	// Create the ghost mesh component
	ProxyGhostMeshComponent = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
	ProxyGhostMeshComponent->SetStaticMesh(FirstMesh);
	ProxyGhostMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProxyGhostMeshComponent->SetCastShadow(false);
	ProxyGhostMeshComponent->bSelectable = false;

	// Apply ghost material to all slots
	if (GhostMaterial)
	{
		const int32 NumMaterials = FirstMesh->GetStaticMaterials().Num();
		for (int32 i = 0; i < NumMaterials; ++i)
		{
			ProxyGhostMeshComponent->SetMaterial(i, GhostMaterial);
		}
	}

	// Apply local transform (rotated by cage rotation)
	const FQuat CageRotation = GetActorQuat();
	const FVector RotatedLocation = CageRotation.RotateVector(LocalTransform.GetLocation());
	const FQuat RotatedRotation = CageRotation * LocalTransform.GetRotation();

	ProxyGhostMeshComponent->SetRelativeLocation(RotatedLocation);
	ProxyGhostMeshComponent->SetRelativeRotation(RotatedRotation.Rotator());
	ProxyGhostMeshComponent->SetRelativeScale3D(LocalTransform.GetScale3D());

	// Attach and register
	ProxyGhostMeshComponent->SetupAttachment(RootComponent);
	ProxyGhostMeshComponent->RegisterComponent();
}

void APCGExValencyCagePattern::ClearProxyGhostMesh()
{
	if (ProxyGhostMeshComponent)
	{
		ProxyGhostMeshComponent->DestroyComponent();
		ProxyGhostMeshComponent = nullptr;
	}
}

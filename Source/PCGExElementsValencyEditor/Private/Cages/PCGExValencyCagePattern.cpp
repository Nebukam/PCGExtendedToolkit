// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Cages/PCGExValencyCagePattern.h"

#include "EngineUtils.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Cages/PCGExValencyCage.h"
#include "Cages/PCGExValencyCageNull.h"
#include "Volumes/ValencyContextVolume.h"
#include "EditorMode/PCGExValencyCageEditorMode.h"
#include "PCGExValencyEditorSettings.h"
#include "Cages/PCGExValencyAssetPalette.h"

namespace PCGExValencyTags
{
	const FName GhostMeshTag = FName(TEXT("PCGEx_Valency_Ghost"));
}

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
		PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCagePattern, bShowProxyGhostMesh))
	{
		RefreshProxyGhostMesh();

		// Notify reference tracker when ProxiedCages changes (incrementally updates dependency graph)
		if (PropertyName == GET_MEMBER_NAME_CHECKED(APCGExValencyCagePattern, ProxiedCages))
		{
			if (FValencyReferenceTracker* Tracker = FPCGExValencyCageEditorMode::GetActiveReferenceTracker())
			{
				Tracker->OnProxiedCagesChanged(this);
			}
		}
	}

	// Update sphere color based on role
	// Pattern cage is visually "wildcard" if ProxiedCages is empty (matches any module)
	if (DebugSphereComponent)
	{
		if (ProxiedCages.IsEmpty())
		{
			DebugSphereComponent->ShapeColor = FColor(200, 200, 100); // Yellow for wildcard (no proxied cages = matches any)
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

	// Check if any property in the chain has PCGEX_ValencyRebuild metadata
	bool bShouldRebuild = false;

	if (const FProperty* Property = PropertyChangedEvent.Property)
	{
		if (Property->HasMetaData(TEXT("PCGEX_ValencyRebuild")))
		{
			bShouldRebuild = true;
		}
	}

	if (!bShouldRebuild && PropertyChangedEvent.MemberProperty)
	{
		if (PropertyChangedEvent.MemberProperty->HasMetaData(TEXT("PCGEX_ValencyRebuild")))
		{
			bShouldRebuild = true;
		}
	}

	// Debounce interactive changes (dragging sliders)
	if (bShouldRebuild && !UPCGExValencyEditorSettings::ShouldAllowRebuild(PropertyChangedEvent.ChangeType))
	{
		bShouldRebuild = false;
	}

	if (bShouldRebuild)
	{
		RequestRebuild(EValencyRebuildReason::PropertyChange);
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

void APCGExValencyCagePattern::PostDuplicate(EDuplicateMode::Type DuplicateMode)
{
	Super::PostDuplicate(DuplicateMode);

	// Clear any orphaned ghost meshes and refresh
	ClearProxyGhostMesh();
	RefreshProxyGhostMesh();
}

void APCGExValencyCagePattern::BeginDestroy()
{
	ClearProxyGhostMesh();
	Super::BeginDestroy();
}

FString APCGExValencyCagePattern::GetCageDisplayName() const
{
	FString Prefix;

	// Pattern cage is "wildcard" if ProxiedCages is empty (matches any module)
	const bool bIsVisualWildcard = ProxiedCages.IsEmpty();

	if (bIsPatternRoot)
	{
		Prefix = TEXT("PATTERN ROOT");
		if (!PatternSettings.PatternName.IsNone())
		{
			Prefix = FString::Printf(TEXT("PATTERN [%s]"), *PatternSettings.PatternName.ToString());
		}
	}
	else if (bIsVisualWildcard)
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

bool APCGExValencyCagePattern::DetectNearbyConnections()
{
	// Call base class implementation (uses ShouldConsiderCageForConnection filter)
	const bool bChanged = Super::DetectNearbyConnections();

	// Notify pattern network that connections may have changed
	if (bChanged)
	{
		NotifyPatternNetworkChanged();
	}

	return bChanged;
}

bool APCGExValencyCagePattern::ShouldConsiderCageForConnection(const APCGExValencyCageBase* CandidateCage) const
{
	if (!CandidateCage)
	{
		return false;
	}

	// Connect to other pattern cages
	if (CandidateCage->IsPatternCage())
	{
		return true;
	}

	// Connect to null cages that are participating in patterns
	// Non-participating null cages act as passive markers only
	if (CandidateCage->IsNullCage())
	{
		if (const APCGExValencyCageNull* NullCage = Cast<APCGExValencyCageNull>(CandidateCage))
		{
			return NullCage->IsParticipatingInPatterns();
		}
		// Fallback: Legacy null cages without participation - accept them for backward compat
		return true;
	}

	return false;
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

	// Include all pattern cages
	for (const APCGExValencyCagePattern* Cage : Connected)
	{
		Bounds += Cage->GetActorLocation();

		// Also include connected null cages
		for (const FPCGExValencyCageOrbital& Orbital : Cage->Orbitals)
		{
			// Check auto-connected null cage
			if (APCGExValencyCageBase* AutoConnected = Orbital.AutoConnectedCage.Get())
			{
				if (AutoConnected->IsNullCage())
				{
					Bounds += AutoConnected->GetActorLocation();
				}
			}

			// Check manual connections for null cages
			for (const TWeakObjectPtr<APCGExValencyCageBase>& ManualConnection : Orbital.ManualConnections)
			{
				if (APCGExValencyCageBase* ManualConnected = ManualConnection.Get())
				{
					if (ManualConnected->IsNullCage())
					{
						Bounds += ManualConnected->GetActorLocation();
					}
				}
			}
		}
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
	// Clear existing ghost meshes first
	ClearProxyGhostMesh();

	// Get settings
	const UPCGExValencyEditorSettings* Settings = UPCGExValencyEditorSettings::Get();

	// Early out if ghosting is disabled or no proxied cages (wildcard pattern cage)
	if (!Settings || !Settings->bEnableGhostMeshes || !bShowProxyGhostMesh || ProxiedCages.IsEmpty() || Settings->MaxPatternGhostMeshes == 0)
	{
		return;
	}

	// Get the shared ghost material from settings
	UMaterialInterface* GhostMaterial = Settings->GetGhostMaterial();
	const FQuat CageRotation = GetActorQuat();

	// Get the limit (-1 = unlimited)
	const int32 MaxGhosts = Settings->MaxPatternGhostMeshes;
	int32 GhostCount = 0;

	// Collect all entries from proxied cages (including their mirror sources)
	TArray<FPCGExValencyAssetEntry> AllEntries;
	TSet<AActor*> VisitedSources;

	// Lambda to collect entries from a source (with recursion for mirror sources)
	TFunction<void(AActor*)> CollectFromSource = [&](AActor* Source)
	{
		if (!Source || VisitedSources.Contains(Source))
		{
			return;
		}
		VisitedSources.Add(Source);

		if (APCGExValencyCage* SourceCage = Cast<APCGExValencyCage>(Source))
		{
			AllEntries.Append(SourceCage->GetAllAssetEntries());

			// Recursively collect from cage's mirror sources
			for (const TObjectPtr<AActor>& MirrorSource : SourceCage->MirrorSources)
			{
				CollectFromSource(MirrorSource);
			}
		}
		else if (APCGExValencyAssetPalette* SourcePalette = Cast<APCGExValencyAssetPalette>(Source))
		{
			AllEntries.Append(SourcePalette->GetAllAssetEntries());
		}
	};

	// Collect from all proxied cages and their mirror sources
	for (const TObjectPtr<APCGExValencyCage>& ProxiedCage : ProxiedCages)
	{
		CollectFromSource(ProxiedCage.Get());
	}

	for (const FPCGExValencyAssetEntry& Entry : AllEntries)
	{
		// Check if we've hit the limit
		if (MaxGhosts >= 0 && GhostCount >= MaxGhosts)
		{
			break;
		}

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

		if (!Mesh)
		{
			continue;
		}

		// Create the ghost mesh component
		UStaticMeshComponent* GhostComp = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
		GhostComp->ComponentTags.Add(PCGExValencyTags::GhostMeshTag);
		GhostComp->SetStaticMesh(Mesh);
		GhostComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GhostComp->SetCastShadow(false);
		GhostComp->bSelectable = false;

		// Apply ghost material to all slots
		if (GhostMaterial)
		{
			const int32 NumMaterials = Mesh->GetStaticMaterials().Num();
			for (int32 i = 0; i < NumMaterials; ++i)
			{
				GhostComp->SetMaterial(i, GhostMaterial);
			}
		}

		// Apply local transform (rotated by cage rotation)
		const FVector RotatedLocation = CageRotation.RotateVector(Entry.LocalTransform.GetLocation());
		const FQuat RotatedRotation = CageRotation * Entry.LocalTransform.GetRotation();

		GhostComp->SetRelativeLocation(RotatedLocation);
		GhostComp->SetRelativeRotation(RotatedRotation.Rotator());
		GhostComp->SetRelativeScale3D(Entry.LocalTransform.GetScale3D());

		// Attach and register
		GhostComp->SetupAttachment(RootComponent);
		GhostComp->RegisterComponent();

		GhostCount++;
	}
}

void APCGExValencyCagePattern::ClearProxyGhostMesh()
{
	// Find and destroy all components with the ghost mesh tag
	TArray<UActorComponent*> TaggedComponents;
	GetComponents(TaggedComponents);

	for (UActorComponent* Component : TaggedComponents)
	{
		if (Component && Component->ComponentHasTag(PCGExValencyTags::GhostMeshTag))
		{
			Component->DestroyComponent();
		}
	}
}

// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Components/PCGExValencyCageSocketComponent.h"

#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Core/PCGExValencySocketRules.h"
#include "Cages/PCGExValencyCageBase.h"

UPCGExValencyCageSocketComponent::UPCGExValencyCageSocketComponent()
{
	// Socket components are attached to cages, should be movable
	Mobility = EComponentMobility::Movable;

	// Default to hidden in game - sockets are editor-only visualization
	bHiddenInGame = true;

	// Socket components don't need to tick
	PrimaryComponentTick.bCanEverTick = false;
}

void UPCGExValencyCageSocketComponent::OnRegister()
{
	Super::OnRegister();

	// Generate a default name if none is set
	if (SocketName.IsNone())
	{
		GenerateDefaultSocketName();
	}
}

#if WITH_EDITOR
void UPCGExValencyCageSocketComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	// Properties that affect compilation - request rebuild
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UPCGExValencyCageSocketComponent, SocketType) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UPCGExValencyCageSocketComponent, bIsOutputSocket) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UPCGExValencyCageSocketComponent, bEnabled))
	{
		RequestCageRebuild();
	}
}
#endif

FLinearColor UPCGExValencyCageSocketComponent::GetEffectiveDebugColor(const UPCGExValencySocketRules* SocketRules) const
{
	// Use override if set (non-zero alpha indicates intentional color)
	if (DebugColorOverride.A > 0.0f)
	{
		return DebugColorOverride;
	}

	// Try to get color from socket rules
	if (SocketRules)
	{
		const int32 TypeIndex = SocketRules->FindSocketTypeIndex(SocketType);
		if (SocketRules->SocketTypes.IsValidIndex(TypeIndex))
		{
			return SocketRules->SocketTypes[TypeIndex].DebugColor;
		}
	}

	// Default to white
	return FLinearColor::White;
}

bool UPCGExValencyCageSocketComponent::SyncTransformFromMeshSocket(UStaticMesh* Mesh)
{
	if (!Mesh || MeshSocketName.IsNone())
	{
		return false;
	}

	// Find the mesh socket
	const UStaticMeshSocket* MeshSocket = Mesh->FindSocket(MeshSocketName);
	if (!MeshSocket)
	{
		return false;
	}

	// Apply the mesh socket's transform to this component
	const FTransform SocketTransform(
		MeshSocket->RelativeRotation,
		MeshSocket->RelativeLocation,
		MeshSocket->RelativeScale
	);

	SetRelativeTransform(SocketTransform);
	return true;
}

void UPCGExValencyCageSocketComponent::GenerateDefaultSocketName()
{
	// Generate a name based on owning actor and component count
	if (AActor* Owner = GetOwner())
	{
		TArray<UPCGExValencyCageSocketComponent*> ExistingComponents;
		Owner->GetComponents<UPCGExValencyCageSocketComponent>(ExistingComponents);

		// Find the next available index
		int32 NextIndex = 0;
		TSet<FName> ExistingNames;
		for (const UPCGExValencyCageSocketComponent* Comp : ExistingComponents)
		{
			if (Comp && Comp != this)
			{
				ExistingNames.Add(Comp->SocketName);
			}
		}

		// Generate a unique name
		FName CandidateName;
		do
		{
			CandidateName = FName(*FString::Printf(TEXT("Socket_%d"), NextIndex++));
		}
		while (ExistingNames.Contains(CandidateName));

		SocketName = CandidateName;
	}
	else
	{
		SocketName = FName("Socket_0");
	}
}

void UPCGExValencyCageSocketComponent::RequestCageRebuild()
{
	// Get owning cage and request rebuild
	if (APCGExValencyCageBase* Cage = Cast<APCGExValencyCageBase>(GetOwner()))
	{
		Cage->RequestRebuild(EValencyRebuildReason::AssetChange);
	}
}

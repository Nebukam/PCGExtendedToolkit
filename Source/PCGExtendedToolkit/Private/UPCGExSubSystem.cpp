// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "UPCGExSubSystem.h"

#include "Components/ActorComponent.h"
#include "PCGComponent.h"
#include "Data/PCGExSharedData.h"

void UPCGExSubSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UPCGExSubSystem::Deinitialize()
{
	Super::Deinitialize();
}

void UPCGExSubSystem::HandleSharedPCGComponentDeactivated(UActorComponent* Component)
{
	uint32 UID = Component->GetUniqueID();
	const TSharedPtr<PCGExData::FSharedPCGComponent>* SharedPCGComponent = nullptr;
	{
		FReadScopeLock ReadScopeLock(SharedPCGComponentLock);
		SharedPCGComponent = SharedPCGComponents.Find(UID);
	}

	if (SharedPCGComponent) { ReleaseSharedPCGComponent(*SharedPCGComponent); }
}

void UPCGExSubSystem::Dispatch(UPCGComponent* InComponent, TArray<FPCGTaggedData> TaggedData, PCGEx::FPCGExEvent Event)
{
}

bool UPCGExSubSystem::AddListener(PCGEx::FPCGExEvent Event, EventCallback&& InCallback)
{
	// Returns true if event data is readily available.
	return false;
}

void UPCGExSubSystem::RemoveListener(PCGEx::FPCGExEvent Event, EventCallback&& InCallback)
{
}

TSharedPtr<PCGExData::FSharedPCGComponent> UPCGExSubSystem::GetOrCreateSharedPCGComponent(UPCGComponent* InComponent)
{
	uint32 UID = InComponent->GetUniqueID();

	{
		FReadScopeLock ReadScopeLock(SharedPCGComponentLock);
		if (TSharedPtr<PCGExData::FSharedPCGComponent>* SharedPCGComponent = SharedPCGComponents.Find(UID)) { return *SharedPCGComponent; }
	}

	{
		FWriteScopeLock WriteScopeLock(SharedPCGComponentLock);
		TSharedPtr<PCGExData::FSharedPCGComponent> NewSharedPCGComponent = MakeShared<PCGExData::FSharedPCGComponent>(InComponent);
		SharedPCGComponents.Add(UID, NewSharedPCGComponent);

		InComponent->OnComponentDeactivated.AddDynamic(this, &UPCGExSubSystem::HandleSharedPCGComponentDeactivated);

		return NewSharedPCGComponent;
	}
}

void UPCGExSubSystem::ReleaseSharedPCGComponent(const TSharedPtr<PCGExData::FSharedPCGComponent>& InSharedPCGComponent)
{
	FWriteScopeLock WriteScopeLock(SharedPCGComponentLock);
	SharedPCGComponents.Remove(InSharedPCGComponent->GetUID());
	InSharedPCGComponent->Release();
}

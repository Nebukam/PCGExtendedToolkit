// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UPCGExSubSystem.generated.h"

struct FPCGTaggedData;
class UActorComponent;
class UPCGComponent;

namespace PCGExData
{
	class FSharedPCGComponent;
}

UENUM()
enum class EPCGExEventScope : uint8
{
	None   = 0 UMETA(Hidden),
	Owner  = 1 UMETA(DisplayName = "Owner", ToolTip="Event is dispatched on the PCG component owner only"),
	Global = 2 UMETA(DisplayName = "Global", ToolTip="Event is dispatched globally"),
};

namespace PCGEx
{
	struct FPCGExEvent
	{
		EPCGExEventScope Scope = EPCGExEventScope::None;
		FName Name = NAME_None;
		const AActor* Owner = nullptr;

		FPCGExEvent() = default;

		FPCGExEvent(const EPCGExEventScope InScope, const FName InName)
			: Scope(InScope), Name(InName)
		{
		}

		FPCGExEvent(const EPCGExEventScope InScope, const FName InName, const AActor* InOwner)
			: Scope(InScope), Name(InName), Owner(InOwner)
		{
		}

		bool operator==(const FPCGExEvent& Other) const { return Scope == Other.Scope && Name == Other.Name && Owner == Other.Owner; }
		FORCEINLINE friend uint32 GetTypeHash(const FPCGExEvent& Key) { return HashCombineFast(HashCombineFast(static_cast<uint32>(Key.Scope), GetTypeHash(Key.Name)), Key.Owner ? Key.Owner->GetUniqueID() : 0); }
	};
}

UCLASS()
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExSubSystem : public UWorldSubsystem
{
	GENERATED_BODY()

private:
	FRWLock SharedPCGComponentLock;

public:
	using EventCallback = std::function<void()>;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION()
	void HandleSharedPCGComponentDeactivated(UActorComponent* Component);

	void Dispatch(UPCGComponent* InComponent, TArray<FPCGTaggedData> TaggedData, PCGEx::FPCGExEvent Event);

	bool AddListener(PCGEx::FPCGExEvent Event, EventCallback&& InCallback);
	void RemoveListener(PCGEx::FPCGExEvent Event, EventCallback&& InCallback);

	TSharedPtr<PCGExData::FSharedPCGComponent> GetOrCreateSharedPCGComponent(UPCGComponent* InComponent);
	void ReleaseSharedPCGComponent(const TSharedPtr<PCGExData::FSharedPCGComponent>& InSharedPCGComponent);

protected:
	TMap<uint32, TSharedPtr<PCGExData::FSharedPCGComponent>> SharedPCGComponents;
	TMultiMap<PCGEx::FPCGExEvent, EventCallback> Listeners;
};

// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "UPCGExSubSystem.generated.h"

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
};

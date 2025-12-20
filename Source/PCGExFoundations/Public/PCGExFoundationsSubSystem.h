// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "PCGExFoundationsSubSystem.generated.h"

#define PCGEX_FOUNDATIONS_SUBSYSTEM UPCGExFoundationsSubSystem* PCGExFoundationSubsystem = UPCGExFoundationsSubSystem::GetSubsystemForCurrentWorld(); check(PCGExFoundationSubsystem)

class UPCGComponent;
class UPCGExConstantFilterFactory;

namespace PCGExPointFilter
{
	class IFilter;
}

UCLASS()
class PCGEXFOUNDATIONS_API UPCGExFoundationsSubSystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UPCGExFoundationsSubSystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** To be used when a PCG component can not have a world anymore, to unregister itself. */
	static UPCGExFoundationsSubSystem* GetSubsystemForCurrentWorld();

	/** Will return the subsystem from the World if it exists and if it is initialized */
	static UPCGExFoundationsSubSystem* GetInstance(UWorld* World);

	TSharedPtr<PCGExPointFilter::IFilter> GetConstantFilter(const bool bValue) const;

protected:
	UPROPERTY()
	TObjectPtr<UPCGExConstantFilterFactory> ConstantFilterFactory_TRUE;

	UPROPERTY()
	TObjectPtr<UPCGExConstantFilterFactory> ConstantFilterFactory_FALSE;
};

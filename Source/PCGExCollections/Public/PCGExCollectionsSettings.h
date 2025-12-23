// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "PCGExCollectionsSettings.generated.h"

class UPCGPin;

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="PCGEx | Collections", Description="Configure PCG Extended Toolkit' Collections module settings"))
class PCGEXCOLLECTIONS_API UPCGExCollectionsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

public:
	/** Disable collision on new entries */
	UPROPERTY(EditAnywhere, config, Category = "Collections")
	bool bDisableCollisionByDefault = true;

	void UpdateSettingsCaches() const;
};

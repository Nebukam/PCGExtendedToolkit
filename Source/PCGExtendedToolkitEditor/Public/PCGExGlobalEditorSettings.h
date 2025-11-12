// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "Engine/EngineTypes.h"
#include "Engine/DeveloperSettings.h"

#include "PCGExGlobalEditorSettings.generated.h"

UCLASS(Config=Editor, DefaultConfig, meta=(DisplayName="PCGEx - Editor", Description="PCGEx Editor Settings"))
class PCGEXTENDEDTOOLKITEDITOR_API UPCGExGlobalEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static FSimpleMulticastDelegate OnHiddenAssetPropertyNamesChanged;

	/** Map a property internal name to a property name, so multiple property visibility can be toggled by a single flag */
	TMap<FName, FName> PropertyNamesMap;
	
	UPROPERTY(Config, EditAnywhere, Category=Settings, meta=(EditCondition=false, EditConditionHides))
	TSet<FName> HiddenPropertyNames;
	
	void ToggleHiddenAssetPropertyName(const FName PropertyName, const bool bHide);
	void ToggleHiddenAssetPropertyName(const TArray<FName> Properties, const bool bHide);
	EVisibility GetPropertyVisibility(const FName PropertyName) const;
	
	bool GetIsPropertyVisible(const FName PropertyName) const;
	
};

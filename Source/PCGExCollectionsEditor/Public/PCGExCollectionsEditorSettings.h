// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Engine/DeveloperSettings.h"

#include "PCGExCollectionsEditorSettings.generated.h"

UCLASS(Config=EditorUser, DefaultConfig, meta=(DisplayName="PCGEx - Editor", Description="PCGEx Editor Settings"))
class PCGEXCOLLECTIONSEDITOR_API UPCGExCollectionsEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual void PostLoad() override;

	virtual FName GetContainerName() const override { return "Editor"; }
	virtual FName GetCategoryName() const override { return "Plugins"; }
	virtual FName GetSectionName() const override { return "PCGEx"; }

	static FSimpleMulticastDelegate OnHiddenAssetPropertyNamesChanged;

	/** Map a property internal name to a property name, so multiple property visibility can be toggled by a single flag */
	TMap<FName, FName> PropertyNamesMap;

	UPROPERTY(Config)
	TSet<FName> HiddenPropertyNames;

	void ToggleHiddenAssetPropertyName(const FName PropertyName, const bool bHide);
	void ToggleHiddenAssetPropertyName(const TArray<FName> Properties, const bool bHide);
	EVisibility GetPropertyVisibility(const FName PropertyName) const;

	bool GetIsPropertyVisible(const FName PropertyName) const;

protected:
	/** Internal version tracking. */
	UPROPERTY(EditAnywhere, config, Category = Settings, AdvancedDisplay)
	int64 PCGExDataVersion = -1;
};

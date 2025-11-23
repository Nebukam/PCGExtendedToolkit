// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExGlobalEditorSettings.h"

#include "CoreMinimal.h"
#include "Details/PCGExVersion.h"

FSimpleMulticastDelegate UPCGExGlobalEditorSettings::OnHiddenAssetPropertyNamesChanged;

void UPCGExGlobalEditorSettings::PostLoad()
{
	Super::PostLoad();
	/*
#if WITH_EDITOR
	bool bWantsSaving = false;
	PCGEX_UPDATE_TO_DATA_VERSION(1, 71, 2)
	{
		HiddenPropertyNames.Add(FName("AssetEditor.Descriptors"));
		bWantsSaving = true;
	}

	if (bWantsSaving) { SaveConfig(); }
#endif
	*/
}

void UPCGExGlobalEditorSettings::ToggleHiddenAssetPropertyName(const FName PropertyName, const bool bHide)
{
	if (bHide)
	{
		bool bIsAlreadyInSet = false;
		HiddenPropertyNames.Add(PropertyName, &bIsAlreadyInSet);
		if (bIsAlreadyInSet) { return; }
	}
	else if (!HiddenPropertyNames.Remove(PropertyName))
	{
		return;
	}

	SaveConfig();
	OnHiddenAssetPropertyNamesChanged.Broadcast();
}

void UPCGExGlobalEditorSettings::ToggleHiddenAssetPropertyName(const TArray<FName> Properties, const bool bHide)
{
	if (Properties.IsEmpty()) { return; }
	bool bAnyChanges = false;
	if (bHide)
	{
		for (FName PropertyName : Properties)
		{
			bool bIsAlreadyInSet = false;
			HiddenPropertyNames.Add(PropertyName, &bIsAlreadyInSet);
			if (!bIsAlreadyInSet) { bAnyChanges = true; }
		}
	}
	else
	{
		for (FName PropertyName : Properties)
		{
			if (HiddenPropertyNames.Remove(PropertyName)) { bAnyChanges = true; }
		}
	}

	if (!bAnyChanges) { return; }

	SaveConfig();
	OnHiddenAssetPropertyNamesChanged.Broadcast();
}

EVisibility UPCGExGlobalEditorSettings::GetPropertyVisibility(const FName PropertyName) const
{
	const FName* Id = PropertyNamesMap.Find(PropertyName);
	return Id ? HiddenPropertyNames.Contains(*Id) ? EVisibility::Collapsed : EVisibility::Visible : EVisibility::Visible;
}

bool UPCGExGlobalEditorSettings::GetIsPropertyVisible(const FName PropertyName) const
{
	return !HiddenPropertyNames.Contains(PropertyName);
}

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExGlobalEditorSettings.h"

#include "CoreMinimal.h"
#include "Components/SlateWrapperTypes.h"

FSimpleMulticastDelegate UPCGExGlobalEditorSettings::OnHiddenAssetPropertyNamesChanged;

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

EVisibility UPCGExGlobalEditorSettings::GetPropertyVisibility(const FName PropertyName) const
{
	const FName* Id = PropertyNamesMap.Find(PropertyName);
	return Id ? HiddenPropertyNames.Contains(*Id) ? EVisibility::Collapsed : EVisibility::Visible : EVisibility::Visible;
}

bool UPCGExGlobalEditorSettings::GetIsPropertyVisible(const FName PropertyName) const
{
	return !HiddenPropertyNames.Contains(PropertyName);
}

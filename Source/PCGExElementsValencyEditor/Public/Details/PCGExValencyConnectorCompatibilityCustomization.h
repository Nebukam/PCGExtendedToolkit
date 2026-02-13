// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/SCompoundWidget.h"


class SVerticalBox;
class UPCGExValencyConnectorSet;

/**
 * Customization for FPCGExValencyConnectorEntry.
 * Displays CompatibleTypeIds as a dropdown multi-select showing connector type names.
 */
class FPCGExValencyConnectorEntryCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> PropertyHandle,
		class FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> PropertyHandle,
		class IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
	UPCGExValencyConnectorSet* GetOuterConnectorSet(TSharedRef<IPropertyHandle> PropertyHandle) const;

	TSharedRef<SWidget> BuildCompatibilityDropdown(
		TSharedPtr<IPropertyHandle> CompatibleTypeIdsHandle,
		UPCGExValencyConnectorSet* ConnectorSet,
		int32 CurrentTypeId);

	FText GetCompatibilitySummary(
		TSharedPtr<IPropertyHandle> CompatibleTypeIdsHandle,
		UPCGExValencyConnectorSet* ConnectorSet) const;
};

/**
 * Widget for the compatibility dropdown menu content.
 * Shows checkboxes for each connector type with search filtering.
 */
class SValencyConnectorCompatibilityDropdown : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SValencyConnectorCompatibilityDropdown) {}
		SLATE_ARGUMENT(TSharedPtr<IPropertyHandle>, CompatibleTypeIdsHandle)
		SLATE_ARGUMENT(UPCGExValencyConnectorSet*, ConnectorSet)
		SLATE_ARGUMENT(int32, CurrentTypeId)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void RebuildCheckboxList();
	void OnSearchTextChanged(const FText& NewText);
	bool IsTypeCompatible(int32 TypeId) const;
	bool DoesTypeConnectToUs(int32 OtherTypeId) const;
	void ToggleTypeCompatibility(int32 TypeId);
	void OnSelectAll();
	void OnClearAll();
	void OnSelfOnly();

	TSharedPtr<IPropertyHandle> CompatibleTypeIdsHandle;
	TWeakObjectPtr<UPCGExValencyConnectorSet> ConnectorSetWeak;
	int32 CurrentTypeId = 0;
	FString SearchFilter;

	TSharedPtr<SVerticalBox> CheckboxContainer;
};

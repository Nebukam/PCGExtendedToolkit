// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/SCompoundWidget.h"

class UPCGExSocketRules;

/**
 * Customization for FPCGExSocketDefinition.
 * Displays CompatibleTypeIds as a dropdown multi-select showing socket type names.
 */
class FPCGExSocketDefinitionCustomization : public IPropertyTypeCustomization
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
	/** Get the owning UPCGExSocketRules from the property handle */
	UPCGExSocketRules* GetOuterSocketRules(TSharedRef<IPropertyHandle> PropertyHandle) const;

	/** Build the compatibility dropdown widget */
	TSharedRef<SWidget> BuildCompatibilityDropdown(
		TSharedPtr<IPropertyHandle> CompatibleTypeIdsHandle,
		UPCGExSocketRules* SocketRules,
		int32 CurrentTypeId);

	/** Get summary text for the dropdown button */
	FText GetCompatibilitySummary(
		TSharedPtr<IPropertyHandle> CompatibleTypeIdsHandle,
		UPCGExSocketRules* SocketRules) const;
};

/**
 * Widget for the compatibility dropdown menu content.
 * Shows checkboxes for each socket type with search filtering.
 */
class SSocketCompatibilityDropdown : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSocketCompatibilityDropdown) {}
		SLATE_ARGUMENT(TSharedPtr<IPropertyHandle>, CompatibleTypeIdsHandle)
		SLATE_ARGUMENT(UPCGExSocketRules*, SocketRules)
		SLATE_ARGUMENT(int32, CurrentTypeId)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	/** Rebuild the checkbox list based on current filter */
	void RebuildCheckboxList();

	/** Handle search text change */
	void OnSearchTextChanged(const FText& NewText);

	/** Check if a type ID is in the compatible list */
	bool IsTypeCompatible(int32 TypeId) const;

	/** Toggle compatibility for a type */
	void ToggleTypeCompatibility(int32 TypeId);

	/** Select all types */
	void OnSelectAll();

	/** Clear all types */
	void OnClearAll();

	/** Make self-compatible (only this type) */
	void OnSelfOnly();

	TSharedPtr<IPropertyHandle> CompatibleTypeIdsHandle;
	TWeakObjectPtr<UPCGExSocketRules> SocketRulesWeak;
	int32 CurrentTypeId = 0;
	FString SearchFilter;

	TSharedPtr<SVerticalBox> CheckboxContainer;
};

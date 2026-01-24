// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "IPropertyTypeCustomization.h"

class IPropertyUtilities;

/**
 * Customizes FPCGExPropertyOverrides to show toggle checkboxes for each property.
 *
 * The Overrides array is kept parallel with schema by SyncToSchema().
 * Each entry has bEnabled to toggle override on/off.
 *
 * Display:
 * - Each property shown with checkbox bound to bEnabled
 * - Enabled = shows value widget (editable)
 * - Disabled = shows value widget (grayed out, uses collection default)
 */
class FPCGExPropertyOverridesCustomization : public IPropertyTypeCustomization
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
	FText GetHeaderText() const;

	TWeakPtr<IPropertyUtilities> WeakPropertyUtilities;
	TWeakPtr<IPropertyHandle> PropertyHandlePtr;
};

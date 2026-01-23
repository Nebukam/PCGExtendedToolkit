// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "IPropertyTypeCustomization.h"

class IPropertyUtilities;

/**
 * Customizes FPCGExPropertySchemaCollection to:
 * - Show dynamic header with schema count
 * - Trigger refresh when schemas change (add/remove/reorder/type change)
 * - Sync PropertyName and HeaderId when array changes
 */
class FPCGExPropertySchemaCollectionCustomization : public IPropertyTypeCustomization
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

	/** Called when Schemas array changes - syncs PropertyNames and broadcasts refresh */
	void OnSchemasArrayChanged();

	TWeakPtr<IPropertyUtilities> WeakPropertyUtilities;
	TWeakPtr<IPropertyHandle> PropertyHandlePtr;
	TWeakPtr<IPropertyHandle> SchemasArrayHandlePtr;
};

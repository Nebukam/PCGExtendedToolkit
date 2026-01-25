// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "IPropertyTypeCustomization.h"

/**
 * Customizes FPCGExPropertySchema to:
 * - Show dynamic header with Name and type
 * - Sync PropertyName and HeaderId when Name or Property changes
 * - When under a property with ReadOnlySchema metadata:
 *   - Hides Name field and struct type picker (schema is synced from cage)
 *   - Only allows editing the inner Value field (the default value)
 */
class FPCGExPropertySchemaCustomization : public IPropertyTypeCustomization
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

	/** Called when Name or Property changes - syncs PropertyName/HeaderId */
	void OnSchemaChanged();

	/** Check if this schema is under a property with ReadOnlySchema metadata */
	bool IsReadOnlySchema(TSharedRef<IPropertyHandle> PropertyHandle) const;

	TWeakPtr<IPropertyHandle> PropertyHandlePtr;
	bool bIsReadOnly = false;
};

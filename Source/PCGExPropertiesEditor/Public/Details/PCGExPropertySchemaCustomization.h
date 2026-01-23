// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "IPropertyTypeCustomization.h"

/**
 * Customizes FPCGExPropertySchema to:
 * - Show dynamic header with Name and type
 * - Sync PropertyName and HeaderId when Name or Property changes
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

	TWeakPtr<IPropertyHandle> PropertyHandlePtr;
};

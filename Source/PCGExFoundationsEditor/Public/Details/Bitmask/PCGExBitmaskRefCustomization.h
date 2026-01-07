// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "IPropertyTypeCustomization.h"

class FPCGExBitmaskRefCustomization : public IPropertyTypeCustomization
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

	TSharedPtr<IPropertyHandle> SourceHandle;
	TSharedPtr<IPropertyHandle> IdentifierHandle;

	TArray<TSharedPtr<FName>> ComboOptions;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> ComboBoxWidget;

	void RefreshOptions();
};

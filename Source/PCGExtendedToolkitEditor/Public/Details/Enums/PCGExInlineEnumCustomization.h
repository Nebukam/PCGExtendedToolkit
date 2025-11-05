// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "IPropertyTypeCustomization.h"

class PCGEXTENDEDTOOLKITEDITOR_API FPCGExInlineEnumCustomization : public IPropertyTypeCustomization
{
public:
	explicit FPCGExInlineEnumCustomization(const FString& InEnumName);

	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> PropertyHandle,
		class FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> PropertyHandle,
		class IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

protected:
	FString EnumName = TEXT("");
	TSharedPtr<IPropertyHandle> EnumHandle;

	virtual TSharedRef<SWidget> GenerateEnumButtons(UEnum* Enum);
};
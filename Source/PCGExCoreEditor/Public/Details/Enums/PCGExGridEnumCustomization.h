// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "IPropertyTypeCustomization.h"

class PCGEXCOREEDITOR_API FPCGExGridEnumCustomization : public IPropertyTypeCustomization
{
public:
	explicit FPCGExGridEnumCustomization(const FString& InEnumName, const int32 InColumns = 3);

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
	int32 Columns = 3;
	TSharedPtr<IPropertyHandle> EnumHandle;

	virtual TSharedRef<SWidget> GenerateEnumButtons(UEnum* Enum);
};

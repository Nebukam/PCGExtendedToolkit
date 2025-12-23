// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "IPropertyTypeCustomization.h"

class SUniformGridPanel;

namespace PCGExBitmaskCustomization
{
	PCGEXCOREEDITOR_API
	TSharedRef<SWidget> BitsGrid(TSharedRef<SUniformGridPanel> Grid, TSharedPtr<IPropertyHandle> BitmaskHandle);
}

class FPCGExBitmaskCustomization : public IPropertyTypeCustomization
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

protected:
	virtual void BuildGrid(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder) const;
};

class FPCGExBitmaskWithOperationCustomization : public FPCGExBitmaskCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
};

class FPCGExBitmaskFilterConfigCustomization : public FPCGExBitmaskCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	virtual void BuildGrid(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder) const override;
};

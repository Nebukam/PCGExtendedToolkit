// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "IPropertyTypeCustomization.h"

class SUniformGridPanel;

namespace PCGExBitmaskCustomization
{
	PCGEXTENDEDTOOLKITEDITOR_API
	void FillGrid(TSharedRef<SUniformGridPanel> Grid, TSharedPtr<IPropertyHandle> BitmaskHandle);
}

class PCGEXTENDEDTOOLKITEDITOR_API FPCGExBitmaskCustomization : public IPropertyTypeCustomization
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

class PCGEXTENDEDTOOLKITEDITOR_API FPCGExBitmaskWithOperationCustomization : public FPCGExBitmaskCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
};

class PCGEXTENDEDTOOLKITEDITOR_API FPCGExBitmaskFilterConfigCustomization : public FPCGExBitmaskCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	virtual void BuildGrid(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder) const override;
};

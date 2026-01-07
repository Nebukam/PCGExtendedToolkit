// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "IPropertyTypeCustomization.h"

class FPCGExCompareShorthandCustomization : public IPropertyTypeCustomization
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

	virtual TSharedRef<SWidget> CreateValueWidget(TSharedPtr<IPropertyHandle> ValueHandle);
	virtual TSharedRef<SWidget> CreateAttributeWidget(TSharedPtr<IPropertyHandle> AttributeHandle);
};

class FPCGExCompareShorthandVectorCustomization : public FPCGExCompareShorthandCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	virtual TSharedRef<SWidget> CreateValueWidget(TSharedPtr<IPropertyHandle> ValueHandle) override;
};

class FPCGExCompareShorthandRotatorCustomization : public FPCGExCompareShorthandCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();
	virtual TSharedRef<SWidget> CreateValueWidget(TSharedPtr<IPropertyHandle> ValueHandle) override;
};

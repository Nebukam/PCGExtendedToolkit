// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "IPropertyTypeCustomization.h"

class FPCGExAssetEntryCustomization : public IPropertyTypeCustomization
{
public:
	virtual void CustomizeHeader(
		TSharedRef<IPropertyHandle> PropertyHandle,
		class FDetailWidgetRow& HeaderRow,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

	virtual void CustomizeChildren(
		TSharedRef<IPropertyHandle> PropertyHandle,
		class IDetailChildrenBuilder& ChildBuilder,
		IPropertyTypeCustomizationUtils& CustomizationUtils) override;

protected:
	TSet<FName> CustomizedTopLevelProperties;
	virtual void FillCustomizedTopLevelPropertiesNames();

	virtual TSharedRef<SWidget> GetAssetPicker(TSharedRef<IPropertyHandle> PropertyHandle, TSharedPtr<IPropertyHandle> IsSubCollectionHandle) =0;
};

class FPCGExMeshEntryCustomization : public FPCGExAssetEntryCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	virtual void FillCustomizedTopLevelPropertiesNames() override;
	virtual TSharedRef<SWidget> GetAssetPicker(TSharedRef<IPropertyHandle> PropertyHandle, TSharedPtr<IPropertyHandle> IsSubCollectionHandle) override;
};

class FPCGExActorEntryCustomization : public FPCGExAssetEntryCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

protected:
	virtual void FillCustomizedTopLevelPropertiesNames() override;
	virtual TSharedRef<SWidget> GetAssetPicker(TSharedRef<IPropertyHandle> PropertyHandle, TSharedPtr<IPropertyHandle> IsSubCollectionHandle) override;
};

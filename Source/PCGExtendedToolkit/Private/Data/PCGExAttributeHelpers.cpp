// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Data/PCGExAttributeHelpers.h"

bool FPCGExInputDescriptor::Validate(const UPCGPointData* InData)
{
	bValidatedAtLeastOnce = true;
	Selector = Selector.CopyAndFixLast(InData);

	if (GetSelection() == EPCGAttributePropertySelection::Attribute)
	{
		Attribute = Selector.IsValid() ? InData->Metadata->GetMutableAttribute(GetName()) : nullptr;

		if (Attribute)
		{
			const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InData, Selector);
			UnderlyingType = Accessor->GetUnderlyingType();
			//if (!Accessor.IsValid()) { Attribute = nullptr; }
		}

		return Attribute != nullptr;
	}
	else if (Selector.IsValid())
	{
		const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InData, Selector);
		UnderlyingType = Accessor->GetUnderlyingType();
		return true;
	}
	return false;
}

void PCGEx::FPinAttributeInfos::Discover(const UPCGPointData* InData)
{
	TArray<FName> Names;
	TArray<EPCGMetadataTypes> Types;
	InData->Metadata->GetAttributes(Names, Types);
	for (int i = 0; i < Names.Num(); i++) { Append(FAttributeInfos(Names[i], Types[i])); }
}

void PCGEx::FPinAttributeInfos::PushToDescriptor(FPCGExInputDescriptor& Descriptor, bool bReset) const
{
	TArray<FString>& ExtraNames = const_cast<TArray<FString>&>(Descriptor.GetMutableSelector().GetExtraNames());
	if (bReset) { ExtraNames.Empty(); }

	for (const FAttributeInfos& Infos : Attributes)
	{
		ExtraNames.AddUnique(Infos.GetDisplayName());
	}
}

void PCGEx::FLocalSingleComponentInput::Capture(const FPCGExInputDescriptorWithSingleField& InDescriptor)
{
	Descriptor = static_cast<FPCGExInputDescriptor>(InDescriptor);
	Field = InDescriptor.Field;
	Axis = InDescriptor.Axis;
}

void PCGEx::FLocalSingleComponentInput::Capture(const FPCGExInputDescriptorGeneric& InDescriptor)
{
	Descriptor = static_cast<FPCGExInputDescriptor>(InDescriptor);
	Field = InDescriptor.Field;
	Axis = InDescriptor.Axis;
}

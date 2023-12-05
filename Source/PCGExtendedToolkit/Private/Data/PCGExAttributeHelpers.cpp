// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Data/PCGExAttributeHelpers.h"

#if WITH_EDITOR
FString FPCGExInputDescriptor::GetDisplayName() const { return GetName().ToString(); }

void FPCGExInputDescriptor::UpdateUserFacingInfos() { TitlePropertyName = GetDisplayName(); }
#endif

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
	if (Selector.IsValid())
	{
		const TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InData, Selector);
		UnderlyingType = Accessor->GetUnderlyingType();
		return true;
	}
	return false;
}

void PCGEx::FLocalSingleFieldGetter::Capture(const FPCGExInputDescriptorWithSingleField& InDescriptor)
{
	Descriptor = static_cast<FPCGExInputDescriptor>(InDescriptor);
	Field = InDescriptor.Field;
	Axis = InDescriptor.Axis;
}

void PCGEx::FLocalSingleFieldGetter::Capture(const FPCGExInputDescriptorGeneric& InDescriptor)
{
	Descriptor = static_cast<FPCGExInputDescriptor>(InDescriptor);
	Field = InDescriptor.Field;
	Axis = InDescriptor.Axis;
}

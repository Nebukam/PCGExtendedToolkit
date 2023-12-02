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

void PCGEx::FPinAttributeInfos::Discover(const UPCGPointData* InData)
{
	TArray<FName> Names;
	TArray<EPCGMetadataTypes> Types;
	InData->Metadata->GetAttributes(Names, Types);
	for (int i = 0; i < Names.Num(); i++) { Append(FAttributeIdentity(Names[i], Types[i])); }
}

void PCGEx::FPinAttributeInfos::PushToDescriptor(FPCGExInputDescriptor& Descriptor, bool bReset) const
{
	TArray<FString>& ExtraNames = const_cast<TArray<FString>&>(Descriptor.GetMutableSelector().GetExtraNames());
	if (bReset) { ExtraNames.Empty(); }

	for (const FAttributeIdentity& Infos : Attributes)
	{
		ExtraNames.AddUnique(Infos.GetDisplayName());
	}
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

void PCGEx::FAttributeMap::PrepareForPoints(const UPCGPointData* InData)
{
	NumAttributes = InData->Metadata->GetAttributeCount();

	TArray<FName> Names;
	TArray<EPCGMetadataTypes> Types;

	Identities.Reset(NumAttributes);
	Attributes.Empty(NumAttributes);

	InData->Metadata->GetAttributes(Names, Types);
	for (int i = 0; i < NumAttributes; i++)
	{
		FAttributeIdentity& Identity = Identities.Emplace_GetRef(Names[i], Types[i]);
		Attributes.Add(Identity.Name, InData->Metadata->GetMutableAttribute(Identity.Name));
	}
}

void PCGEx::FAttributeMap::Prepare(const FAttributeMap& From, const FAttributeMap& To)
{
	Identities.Reset(From.NumAttributes);
	Attributes.Empty(From.NumAttributes);
}

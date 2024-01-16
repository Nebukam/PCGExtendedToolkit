// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Data/PCGExAttributeHelpers.h"

#if WITH_EDITOR
FString FPCGExInputDescriptor::GetDisplayName() const { return GetName().ToString(); }

void FPCGExInputDescriptor::UpdateUserFacingInfos() { TitlePropertyName = GetDisplayName(); }
#endif

bool FPCGExInputDescriptor::Validate(const UPCGPointData* InData)
{
	Selector = Selector.CopyAndFixLast(InData);

	if (GetSelection() == EPCGAttributePropertySelection::Attribute)
	{
		Attribute = Selector.IsValid() ? InData->Metadata->GetMutableAttribute(GetName()) : nullptr;
		UnderlyingType = Attribute ? Attribute->GetTypeId() : static_cast<int16>(EPCGMetadataTypes::Unknown);
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

namespace PCGEx
{
	void PCGEx::FAttributeIdentity::Get(const UPCGPointData* InData, TArray<FAttributeIdentity>& OutIdentities)
	{
		if (!InData->Metadata) { return; }
		TArray<FName> Names;
		TArray<EPCGMetadataTypes> Types;
		InData->Metadata->GetAttributes(Names, Types);
		const int32 NumAttributes = Names.Num();
		for (int i = 0; i < NumAttributes; i++)
		{
			OutIdentities.AddUnique(FAttributeIdentity(Names[i], Types[i]));
		}
	}

	void PCGEx::FAttributeIdentity::Get(const UPCGPointData* InData, TArray<FName>& OutNames, TMap<FName, FAttributeIdentity>& OutIdentities)
	{
		if (!InData->Metadata) { return; }
		TArray<EPCGMetadataTypes> Types;
		InData->Metadata->GetAttributes(OutNames, Types);
		const int32 NumAttributes = OutNames.Num();
		for (int i = 0; i < NumAttributes; i++)
		{
			FName Name = OutNames[i];
			OutIdentities.Add(Name, FAttributeIdentity(Name, Types[i]));
		}
	}

	bool FAttributesInfos::Contains(const FName AttributeName, const EPCGMetadataTypes Type)
	{
		for (FAttributeIdentity& Identity : Identities) { if (Identity.Name == AttributeName && Identity.UnderlyingType == Type) { return true; } }
		return false;
	}

	PCGEx::FAttributesInfos* PCGEx::FAttributesInfos::Get(const UPCGPointData* InData)
	{
		FAttributesInfos* NewInfos = new FAttributesInfos();
		FAttributeIdentity::Get(InData, NewInfos->Identities);
		return NewInfos;
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
}

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

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

	if (Selector.IsValid() &&
		Selector.GetSelection() == EPCGAttributePropertySelection::PointProperty)
	{
		UnderlyingType = static_cast<int16>(PCGEx::GetPointPropertyTypeId(Selector.GetPointProperty()));
		return true;
	}

	return false;
}

namespace PCGEx
{
	void FAttributeIdentity::Get(const UPCGMetadata* InMetadata, TArray<FAttributeIdentity>& OutIdentities)
	{
		if (!InMetadata) { return; }
		TArray<FName> Names;
		TArray<EPCGMetadataTypes> Types;
		InMetadata->GetAttributes(Names, Types);
		const int32 NumAttributes = Names.Num();
		for (int i = 0; i < NumAttributes; i++) { OutIdentities.AddUnique(FAttributeIdentity(Names[i], Types[i])); }
	}

	void FAttributeIdentity::Get(const UPCGMetadata* InMetadata, TArray<FName>& OutNames, TMap<FName, FAttributeIdentity>& OutIdentities)
	{
		if (!InMetadata) { return; }
		TArray<EPCGMetadataTypes> Types;
		InMetadata->GetAttributes(OutNames, Types);
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

	bool FAttributesInfos::Contains(const FName AttributeName)
	{
		for (FAttributeIdentity& Identity : Identities) { if (Identity.Name == AttributeName) { return true; } }
		return false;
	}

	FAttributeIdentity* FAttributesInfos::Find(const FName AttributeName)
	{
		for (FAttributeIdentity& Identity : Identities) { if (Identity.Name == AttributeName) { return &Identity; } }
		return nullptr;
	}

	FAttributesInfos* FAttributesInfos::Get(const UPCGMetadata* InMetadata)
	{
		FAttributesInfos* NewInfos = new FAttributesInfos();
		FAttributeIdentity::Get(InMetadata, NewInfos->Identities);
		if (InMetadata)
		{
			UPCGMetadata* MutableData = const_cast<UPCGMetadata*>(InMetadata);
			for (const FAttributeIdentity& Identity : NewInfos->Identities)
			{
				NewInfos->Attributes.Add(MutableData->GetMutableAttribute(Identity.Name));
			}
		}
		return NewInfos;
	}
}

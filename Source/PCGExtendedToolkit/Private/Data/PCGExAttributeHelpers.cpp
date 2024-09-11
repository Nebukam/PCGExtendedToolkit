// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExAttributeHelpers.h"

#include "Data/PCGExData.h"

#if WITH_EDITOR
FString FPCGExInputConfig::GetDisplayName() const { return GetName().ToString(); }

void FPCGExInputConfig::UpdateUserFacingInfos() { TitlePropertyName = GetDisplayName(); }
#endif

bool FPCGExInputConfig::Validate(const UPCGPointData* InData)
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
		for (int i = 0; i < NumAttributes; ++i)
		{
			OutIdentities.AddUnique(FAttributeIdentity(Names[i], Types[i], InMetadata->GetConstAttribute(Names[i])->AllowsInterpolation()));
		}
	}

	void FAttributeIdentity::Get(const UPCGMetadata* InMetadata, TArray<FName>& OutNames, TMap<FName, FAttributeIdentity>& OutIdentities)
	{
		if (!InMetadata) { return; }
		TArray<EPCGMetadataTypes> Types;
		InMetadata->GetAttributes(OutNames, Types);
		const int32 NumAttributes = OutNames.Num();

		for (int i = 0; i < NumAttributes; ++i)
		{
			FName Name = OutNames[i];
			OutIdentities.Add(Name, FAttributeIdentity(Name, Types[i], InMetadata->GetConstAttribute(Name)->AllowsInterpolation()));
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

	bool FAttributesInfos::FindMissing(const TSet<FName>& Checklist, TSet<FName>& OutMissing)
	{
		bool bAnyMissing = false;
		for (const FName& Id : Checklist)
		{
			if (!Contains(Id) || !IsValidName(Id))
			{
				OutMissing.Add(Id);
				bAnyMissing = true;
			}
		}
		return bAnyMissing;
	}

	bool FAttributesInfos::FindMissing(const TArray<FName>& Checklist, TSet<FName>& OutMissing)
	{
		bool bAnyMissing = false;
		for (const FName& Id : Checklist)
		{
			if (!Contains(Id) || !IsValidName(Id))
			{
				OutMissing.Add(Id);
				bAnyMissing = true;
			}
		}
		return bAnyMissing;
	}

	void FAttributesInfos::Append(const FAttributesInfos* Other, const FPCGExAttributeGatherDetails& InGatherDetails, TSet<FName>& OutTypeMismatch)
	{
		for (int i = 0; i < Other->Identities.Num(); ++i)
		{
			const FAttributeIdentity& OtherId = Other->Identities[i];

			if (!InGatherDetails.Test(OtherId.Name.ToString())) { continue; }

			if (const int32* Index = Map.Find(OtherId.Name))
			{
				const FAttributeIdentity& Id = Identities[*Index];
				if (Id.UnderlyingType != OtherId.UnderlyingType)
				{
					OutTypeMismatch.Add(Id.Name);
					// TODO : Update existing based on settings
				}

				continue;
			}

			FPCGMetadataAttributeBase* Attribute = Other->Attributes[i];
			int32 AppendIndex = Identities.Add(OtherId);
			Attributes.Add(Attribute);
			Map.Add(OtherId.Name, AppendIndex);
		}
	}

	void FAttributesInfos::Update(const FAttributesInfos* Other, const FPCGExAttributeGatherDetails& InGatherDetails, TSet<FName>& OutTypeMismatch)
	{
		// TODO : Update types and attributes according to input settings?
	}

	FAttributesInfos* FAttributesInfos::Get(const UPCGMetadata* InMetadata)
	{
		FAttributesInfos* NewInfos = new FAttributesInfos();
		FAttributeIdentity::Get(InMetadata, NewInfos->Identities);

		UPCGMetadata* MutableData = const_cast<UPCGMetadata*>(InMetadata);
		for (int i = 0; i < NewInfos->Identities.Num(); ++i)
		{
			const FAttributeIdentity& Identity = NewInfos->Identities[i];
			NewInfos->Map.Add(Identity.Name, i);
			NewInfos->Attributes.Add(MutableData->GetMutableAttribute(Identity.Name));
		}

		return NewInfos;
	}
}

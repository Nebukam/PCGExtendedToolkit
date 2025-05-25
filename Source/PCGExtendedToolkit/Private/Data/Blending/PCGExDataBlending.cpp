// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExDataBlending.h"

namespace PCGExDataBlending
{
	void FBlendingHeader::Select(const FString& Selection) { Selector.Update(Selection); }

	void FBlendingHeader::SetBlending(const EPCGExDataBlendingType InBlending)
	{
		switch (InBlending)
		{
		case EPCGExDataBlendingType::None: Blending = EPCGExABBlendingType::None;
			break;
		case EPCGExDataBlendingType::Average: Blending = EPCGExABBlendingType::Average;
			break;
		case EPCGExDataBlendingType::Weight: Blending = EPCGExABBlendingType::Weight;
			break;
		case EPCGExDataBlendingType::Min: Blending = EPCGExABBlendingType::Min;
			break;
		case EPCGExDataBlendingType::Max: Blending = EPCGExABBlendingType::Max;
			break;
		case EPCGExDataBlendingType::Copy: Blending = EPCGExABBlendingType::CopySource;
			break;
		case EPCGExDataBlendingType::Sum: Blending = EPCGExABBlendingType::Add;
			break;
		case EPCGExDataBlendingType::WeightedSum: Blending = EPCGExABBlendingType::WeightedAdd;
			break;
		case EPCGExDataBlendingType::Lerp: Blending = EPCGExABBlendingType::Lerp;
			break;
		case EPCGExDataBlendingType::Subtract: Blending = EPCGExABBlendingType::Subtract;
			break;
		case EPCGExDataBlendingType::UnsignedMin: Blending = EPCGExABBlendingType::UnsignedMin;
			break;
		case EPCGExDataBlendingType::UnsignedMax: Blending = EPCGExABBlendingType::UnsignedMax;
			break;
		case EPCGExDataBlendingType::AbsoluteMin: Blending = EPCGExABBlendingType::AbsoluteMin;
			break;
		case EPCGExDataBlendingType::AbsoluteMax: Blending = EPCGExABBlendingType::AbsoluteMax;
			break;
		case EPCGExDataBlendingType::WeightedSubtract: Blending = EPCGExABBlendingType::WeightedSubtract;
			break;
		case EPCGExDataBlendingType::CopyOther: Blending = EPCGExABBlendingType::CopyTarget;
			break;
		case EPCGExDataBlendingType::Hash: Blending = EPCGExABBlendingType::Hash;
			break;
		case EPCGExDataBlendingType::UnsignedHash: Blending = EPCGExABBlendingType::UnsignedHash;
			break;
		}
	}
}

FPCGExPropertiesBlendingDetails::FPCGExPropertiesBlendingDetails(const EPCGExDataBlendingType InDefaultBlending)
	: DefaultBlending(InDefaultBlending)
{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_NAME, ...) _NAME##Blending = InDefaultBlending;
	PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY
}

FPCGExBlendingDetails::FPCGExBlendingDetails(const EPCGExDataBlendingType InDefaultBlending)
	: DefaultBlending(InDefaultBlending)
{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_NAME, ...) PropertiesOverrides._NAME##Blending = InDefaultBlending;
	PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY
}

FPCGExBlendingDetails::FPCGExBlendingDetails(const EPCGExDataBlendingType InDefaultBlending, const EPCGExDataBlendingType InPositionBlending)
	: DefaultBlending(InDefaultBlending)
{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_NAME, ...) PropertiesOverrides._NAME##Blending = InDefaultBlending;
	PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY

	PropertiesOverrides.bOverridePosition = true;
	PropertiesOverrides.PositionBlending = InPositionBlending;
}

FPCGExBlendingDetails::FPCGExBlendingDetails(const FPCGExPropertiesBlendingDetails& InDetails)
	: DefaultBlending(InDetails.DefaultBlending)
{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_NAME, ...) PropertiesOverrides.bOverride##_NAME = InDetails._NAME##Blending != EPCGExDataBlendingType::None; PropertiesOverrides._NAME##Blending = InDetails._NAME##Blending;
	PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY
}

FPCGExPropertiesBlendingDetails FPCGExBlendingDetails::GetPropertiesBlendingDetails() const
{
	FPCGExPropertiesBlendingDetails OutDetails;
#define PCGEX_SET_POINTPROPERTY(_NAME, ...) OutDetails._NAME##Blending = PropertiesOverrides.bOverride##_NAME ? PropertiesOverrides._NAME##Blending : DefaultBlending;
	PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_POINTPROPERTY)
#undef PCGEX_SET_POINTPROPERTY
	return OutDetails;
}

bool FPCGExBlendingDetails::CanBlend(const FName AttributeName) const
{
	switch (BlendingFilter)
	{
	default: ;
	case EPCGExAttributeFilter::All:
		return true;
	case EPCGExAttributeFilter::Exclude:
		return !FilteredAttributes.Contains(AttributeName);
	case EPCGExAttributeFilter::Include:
		return FilteredAttributes.Contains(AttributeName);
	}
}

void FPCGExBlendingDetails::Filter(TArray<PCGEx::FAttributeIdentity>& Identities) const
{
	if (BlendingFilter == EPCGExAttributeFilter::All) { return; }
	for (int i = 0; i < Identities.Num(); i++)
	{
		if (!CanBlend(Identities[i].Name))
		{
			Identities.RemoveAt(i);
			i--;
		}
	}
}

bool FPCGExBlendingDetails::GetBlendingHeader(const FName InName, PCGExDataBlending::FBlendingHeader& OutHeader) const
{
	if (!CanBlend(InName)) { return false; }

	OutHeader = PCGExDataBlending::FBlendingHeader{};
	OutHeader.Select(InName.ToString());

	// TODO : Update with information regarding whether this is a new attribute or not

	if (OutHeader.Selector.GetSelection() == EPCGAttributePropertySelection::Attribute &&
		PCGEx::IsPCGExAttribute(InName))
	{
		// Don't blend PCGEx stuff
		OutHeader.SetBlending(EPCGExDataBlendingType::Copy);
	}
	else
	{
		const EPCGExDataBlendingType* TypePtr = AttributesOverrides.Find(InName);
		OutHeader.SetBlending(TypePtr ? *TypePtr : DefaultBlending);
	}

	if (OutHeader.Blending == EPCGExABBlendingType::None) { return false; }
	return true;
}

void FPCGExBlendingDetails::GetPointPropertyBlendingHeaders(TArray<PCGExDataBlending::FBlendingHeader>& OutHeaders) const
{
	// Emplace all individual properties if they aren't blending to None
#define PCGEX_SET_POINTPROPERTY(_NAME, ...) \
	if(const EPCGExDataBlendingType _NAME##Blending = PropertiesOverrides.bOverride##_NAME ? PropertiesOverrides._NAME##Blending : DefaultBlending;\
		_NAME##Blending != EPCGExDataBlendingType::None){ \
		PCGExDataBlending::FBlendingHeader& _NAME##Header = OutHeaders.Emplace_GetRef();\
		_NAME##Header.Select(TEXT("$" #_NAME ));\
		_NAME##Header.SetBlending(PropertiesOverrides.bOverride##_NAME ? PropertiesOverrides._NAME##Blending : DefaultBlending);\
	}
	PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_POINTPROPERTY)
#undef PCGEX_SET_POINTPROPERTY
}

void FPCGExBlendingDetails::GetBlendingHeaders(
	const UPCGMetadata* SourceMetadata, UPCGMetadata* TargetMetadata,
	TArray<PCGExDataBlending::FBlendingHeader>& OutHeaders,
	const bool bSkipProperties,
	const TSet<FName>* IgnoreAttributeSet) const
{
	if (!bSkipProperties) { GetPointPropertyBlendingHeaders(OutHeaders); }

	TArray<PCGEx::FAttributeIdentity> Identities;
	PCGEx::FAttributeIdentity::Get(TargetMetadata, Identities);

	Filter(Identities);

	// Track attributes that are missing on the target
	TSet<int32> MissingAttribute;

	if (SourceMetadata != TargetMetadata)
	{
		// If target & source are not the same we :
		// - Remove identities that only exist on target
		// - Remove type mismatches (we could let the blender handle broadcasting)
		// - Add any attribute from source that's missing from the target

		TArray<FName> TargetNames;
		TArray<FName> SourceNames;

		TMap<FName, PCGEx::FAttributeIdentity> TargetIdentityMap;
		TMap<FName, PCGEx::FAttributeIdentity> SourceIdentityMap;

		PCGEx::FAttributeIdentity::Get(TargetMetadata, TargetNames, TargetIdentityMap);
		PCGEx::FAttributeIdentity::Get(SourceMetadata, SourceNames, SourceIdentityMap);

		for (FName TargetName : TargetNames)
		{
			const PCGEx::FAttributeIdentity& TargetIdentity = *TargetIdentityMap.Find(TargetName);
			//An attribute exists on Target but not on Source -- Skip it.
			if (!SourceIdentityMap.Find(TargetName)) { Identities.Remove(TargetIdentity); }
		}

		for (FName SourceName : SourceNames)
		{
			const PCGEx::FAttributeIdentity& SourceIdentity = *SourceIdentityMap.Find(SourceName);
			if (const PCGEx::FAttributeIdentity* TargetIdentityPtr = TargetIdentityMap.Find(SourceName))
			{
				// Type mismatch -- Simply ignore it
				if (TargetIdentityPtr->UnderlyingType != SourceIdentity.UnderlyingType) { Identities.Remove(*TargetIdentityPtr); }
			}
			else if (CanBlend(SourceIdentity.Name))
			{
				//Attribute exists on source, but not target.
				MissingAttribute.Add(Identities.Add(SourceIdentity));
			}
		}
	}

	// We now have a list of attribute identities we can process

	for (int i = 0; i < Identities.Num(); i++)
	{
		const PCGEx::FAttributeIdentity& Identity = Identities[i];

		if (IgnoreAttributeSet && IgnoreAttributeSet->Contains(Identity.Name)) { continue; }

		PCGExDataBlending::FBlendingHeader Header{};
		Header.bIsNewAttribute = MissingAttribute.Contains(i);

		if (PCGEx::IsPCGExAttribute(Identity.Name))
		{
			// Don't blend PCGEx stuff
			Header.SetBlending(EPCGExDataBlendingType::Copy);
		}
		else
		{
			const EPCGExDataBlendingType* TypePtr = AttributesOverrides.Find(Identity.Name);
			// TODO : Support global defaults (or ditch support)
			Header.SetBlending(TypePtr ? *TypePtr : DefaultBlending);
		}

		if (Header.Blending == EPCGExABBlendingType::None) { continue; }

		OutHeaders.Add(Header);
	}
}

namespace PCGExDataBlending
{
	void AssembleBlendingDetails(
		const FPCGExPropertiesBlendingDetails& PropertiesBlending,
		const TMap<FName, EPCGExDataBlendingType>& PerAttributeBlending,
		const TSharedRef<PCGExData::FPointIO>& SourceIO,
		FPCGExBlendingDetails& OutDetails,
		TSet<FName>& OutMissingAttributes)
	{
		const TSharedPtr<PCGEx::FAttributesInfos> AttributesInfos = PCGEx::FAttributesInfos::Get(SourceIO->GetIn()->Metadata);

		OutDetails = FPCGExBlendingDetails(PropertiesBlending);
		OutDetails.BlendingFilter = EPCGExAttributeFilter::Include;

		TArray<FName> SourceAttributesList;
		PerAttributeBlending.GetKeys(SourceAttributesList);

		AttributesInfos->FindMissing(SourceAttributesList, OutMissingAttributes);

		for (const FName& Id : SourceAttributesList)
		{
			if (OutMissingAttributes.Contains(Id)) { continue; }

			OutDetails.AttributesOverrides.Add(Id, *PerAttributeBlending.Find(Id));
			OutDetails.FilteredAttributes.Add(Id);
		}
	}

	void AssembleBlendingDetails(
		const EPCGExDataBlendingType& DefaultBlending,
		const TArray<FName>& Attributes,
		const TSharedRef<PCGExData::FPointIO>& SourceIO,
		FPCGExBlendingDetails& OutDetails,
		TSet<FName>& OutMissingAttributes)
	{
		const TSharedPtr<PCGEx::FAttributesInfos> AttributesInfos = PCGEx::FAttributesInfos::Get(SourceIO->GetIn()->Metadata);
		OutDetails = FPCGExBlendingDetails(FPCGExPropertiesBlendingDetails(EPCGExDataBlendingType::None));
		OutDetails.BlendingFilter = EPCGExAttributeFilter::Include;

		AttributesInfos->FindMissing(Attributes, OutMissingAttributes);

		for (const FName& Id : Attributes)
		{
			if (OutMissingAttributes.Contains(Id)) { continue; }

			OutDetails.AttributesOverrides.Add(Id, DefaultBlending);
			OutDetails.FilteredAttributes.Add(Id);
		}
	}
}

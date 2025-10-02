﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExDataBlending.h"

#include "PCGPin.h"
#include "Data/PCGExData.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExDataPreloader.h"
#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExBlendOpFactoryProvider.h"

namespace PCGExDataBlending
{
	void DeclareBlendOpsInputs(TArray<FPCGPinProperties>& PinProperties, const EPCGPinStatus InStatus, EPCGExBlendingInterface Interface)
	{
		FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(SourceBlendingLabel, EPCGDataType::Param);
		PCGEX_PIN_TOOLTIP("Blending configurations, used by Individual (non-monolithic) blending interface.")
		Pin.PinStatus = Interface == EPCGExBlendingInterface::Monolithic ? EPCGPinStatus::Advanced : InStatus;
	}

	void FBlendingParam::SelectFromString(const FString& Selection)
	{
		Identifier = FName(*Selection);
		Selector.Update(Selection);
	}

	void FBlendingParam::Select(const FPCGAttributeIdentifier& InIdentifier)
	{
		Identifier = InIdentifier;
		Selector.Update(InIdentifier.Name.ToString());

		if (InIdentifier.MetadataDomain.Flag == EPCGMetadataDomainFlag::Data) { Selector.SetDomainName(PCGDataConstants::DataDomainName); }
		else { Selector.SetDomainName(PCGDataConstants::DefaultDomainName); }
	}

	void FBlendingParam::SetBlending(const EPCGExDataBlendingType InBlending)
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
		if (!CanBlend(Identities[i].Identifier.Name))
		{
			Identities.RemoveAt(i);
			i--;
		}
	}
}

bool FPCGExBlendingDetails::GetBlendingParam(const FPCGAttributeIdentifier& InIdentifer, PCGExDataBlending::FBlendingParam& OutParam) const
{
	if (!CanBlend(InIdentifer.Name)) { return false; }

	OutParam = PCGExDataBlending::FBlendingParam{};
	OutParam.Select(InIdentifer);

	// TODO : Update with information regarding whether this is a new attribute or not

	if (OutParam.Selector.GetSelection() == EPCGAttributePropertySelection::Attribute &&
		PCGEx::IsPCGExAttribute(InIdentifer.Name))
	{
		// Don't blend PCGEx stuff
		OutParam.SetBlending(EPCGExDataBlendingType::Copy);
	}
	else
	{
		const EPCGExDataBlendingType* TypePtr = AttributesOverrides.Find(InIdentifer.Name);
		OutParam.SetBlending(TypePtr ? *TypePtr : DefaultBlending);
	}

	if (OutParam.Blending == EPCGExABBlendingType::None) { return false; }
	return true;
}

void FPCGExBlendingDetails::GetPointPropertyBlendingParams(TArray<PCGExDataBlending::FBlendingParam>& OutParams) const
{
	// Emplace all individual properties if they aren't blending to None
#define PCGEX_SET_POINTPROPERTY(_NAME, ...) \
	if(const EPCGExDataBlendingType _NAME##Blending = PropertiesOverrides.bOverride##_NAME ? PropertiesOverrides._NAME##Blending : DefaultBlending;\
		_NAME##Blending != EPCGExDataBlendingType::None){ \
		PCGExDataBlending::FBlendingParam& _NAME##Param = OutParams.Emplace_GetRef();\
		_NAME##Param.SelectFromString(TEXT("$" #_NAME ));\
		_NAME##Param.SetBlending(PropertiesOverrides.bOverride##_NAME ? PropertiesOverrides._NAME##Blending : DefaultBlending);\
	}
	PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_POINTPROPERTY)
#undef PCGEX_SET_POINTPROPERTY
}

void FPCGExBlendingDetails::GetBlendingParams(
	const UPCGMetadata* SourceMetadata, UPCGMetadata* TargetMetadata,
	TArray<PCGExDataBlending::FBlendingParam>& OutParams,
	TArray<FPCGAttributeIdentifier>& OutAttributeIdentifiers,
	const bool bSkipProperties,
	const TSet<FName>* IgnoreAttributeSet) const
{
	if (!bSkipProperties) { GetPointPropertyBlendingParams(OutParams); }

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

		TArray<FPCGAttributeIdentifier> TargetIdentifiers;
		TArray<FPCGAttributeIdentifier> SourceIdentifiers;

		TMap<FPCGAttributeIdentifier, PCGEx::FAttributeIdentity> TargetIdentityMap;
		TMap<FPCGAttributeIdentifier, PCGEx::FAttributeIdentity> SourceIdentityMap;

		PCGEx::FAttributeIdentity::Get(TargetMetadata, TargetIdentifiers, TargetIdentityMap);
		PCGEx::FAttributeIdentity::Get(SourceMetadata, SourceIdentifiers, SourceIdentityMap);

		for (const FPCGAttributeIdentifier& TargetIdentifier : TargetIdentifiers)
		{
			const PCGEx::FAttributeIdentity& TargetIdentity = *TargetIdentityMap.Find(TargetIdentifier);
			//An attribute exists on Target but not on Source -- Skip it.
			if (!SourceIdentityMap.Find(TargetIdentifier)) { Identities.Remove(TargetIdentity); }
		}

		for (const FPCGAttributeIdentifier& SourceIdentifier : SourceIdentifiers)
		{
			const PCGEx::FAttributeIdentity& SourceIdentity = *SourceIdentityMap.Find(SourceIdentifier);
			if (const PCGEx::FAttributeIdentity* TargetIdentityPtr = TargetIdentityMap.Find(SourceIdentifier))
			{
				// Type mismatch -- Simply ignore it
				if (TargetIdentityPtr->UnderlyingType != SourceIdentity.UnderlyingType) { Identities.Remove(*TargetIdentityPtr); }
			}
			else if (CanBlend(SourceIdentity.Identifier.Name))
			{
				//Attribute exists on source, but not target.
				MissingAttribute.Add(Identities.Add(SourceIdentity));
			}
		}
	}

	// We now have a list of attribute identities we can process

	OutAttributeIdentifiers.Reserve(Identities.Num());

	for (int i = 0; i < Identities.Num(); i++)
	{
		const PCGEx::FAttributeIdentity& Identity = Identities[i];

		if (IgnoreAttributeSet && IgnoreAttributeSet->Contains(Identity.Identifier.Name)) { continue; }

		PCGExDataBlending::FBlendingParam Param{};
		Param.bIsNewAttribute = MissingAttribute.Contains(i);

		if (PCGEx::IsPCGExAttribute(Identity.Identifier.Name))
		{
			// Don't blend PCGEx stuff
			Param.SetBlending(EPCGExDataBlendingType::Copy);
		}
		else
		{
			const EPCGExDataBlendingType* TypePtr = AttributesOverrides.Find(Identity.Identifier.Name);
			// TODO : Support global defaults (or ditch support)
			Param.SetBlending(TypePtr ? *TypePtr : DefaultBlending);
		}

		if (Param.Blending == EPCGExABBlendingType::None) { continue; }

		OutAttributeIdentifiers.Add(Identity.Identifier);
		Param.Select(Identity.Identifier);
		OutParams.Add(Param);
	}
}

void FPCGExBlendingDetails::RegisterBuffersDependencies(
	FPCGExContext* InContext,
	PCGExData::FFacadePreloader& FacadePreloader, const
	TSet<FName>* IgnoredAttributes) const
{
	TSharedPtr<PCGExData::FFacade> InDataFacade = FacadePreloader.GetDataFacade();
	if (!InDataFacade) { return; }

	const TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(InDataFacade->GetIn()->Metadata, IgnoredAttributes);
	Filter(Infos->Identities);

	for (const PCGEx::FAttributeIdentity& Identity : Infos->Identities) { FacadePreloader.Register(InContext, Identity); }
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

	void GetFilteredIdentities(
		const UPCGMetadata* InMetadata,
		TArray<PCGEx::FAttributeIdentity>& OutIdentities,
		const FPCGExBlendingDetails* InBlendingDetails,
		const FPCGExCarryOverDetails* InCarryOverDetails,
		const TSet<FName>* IgnoreAttributeSet)
	{
		PCGEx::FAttributeIdentity::Get(InMetadata, OutIdentities, IgnoreAttributeSet);
		if (InCarryOverDetails) { InCarryOverDetails->Prune(OutIdentities); }
		if (InBlendingDetails) { InBlendingDetails->Filter(OutIdentities); }
	}
}

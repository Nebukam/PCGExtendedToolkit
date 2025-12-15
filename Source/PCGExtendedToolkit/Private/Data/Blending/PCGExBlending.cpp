// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExBlending.h"

#include "PCGPin.h"
#include "Data/PCGExData.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExDataPreloader.h"
#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExBlendOpFactoryProvider.h"

namespace PCGExBlending
{
	void ConvertBlending(const EPCGExBlendingType From, EPCGExABBlendingType& OutTo)
	{
		switch (From)
		{
		case EPCGExBlendingType::None: OutTo = EPCGExABBlendingType::None;
			break;
		case EPCGExBlendingType::Average: OutTo = EPCGExABBlendingType::Average;
			break;
		case EPCGExBlendingType::Weight: OutTo = EPCGExABBlendingType::Weight;
			break;
		case EPCGExBlendingType::Min: OutTo = EPCGExABBlendingType::Min;
			break;
		case EPCGExBlendingType::Max: OutTo = EPCGExABBlendingType::Max;
			break;
		case EPCGExBlendingType::Copy: OutTo = EPCGExABBlendingType::CopySource;
			break;
		case EPCGExBlendingType::Sum: OutTo = EPCGExABBlendingType::Add;
			break;
		case EPCGExBlendingType::WeightedSum: OutTo = EPCGExABBlendingType::WeightedAdd;
			break;
		case EPCGExBlendingType::Lerp: OutTo = EPCGExABBlendingType::Lerp;
			break;
		case EPCGExBlendingType::Subtract: OutTo = EPCGExABBlendingType::Subtract;
			break;
		case EPCGExBlendingType::UnsignedMin: OutTo = EPCGExABBlendingType::UnsignedMin;
			break;
		case EPCGExBlendingType::UnsignedMax: OutTo = EPCGExABBlendingType::UnsignedMax;
			break;
		case EPCGExBlendingType::AbsoluteMin: OutTo = EPCGExABBlendingType::AbsoluteMin;
			break;
		case EPCGExBlendingType::AbsoluteMax: OutTo = EPCGExABBlendingType::AbsoluteMax;
			break;
		case EPCGExBlendingType::WeightedSubtract: OutTo = EPCGExABBlendingType::WeightedSubtract;
			break;
		case EPCGExBlendingType::CopyOther: OutTo = EPCGExABBlendingType::CopyTarget;
			break;
		case EPCGExBlendingType::Hash: OutTo = EPCGExABBlendingType::Hash;
			break;
		case EPCGExBlendingType::UnsignedHash: OutTo = EPCGExABBlendingType::UnsignedHash;
			break;
		case EPCGExBlendingType::WeightNormalize: OutTo = EPCGExABBlendingType::WeightNormalize;
			break;
		default:
		case EPCGExBlendingType::Unset: OutTo = EPCGExABBlendingType::None;
			break;
		}
	}

	void DeclareBlendOpsInputs(TArray<FPCGPinProperties>& PinProperties, const EPCGPinStatus InStatus, EPCGExBlendingInterface Interface)
	{
		FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(SourceBlendingLabel, FPCGExDataTypeInfoBlendOp::AsId());
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

	void FBlendingParam::SetBlending(const EPCGExBlendingType InBlending)
	{
		ConvertBlending(InBlending, Blending);
	}
}

FPCGExPropertiesBlendingDetails::FPCGExPropertiesBlendingDetails(const EPCGExBlendingType InDefaultBlending)
	: DefaultBlending(InDefaultBlending)
{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_NAME, ...) _NAME##Blending = InDefaultBlending;
	PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY
}

FPCGExBlendingDetails::FPCGExBlendingDetails(const EPCGExBlendingType InDefaultBlending)
	: DefaultBlending(InDefaultBlending)
{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_NAME, ...) PropertiesOverrides._NAME##Blending = InDefaultBlending;
	PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY
}

FPCGExBlendingDetails::FPCGExBlendingDetails(const EPCGExBlendingType InDefaultBlending, const EPCGExBlendingType InPositionBlending)
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
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_NAME, ...) PropertiesOverrides.bOverride##_NAME = InDetails._NAME##Blending != EPCGExBlendingType::None; PropertiesOverrides._NAME##Blending = InDetails._NAME##Blending;
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
	case EPCGExAttributeFilter::All: return true;
	case EPCGExAttributeFilter::Exclude: return !FilteredAttributes.Contains(AttributeName);
	case EPCGExAttributeFilter::Include: return FilteredAttributes.Contains(AttributeName);
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

bool FPCGExBlendingDetails::GetBlendingParam(const FPCGAttributeIdentifier& InIdentifer, PCGExBlending::FBlendingParam& OutParam) const
{
	if (!CanBlend(InIdentifer.Name)) { return false; }

	OutParam = PCGExBlending::FBlendingParam{};
	OutParam.Select(InIdentifer);

	// TODO : Update with information regarding whether this is a new attribute or not

	if (OutParam.Selector.GetSelection() == EPCGAttributePropertySelection::Attribute && PCGEx::IsPCGExAttribute(InIdentifer.Name))
	{
		// Don't blend PCGEx stuff
		OutParam.SetBlending(EPCGExBlendingType::Copy);
	}
	else
	{
		const EPCGExBlendingType* TypePtr = AttributesOverrides.Find(InIdentifer.Name);
		OutParam.SetBlending(TypePtr ? *TypePtr : DefaultBlending);
	}

	if (OutParam.Blending == EPCGExABBlendingType::None) { return false; }
	return true;
}

void FPCGExBlendingDetails::GetPointPropertyBlendingParams(TArray<PCGExBlending::FBlendingParam>& OutParams) const
{
	// Emplace all individual properties if they aren't blending to None
#define PCGEX_SET_POINTPROPERTY(_NAME, ...) \
	if(const EPCGExBlendingType _NAME##Blending = PropertiesOverrides.bOverride##_NAME ? PropertiesOverrides._NAME##Blending : DefaultBlending;\
		_NAME##Blending != EPCGExBlendingType::None){ \
		PCGExBlending::FBlendingParam& _NAME##Param = OutParams.Emplace_GetRef();\
		_NAME##Param.SelectFromString(TEXT("$" #_NAME ));\
		_NAME##Param.SetBlending(PropertiesOverrides.bOverride##_NAME ? PropertiesOverrides._NAME##Blending : DefaultBlending);\
	}
	PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_POINTPROPERTY)
#undef PCGEX_SET_POINTPROPERTY
}

void FPCGExBlendingDetails::GetBlendingParams(const UPCGMetadata* SourceMetadata, UPCGMetadata* TargetMetadata, TArray<PCGExBlending::FBlendingParam>& OutParams, TArray<FPCGAttributeIdentifier>& OutAttributeIdentifiers, const bool bSkipProperties, const TSet<FName>* IgnoreAttributeSet) const
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

		PCGExBlending::FBlendingParam Param{};
		Param.bIsNewAttribute = MissingAttribute.Contains(i);

		if (PCGEx::IsPCGExAttribute(Identity.Identifier.Name))
		{
			// Don't blend PCGEx stuff
			Param.SetBlending(EPCGExBlendingType::Copy);
		}
		else
		{
			if (const EPCGExBlendingType* TypePtr = AttributesOverrides.Find(Identity.Identifier.Name))
			{
				Param.SetBlending(*TypePtr);
			}
			else
			{
				EPCGExBlendingType DesiredBlending = DefaultBlending;

#define PCGEX_GET_GLOBAL_BLENDMODE(_TYPE, _NAME, ...)\
if (Identity.UnderlyingType == EPCGMetadataTypes::Boolean){\
if (GetDefault<UPCGExGlobalSettings>()->DefaultBooleanBlendMode != EPCGExBlendingTypeDefault::Default){\
DesiredBlending = static_cast<EPCGExBlendingType>(GetDefault<UPCGExGlobalSettings>()->DefaultBooleanBlendMode);}}

				PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_GET_GLOBAL_BLENDMODE)

#undef PCGEX_GET_GLOBAL_BLENDMODE

				Param.SetBlending(DesiredBlending);
			}
		}

		if (Param.Blending == EPCGExABBlendingType::None) { continue; }

		OutAttributeIdentifiers.Add(Identity.Identifier);
		Param.Select(Identity.Identifier);
		OutParams.Add(Param);
	}
}

void FPCGExBlendingDetails::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader, const TSet<FName>* IgnoredAttributes) const
{
	TSharedPtr<PCGExData::FFacade> InDataFacade = FacadePreloader.GetDataFacade();
	if (!InDataFacade) { return; }

	const TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(InDataFacade->GetIn()->Metadata, IgnoredAttributes);
	Filter(Infos->Identities);

	for (const PCGEx::FAttributeIdentity& Identity : Infos->Identities) { FacadePreloader.Register(InContext, Identity); }
}

namespace PCGExBlending
{
	void AssembleBlendingDetails(const FPCGExPropertiesBlendingDetails& PropertiesBlending, const TMap<FName, EPCGExBlendingType>& PerAttributeBlending, const TSharedRef<PCGExData::FPointIO>& SourceIO, FPCGExBlendingDetails& OutDetails, TSet<FName>& OutMissingAttributes)
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

	void AssembleBlendingDetails(const FPCGExPropertiesBlendingDetails& PropertiesBlending, const TMap<FName, EPCGExBlendingType>& PerAttributeBlending, const TArray<TSharedRef<PCGExData::FFacade>>& InSources, FPCGExBlendingDetails& OutDetails, TSet<FName>& OutMissingAttributes)
	{
		OutDetails = FPCGExBlendingDetails(PropertiesBlending);
		OutDetails.BlendingFilter = EPCGExAttributeFilter::Include;

		for (const TSharedRef<PCGExData::FFacade>& Facade : InSources)
		{
			const TSharedPtr<PCGEx::FAttributesInfos> AttributesInfos = PCGEx::FAttributesInfos::Get(Facade->Source->GetIn()->Metadata);

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
	}

	void AssembleBlendingDetails(const EPCGExBlendingType& DefaultBlending, const TArray<FName>& Attributes, const TSharedRef<PCGExData::FPointIO>& SourceIO, FPCGExBlendingDetails& OutDetails, TSet<FName>& OutMissingAttributes)
	{
		const TSharedPtr<PCGEx::FAttributesInfos> AttributesInfos = PCGEx::FAttributesInfos::Get(SourceIO->GetIn()->Metadata);
		OutDetails = FPCGExBlendingDetails(FPCGExPropertiesBlendingDetails(EPCGExBlendingType::None));
		OutDetails.BlendingFilter = EPCGExAttributeFilter::Include;

		AttributesInfos->FindMissing(Attributes, OutMissingAttributes);

		for (const FName& Id : Attributes)
		{
			if (OutMissingAttributes.Contains(Id)) { continue; }

			OutDetails.AttributesOverrides.Add(Id, DefaultBlending);
			OutDetails.FilteredAttributes.Add(Id);
		}
	}

	void AssembleBlendingDetails(const EPCGExBlendingType& DefaultBlending, const TArray<FName>& Attributes, const TArray<TSharedRef<PCGExData::FFacade>>& InSources, FPCGExBlendingDetails& OutDetails, TSet<FName>& OutMissingAttributes)
	{
		OutDetails = FPCGExBlendingDetails(FPCGExPropertiesBlendingDetails(EPCGExBlendingType::None));
		OutDetails.BlendingFilter = EPCGExAttributeFilter::Include;

		for (const TSharedRef<PCGExData::FFacade>& Facade : InSources)
		{
			const TSharedPtr<PCGEx::FAttributesInfos> AttributesInfos = PCGEx::FAttributesInfos::Get(Facade->Source->GetIn()->Metadata);
			AttributesInfos->FindMissing(Attributes, OutMissingAttributes);

			for (const FName& Id : Attributes)
			{
				if (OutMissingAttributes.Contains(Id)) { continue; }

				OutDetails.AttributesOverrides.Add(Id, DefaultBlending);
				OutDetails.FilteredAttributes.Add(Id);
			}
		}
	}

	void GetFilteredIdentities(const UPCGMetadata* InMetadata, TArray<PCGEx::FAttributeIdentity>& OutIdentities, const FPCGExBlendingDetails* InBlendingDetails, const FPCGExCarryOverDetails* InCarryOverDetails, const TSet<FName>* IgnoreAttributeSet)
	{
		PCGEx::FAttributeIdentity::Get(InMetadata, OutIdentities, IgnoreAttributeSet);
		if (InCarryOverDetails) { InCarryOverDetails->Prune(OutIdentities); }
		if (InBlendingDetails) { InBlendingDetails->Filter(OutIdentities); }
	}
}

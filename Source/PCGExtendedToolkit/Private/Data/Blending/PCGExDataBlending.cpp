// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExDataBlending.h"

FPCGExPropertiesBlendingDetails::FPCGExPropertiesBlendingDetails(const EPCGExDataBlendingType InDefaultBlending)
	: DefaultBlending(InDefaultBlending)
{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_TYPE, _NAME, _TYPENAME) _NAME##Blending = InDefaultBlending;
	PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY
}

bool FPCGExPropertiesBlendingDetails::HasNoBlending() const
{
	return DensityBlending == EPCGExDataBlendingType::None &&
		BoundsMinBlending == EPCGExDataBlendingType::None &&
		BoundsMaxBlending == EPCGExDataBlendingType::None &&
		ColorBlending == EPCGExDataBlendingType::None &&
		PositionBlending == EPCGExDataBlendingType::None &&
		RotationBlending == EPCGExDataBlendingType::None &&
		ScaleBlending == EPCGExDataBlendingType::None &&
		SteepnessBlending == EPCGExDataBlendingType::None &&
		SeedBlending == EPCGExDataBlendingType::None;
}

void FPCGExPropertiesBlendingDetails::GetNonNoneBlendings(TArray<FName>& OutNames) const
{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_TYPE, _NAME, _TYPENAME) if(_NAME##Blending != EPCGExDataBlendingType::None){ OutNames.Add(#_NAME);}
	PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY
}

FPCGExBlendingDetails::FPCGExBlendingDetails(const EPCGExDataBlendingType InDefaultBlending)
	: DefaultBlending(InDefaultBlending)
{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_TYPE, _NAME, _TYPENAME) PropertiesOverrides._NAME##Blending = InDefaultBlending;
	PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY
}

FPCGExBlendingDetails::FPCGExBlendingDetails(const EPCGExDataBlendingType InDefaultBlending, const EPCGExDataBlendingType InPositionBlending)
	: DefaultBlending(InDefaultBlending)
{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_TYPE, _NAME, _TYPENAME) PropertiesOverrides._NAME##Blending = InDefaultBlending;
	PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY

	PropertiesOverrides.bOverridePosition = true;
	PropertiesOverrides.PositionBlending = InPositionBlending;
}

FPCGExBlendingDetails::FPCGExBlendingDetails(const FPCGExPropertiesBlendingDetails& InDetails)
	: DefaultBlending(InDetails.DefaultBlending)
{
#define PCGEX_SET_DEFAULT_POINTPROPERTY(_TYPE, _NAME, _TYPENAME) PropertiesOverrides.bOverride##_NAME = InDetails._NAME##Blending != EPCGExDataBlendingType::None; PropertiesOverrides._NAME##Blending = InDetails._NAME##Blending;
	PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_DEFAULT_POINTPROPERTY)
#undef PCGEX_SET_DEFAULT_POINTPROPERTY
}

FPCGExPropertiesBlendingDetails FPCGExBlendingDetails::GetPropertiesBlendingDetails() const
{
	FPCGExPropertiesBlendingDetails OutDetails;
#define PCGEX_SET_POINTPROPERTY(_TYPE, _NAME, _TYPENAME) OutDetails._NAME##Blending = PropertiesOverrides.bOverride##_NAME ? PropertiesOverrides._NAME##Blending : DefaultBlending;
	PCGEX_FOREACH_BLEND_POINTPROPERTY(PCGEX_SET_POINTPROPERTY)
#undef PCGEX_SET_POINTPROPERTY
	return OutDetails;
}

bool FPCGExBlendingDetails::HasAnyBlending() const
{
	return !FilteredAttributes.IsEmpty() || !GetPropertiesBlendingDetails().HasNoBlending();
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

void FPCGExBlendingDetails::RegisterBuffersDependencies(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade, PCGExData::FFacadePreloader& FacadePreloader, const TSet<FName>* IgnoredAttributes) const
{
	const TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(InDataFacade->GetIn()->Metadata, IgnoredAttributes);
	Filter(Infos->Identities);
	for (const PCGEx::FAttributeIdentity& Identity : Infos->Identities) { FacadePreloader.Register(InContext, Identity); }
}

namespace PCGExDataBlending
{
	void FDataBlendingProcessorBase::PrepareForData(const TSharedPtr<PCGExData::FFacade>& InPrimaryFacade, const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade, const PCGExData::ESource SecondarySource)
	{
		PrimaryData = InPrimaryFacade->Source->GetOut();
		SecondaryData = InSecondaryFacade->Source->GetData(SecondarySource);
	}

	void FDataBlendingProcessorBase::PrepareForData(const TSharedPtr<PCGExData::FBufferBase>& InWriter, const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade, const PCGExData::ESource SecondarySource)
	{
		PrimaryData = nullptr;
		SecondaryData = InSecondaryFacade->Source->GetData(SecondarySource);
	}

	void FDataBlendingProcessorBase::SoftPrepareForData(const TSharedPtr<PCGExData::FFacade>& InPrimaryFacade, const TSharedPtr<PCGExData::FFacade>& InSecondaryFacade, const PCGExData::ESource SecondarySource)
	{
		PrimaryData = InPrimaryFacade->Source->GetOut();
		SecondaryData = InSecondaryFacade->Source->GetData(SecondarySource);
	}

	void FDataBlendingProcessorBase::DoOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const int32 WriteIndex, const double Weight, const int8 bFirstOperation) const
	{
		TArray<double> Weights = {Weight};
		DoRangeOperation(PrimaryReadIndex, SecondaryReadIndex, WriteIndex, Weights, bFirstOperation);
	}

	void FDataBlendingProcessorBase::CompleteOperation(const int32 WriteIndex, const int32 Count, const double TotalWeight) const
	{
		TArray<double> Weights = {TotalWeight};
		TArray<int32> Counts = {Count};
		CompleteRangeOperation(WriteIndex, Counts, Weights);
	}

	void PCGExDataBlending::AssembleBlendingDetails(const FPCGExPropertiesBlendingDetails& PropertiesBlending, const TMap<FName, EPCGExDataBlendingType>& PerAttributeBlending, const TSharedRef<PCGExData::FPointIO>& SourceIO, FPCGExBlendingDetails& OutDetails, TSet<FName>& OutMissingAttributes)
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

	void PCGExDataBlending::AssembleBlendingDetails(const EPCGExDataBlendingType& DefaultBlending, const TArray<FName>& Attributes, const TSharedRef<PCGExData::FPointIO>& SourceIO, FPCGExBlendingDetails& OutDetails, TSet<FName>& OutMissingAttributes)
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

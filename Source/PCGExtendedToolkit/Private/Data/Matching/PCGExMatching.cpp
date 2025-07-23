// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Data/Matching/PCGExMatching.h"

#include "PCGExCompare.h"
#include "PCGExDetails.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTag.h"

bool FPCGExAttributeToTagComparisonDetails::Init(const FPCGContext* InContext, const TSharedRef<PCGExData::FFacade>& InSourceDataFacade)
{
	if (TagNameInput == EPCGExInputValueType::Attribute)
	{
		TagNameGetter = MakeShared<PCGEx::TAttributeBroadcaster<FString>>();
		if (!TagNameGetter->Prepare(TagNameAttribute, InSourceDataFacade->Source))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Invalid tag name attribute."));
			return false;
		}
	}

	if (!bDoValueMatch) { return true; }

	switch (ValueType)
	{
	case EPCGExComparisonDataType::Numeric:
		NumericValueGetter = MakeShared<PCGEx::TAttributeBroadcaster<double>>();
		if (!NumericValueGetter->Prepare(ValueAttribute, InSourceDataFacade->Source))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Invalid tag value attribute."));
			return false;
		}
		break;
	case EPCGExComparisonDataType::String:
		StringValueGetter = MakeShared<PCGEx::TAttributeBroadcaster<FString>>();
		if (!StringValueGetter->Prepare(ValueAttribute, InSourceDataFacade->Source))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Invalid tag value attribute."));
			return false;
		}
		break;
	}

	return true;
}

bool FPCGExAttributeToTagComparisonDetails::Matches(const TSharedPtr<PCGExData::FPointIO>& InData, const PCGExData::FConstPoint& SourcePoint) const
{
	const FString TestTagName = TagNameGetter ? TagNameGetter->FetchSingle(SourcePoint, TEXT("")) : TagName;

	if (!bDoValueMatch)
	{
		return PCGExCompare::HasMatchingTags(InData->Tags, TagNameGetter ? TagNameGetter->FetchSingle(SourcePoint, TEXT("")) : TagName, NameMatch);
	}


	TArray<TSharedPtr<PCGExData::IDataValue>> TagValues;
	if (!PCGExCompare::GetMatchingValueTags(InData->Tags, TestTagName, NameMatch, TagValues)) { return false; }

	if (ValueType == EPCGExComparisonDataType::Numeric)
	{
		const double OperandBNumeric = NumericValueGetter->FetchSingle(SourcePoint, 0);
		for (const TSharedPtr<PCGExData::IDataValue>& TagValue : TagValues)
		{
			if (!PCGExCompare::Compare(NumericComparison, TagValue, OperandBNumeric, Tolerance)) { return false; }
		}
	}
	else
	{
		const FString OperandBString = StringValueGetter->FetchSingle(SourcePoint, TEXT(""));
		for (const TSharedPtr<PCGExData::IDataValue>& TagValue : TagValues)
		{
			if (!PCGExCompare::Compare(StringComparison, TagValue, OperandBString)) { return false; }
		}
	}

	return true;
}

void FPCGExAttributeToTagComparisonDetails::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	InContext->AddConsumableAttributeName(TagNameAttribute);

	FName Consumable = NAME_None;
	PCGEX_CONSUMABLE_SELECTOR(ValueAttribute, Consumable)
}

bool FPCGExAttributeToTagComparisonDetails::GetOnlyUseDataDomain() const
{
	return TagNameInput == EPCGExInputValueType::Constant &&
		PCGExHelpers::IsDataDomainAttribute(ValueAttribute);
}

bool FPCGExAttributeToDataComparisonDetails::Init(const FPCGContext* InContext, const TSharedRef<PCGExData::FFacade>& InSourceDataFacade)
{
	if (DataNameInput == EPCGExInputValueType::Attribute)
	{
		DataNameGetter = MakeShared<PCGEx::TAttributeBroadcaster<FName>>();
		if (!DataNameGetter->Prepare(DataNameAttribute, InSourceDataFacade->Source))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Invalid tag name attribute."));
			return false;
		}
	}

	switch (Check)
	{
	case EPCGExComparisonDataType::Numeric:
		NumericValueGetter = MakeShared<PCGEx::TAttributeBroadcaster<double>>();
		if (!NumericValueGetter->Prepare(ValueNameAttribute, InSourceDataFacade->Source))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Invalid tag value attribute."));
			return false;
		}
		break;
	case EPCGExComparisonDataType::String:
		StringValueGetter = MakeShared<PCGEx::TAttributeBroadcaster<FString>>();
		if (!StringValueGetter->Prepare(ValueNameAttribute, InSourceDataFacade->Source))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Invalid tag value attribute."));
			return false;
		}
		break;
	}

	return true;
}

bool FPCGExAttributeToDataComparisonDetails::Matches(const TSharedPtr<PCGExData::FPointIO>& InData, const PCGExData::FConstPoint& SourcePoint) const
{
	FPCGAttributeIdentifier Identifier = PCGEx::GetAttributeIdentifier(DataNameGetter ? DataNameGetter->FetchSingle(SourcePoint, NAME_None) : DataName, InData->GetIn());
	Identifier.MetadataDomain = PCGMetadataDomainID::Data;

	const FPCGMetadataAttributeBase* Attribute = InData->FindConstAttribute(Identifier);
	if (!Attribute) { return false; }

	if (Check == EPCGExComparisonDataType::Numeric)
	{
		return PCGExCompare::Compare(
			NumericCompare, PCGExDataHelpers::ReadDataValue<double>(Attribute, 0),
			NumericValueGetter->FetchSingle(SourcePoint, 0), Tolerance);
	}
	return PCGExCompare::Compare(
		StringCompare, PCGExDataHelpers::ReadDataValue<FString>(Attribute, TEXT("")),
		StringValueGetter->FetchSingle(SourcePoint, TEXT("")));
}

void FPCGExAttributeToDataComparisonDetails::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	InContext->AddConsumableAttributeName(DataNameAttribute);
	InContext->AddConsumableAttributeName(ValueNameAttribute);
}

bool FPCGExAttributeToDataComparisonDetails::GetOnlyUseDataDomain() const
{
	return DataNameInput == EPCGExInputValueType::Constant &&
		PCGExHelpers::IsDataDomainAttribute(ValueNameAttribute);
}

void PCGExMatching::DeclareMatchingRulesInputs(const FPCGExMatchingDetails& InDetails, TArray<FPCGPinProperties>& PinProperties, const EPCGPinStatus InStatus)
{
	if (InDetails.Mode == EPCGExMapMatchMode::Disabled) { return; }

	FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(PCGExMatching::SourceMatchRulesLabel, EPCGDataType::Param);
	Pin.Tooltip = FTEXT("Matching rules to determine which target can be sampled by each input.");
	Pin.PinStatus = InStatus;
}

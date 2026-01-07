// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Utils/PCGExDataFilterDetails.h"

#include "Algo/RemoveIf.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGExArrayHelpers.h"
#include "Types/PCGExAttributeIdentity.h"

void FPCGExNameFiltersDetails::Init()
{
	for (const TArray<FString> Names = PCGExArrayHelpers::GetStringArrayFromCommaSeparatedList(CommaSeparatedNames); const FString& Name : Names) { Matches.Add(Name, CommaSeparatedNameFilter); }
}

bool FPCGExNameFiltersDetails::Test(const FString& Name) const

{
	if (bPreservePCGExData && Name.StartsWith(PCGExCommon::PCGExPrefix)) { return !bFilterToRemove; }

	switch (FilterMode)
	{
	default: ;
	case EPCGExAttributeFilter::All: return true;
	case EPCGExAttributeFilter::Exclude: for (const TPair<FString, EPCGExStringMatchMode>& Filter : Matches)
		{
			switch (Filter.Value)
			{
			case EPCGExStringMatchMode::Equals: if (Filter.Key == Name) { return false; }
				break;
			case EPCGExStringMatchMode::Contains: if (Name.Contains(Filter.Key)) { return false; }
				break;
			case EPCGExStringMatchMode::StartsWith: if (Name.StartsWith(Filter.Key)) { return false; }
				break;
			case EPCGExStringMatchMode::EndsWith: if (Name.EndsWith(Filter.Key)) { return false; }
				break;
			default: ;
			}
		}
		return true;
	case EPCGExAttributeFilter::Include: for (const TPair<FString, EPCGExStringMatchMode>& Filter : Matches)
		{
			switch (Filter.Value)
			{
			case EPCGExStringMatchMode::Equals: if (Filter.Key == Name) { return true; }
				break;
			case EPCGExStringMatchMode::Contains: if (Name.Contains(Filter.Key)) { return true; }
				break;
			case EPCGExStringMatchMode::StartsWith: if (Name.StartsWith(Filter.Key)) { return true; }
				break;
			case EPCGExStringMatchMode::EndsWith: if (Name.EndsWith(Filter.Key)) { return true; }
				break;
			default: ;
			}
		}
		return false;
	}
}

bool FPCGExNameFiltersDetails::Test(const FPCGMetadataAttributeBase* InAttribute) const
{
	return Test(InAttribute->Name.ToString());
}

void FPCGExNameFiltersDetails::Prune(TArray<FString>& Names, bool bInvert) const
{
	if (bInvert)
	{
		for (int i = 0; i < Names.Num(); i++)
		{
			if (Test(Names[i]))
			{
				Names.RemoveAt(i);
				i--;
			}
		}
	}
	else
	{
		for (int i = 0; i < Names.Num(); i++)
		{
			if (!Test(Names[i]))
			{
				Names.RemoveAt(i);
				i--;
			}
		}
	}
}

void FPCGExNameFiltersDetails::Prune(TSet<FName>& Names, bool bInvert) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExNameFiltersDetails::Prune);
	TArray<FName> ValidNames;
	ValidNames.Reserve(Names.Num());
	if (bInvert) { for (FName Name : Names) { if (!Test(Name.ToString())) { ValidNames.Add(Name); } } }
	else { for (FName Name : Names) { if (Test(Name.ToString())) { ValidNames.Add(Name); } } }
	Names.Empty();
	Names.Append(ValidNames);
}

void FPCGExNameFiltersDetails::Prune(PCGExData::FAttributesInfos& InAttributeInfos, const bool bInvert) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExNameFiltersDetails::Prune);
	if (bInvert) { InAttributeInfos.Filter([&](const FName& InName) { return Test(InName.ToString()); }); }
	else { InAttributeInfos.Filter([&](const FName& InName) { return !Test(InName.ToString()); }); }
}

FPCGExAttributeGatherDetails::FPCGExAttributeGatherDetails()
{
	bPreservePCGExData = false;
}

void FPCGExCarryOverDetails::Init()
{
	Attributes.Init();
	Tags.Init();
}

void FPCGExCarryOverDetails::Prune(TSet<FString>& InValues) const
{
	if (Tags.FilterMode == EPCGExAttributeFilter::All) { return; }
	for (TArray<FString> TagList = InValues.Array(); const FString& Tag : TagList) { if (!Tags.Test(Tag)) { InValues.Remove(Tag); } }
}

void FPCGExCarryOverDetails::Prune(TArray<FString>& InValues) const
{
	if (Tags.FilterMode == EPCGExAttributeFilter::All) { return; }
	InValues.SetNum(Algo::StableRemoveIf(
		InValues,
		[&](const FString& Value)
		{
			return Tags.Test(Value);
		}));
}

void FPCGExCarryOverDetails::Prune(const PCGExData::FPointIO* PointIO) const
{
	Prune(PointIO->GetOut()->Metadata);
	Prune(PointIO->Tags.Get());
}

void FPCGExCarryOverDetails::Prune(TArray<PCGExData::FAttributeIdentity>& Identities) const
{
	if (Attributes.FilterMode == EPCGExAttributeFilter::All) { return; }
	Identities.SetNum(Algo::StableRemoveIf(
		Identities,
		[&](const PCGExData::FAttributeIdentity& Identity)
		{
			return Attributes.Test(Identity.Identifier.Name.ToString());
		}));
}

void FPCGExCarryOverDetails::Prune(PCGExData::FTags* InTags) const
{
	if (Tags.FilterMode == EPCGExAttributeFilter::All) { return; }

	TSet<FString> ToBeRemoved;
	ToBeRemoved.Reserve(InTags->Num());

	if (bTestTagsWithValues)
	{
		// Test flattened tags; this is rather expensive.
		for (TSet<FString> Flattened = InTags->Flatten(); const FString& Tag : Flattened) { if (!Tags.Test(Tag)) { ToBeRemoved.Add(Tag); } }
	}
	else
	{
		for (const FString& Tag : InTags->RawTags) { if (!Tags.Test(Tag)) { ToBeRemoved.Add(Tag); } }
		for (const TPair<FString, TSharedPtr<PCGExData::IDataValue>>& Pair : InTags->ValueTags) { if (!Tags.Test(Pair.Key)) { ToBeRemoved.Add(Pair.Key); } }
	}

	InTags->Remove(ToBeRemoved);
}

bool FPCGExCarryOverDetails::Test(const PCGExData::FPointIO* PointIO) const
{
	if (!Test(PointIO->GetOutIn()->Metadata)) { return false; }
	if (!Test(PointIO->Tags.Get())) { return false; }
	return true;
}

bool FPCGExCarryOverDetails::Test(PCGExData::FTags* InTags) const
{
	if (Tags.FilterMode == EPCGExAttributeFilter::All) { return true; }

	if (bTestTagsWithValues)
	{
		// Test flattened tags; this is rather expensive.
		for (TSet<FString> Flattened = InTags->Flatten(); const FString& Tag : Flattened) { if (!Tags.Test(Tag)) { return false; } }
	}
	else
	{
		for (const FString& Tag : InTags->RawTags) { if (!Tags.Test(Tag)) { return false; } }
		for (const TPair<FString, TSharedPtr<PCGExData::IDataValue>>& Pair : InTags->ValueTags) { if (!Tags.Test(Pair.Key)) { return false; } }
	}

	return true;
}

void FPCGExCarryOverDetails::Prune(UPCGMetadata* Metadata) const
{
	if (Attributes.FilterMode == EPCGExAttributeFilter::All) { return; }

	TArray<FPCGAttributeIdentifier> Identifiers;
	TArray<EPCGMetadataTypes> Types;
	Metadata->GetAllAttributes(Identifiers, Types);
	for (const FPCGAttributeIdentifier& Identifier : Identifiers) { if (!Attributes.Test(Identifier.Name.ToString())) { Metadata->DeleteAttribute(Identifier); } }
}

bool FPCGExCarryOverDetails::Test(const UPCGMetadata* Metadata) const
{
	if (Attributes.FilterMode == EPCGExAttributeFilter::All) { return true; }

	TArray<FPCGAttributeIdentifier> Identifiers;
	TArray<EPCGMetadataTypes> Types;
	Metadata->GetAllAttributes(Identifiers, Types);
	if (Attributes.FilterMode == EPCGExAttributeFilter::Exclude)
	{
		for (const FPCGAttributeIdentifier& Identifier : Identifiers) { if (!Attributes.Test(Identifier.Name.ToString())) { return false; } }
		return true;
	}
	for (const FPCGAttributeIdentifier& Identifier : Identifiers) { if (Attributes.Test(Identifier.Name.ToString())) { return true; } }
	return false;
}

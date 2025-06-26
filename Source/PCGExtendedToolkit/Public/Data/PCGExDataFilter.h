// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExAttributeHelpers.h"
#include "PCGExCompare.h"
#include "PCGExPointIO.h"

#include "PCGExDataFilter.generated.h"

class UPCGMetadata;
enum class EPCGMetadataTypes : uint8;

UENUM()
enum class EPCGExAttributeFilter : uint8
{
	All     = 0 UMETA(DisplayName = "All", ToolTip="All attributes"),
	Exclude = 1 UMETA(DisplayName = "Exclude", ToolTip="Exclude listed attributes"),
	Include = 2 UMETA(DisplayName = "Include", ToolTip="Only listed attributes"),
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExNameFiltersDetails
{
	GENERATED_BODY()

	FPCGExNameFiltersDetails()
	{
	}

	explicit FPCGExNameFiltersDetails(const bool FilterToRemove)
		: bFilterToRemove(FilterToRemove)
	{
	}

	bool bFilterToRemove = false;

	/** How the names are processed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExAttributeFilter FilterMode = EPCGExAttributeFilter::All;

	/** List of matches that will be checked. Any success is a pass. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="FilterMode != EPCGExAttributeFilter::All", EditConditionHides))
	TMap<FString, EPCGExStringMatchMode> Matches;

	/** A list of names separated by a comma, for easy overrides. The limitation is that they all use the same shared filter mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FString CommaSeparatedNames;

	/** Unique filter mode applied to comma separated names */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="FilterMode != EPCGExAttributeFilter::All", EditConditionHides))
	EPCGExStringMatchMode CommaSeparatedNameFilter = EPCGExStringMatchMode::Equals;

	/** If enabled, PCGEx attributes & tags won't be affected.  Cluster-related nodes rely on these to work! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bPreservePCGExData = true;

	void Init()
	{
		for (const TArray<FString> Names = PCGExHelpers::GetStringArrayFromCommaSeparatedList(CommaSeparatedNames);
		     const FString& Name : Names) { Matches.Add(Name, CommaSeparatedNameFilter); }
	}

	bool Test(const FString& Name) const
	{
		switch (FilterMode)
		{
		default: ;
		case EPCGExAttributeFilter::All:
			return true;
		case EPCGExAttributeFilter::Exclude:
			if (bPreservePCGExData && Name.StartsWith(PCGEx::PCGExPrefix)) { return !bFilterToRemove; }
			for (const TPair<FString, EPCGExStringMatchMode>& Filter : Matches)
			{
				switch (Filter.Value)
				{
				case EPCGExStringMatchMode::Equals:
					if (Filter.Key == Name) { return false; }
					break;
				case EPCGExStringMatchMode::Contains:
					if (Name.Contains(Filter.Key)) { return false; }
					break;
				case EPCGExStringMatchMode::StartsWith:
					if (Name.StartsWith(Filter.Key)) { return false; }
					break;
				case EPCGExStringMatchMode::EndsWith:
					if (Name.EndsWith(Filter.Key)) { return false; }
					break;
				default: ;
				}
			}
			return true;
		case EPCGExAttributeFilter::Include:
			if (bPreservePCGExData && Name.StartsWith(PCGEx::PCGExPrefix)) { return !bFilterToRemove; }
			for (const TPair<FString, EPCGExStringMatchMode>& Filter : Matches)
			{
				switch (Filter.Value)
				{
				case EPCGExStringMatchMode::Equals:
					if (Filter.Key == Name) { return true; }
					break;
				case EPCGExStringMatchMode::Contains:
					if (Name.Contains(Filter.Key)) { return true; }
					break;
				case EPCGExStringMatchMode::StartsWith:
					if (Name.StartsWith(Filter.Key)) { return true; }
					break;
				case EPCGExStringMatchMode::EndsWith:
					if (Name.EndsWith(Filter.Key)) { return true; }
					break;
				default: ;
				}
			}
			return false;
		}
	}

	bool Test(const FPCGMetadataAttributeBase* InAttribute) const
	{
		return Test(InAttribute->Name.ToString());
	}

	void Prune(TArray<FString>& Names, bool bInvert = false) const
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

	void Prune(TSet<FName>& Names, bool bInvert = false) const
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExNameFiltersDetails::Prune);
		TArray<FName> ValidNames;
		ValidNames.Reserve(Names.Num());
		if (bInvert) { for (FName Name : Names) { if (!Test(Name.ToString())) { ValidNames.Add(Name); } } }
		else { for (FName Name : Names) { if (Test(Name.ToString())) { ValidNames.Add(Name); } } }
		Names.Empty();
		Names.Append(ValidNames);
	}

	void Prune(PCGEx::FAttributesInfos& InAttributeInfos, const bool bInvert = false) const
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExNameFiltersDetails::Prune);
		if (bInvert) { InAttributeInfos.Filter([&](const FName& InName) { return Test(InName.ToString()); }); }
		else { InAttributeInfos.Filter([&](const FName& InName) { return !Test(InName.ToString()); }); }
	}
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExAttributeGatherDetails : public FPCGExNameFiltersDetails
{
	GENERATED_BODY()

	FPCGExAttributeGatherDetails()
	{
		bPreservePCGExData = false;
	}

	// TODO : Expose how to handle overlaps
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExCarryOverDetails
{
	GENERATED_BODY()

	FPCGExCarryOverDetails()
	{
	}

	/** If enabled, will preserve the initial attribute default value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bPreserveAttributesDefaultValue = false;

	/** Attributes to carry over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExNameFiltersDetails Attributes = FPCGExNameFiltersDetails(false);

	/** If enabled, will convert data domain attributes to elements domain ones. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Data domain to Elements"))
	bool bDataDomainToElements = true;

	/** Tags to carry over. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExNameFiltersDetails Tags = FPCGExNameFiltersDetails(false);

	/** If enabled, will test full tag with its value ('Tag:Value'), otherwise only test the left part ignoring the right `:Value` ('Tag'). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Flatten tag value"))
	bool bTestTagsWithValues = false;

	void Init()
	{
		Attributes.Init();
		Tags.Init();
	}

	void Prune(TSet<FString>& InValues) const
	{
		if (Tags.FilterMode == EPCGExAttributeFilter::All) { return; }
		for (TArray<FString> TagList = InValues.Array(); const FString& Tag : TagList) { if (!Tags.Test(Tag)) { InValues.Remove(Tag); } }
	}

	void Prune(TArray<FString>& InValues) const
	{
		if (Tags.FilterMode == EPCGExAttributeFilter::All) { return; }

		int32 WriteIndex = 0;
		for (int32 i = 0; i < InValues.Num(); i++) { if (Tags.Test(InValues[i])) { InValues[WriteIndex++] = InValues[i]; } }
		InValues.SetNum(WriteIndex);
	}

	void Prune(const PCGExData::FPointIO* PointIO) const
	{
		Prune(PointIO->GetOut()->Metadata);
		Prune(PointIO->Tags.Get());
	}

	void Prune(TArray<PCGEx::FAttributeIdentity>& Identities) const
	{
		if (Attributes.FilterMode == EPCGExAttributeFilter::All) { return; }

		int32 WriteIndex = 0;
		for (int32 i = 0; i < Identities.Num(); i++) { if (Attributes.Test(Identities[i].Identifier.Name.ToString())) { Identities[WriteIndex++] = Identities[i]; } }
		Identities.SetNum(WriteIndex);
	}

	void Prune(PCGExData::FTags* InTags) const
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

	bool Test(const PCGExData::FPointIO* PointIO) const
	{
		if (!Test(PointIO->GetOutIn()->Metadata)) { return false; }
		if (!Test(PointIO->Tags.Get())) { return false; }
		return true;
	}

	bool Test(PCGExData::FTags* InTags) const
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

	void Prune(UPCGMetadata* Metadata) const
	{
		if (Attributes.FilterMode == EPCGExAttributeFilter::All) { return; }

		TArray<FPCGAttributeIdentifier> Identifiers;
		TArray<EPCGMetadataTypes> Types;
		Metadata->GetAllAttributes(Identifiers, Types);
		for (const FPCGAttributeIdentifier& Identifier : Identifiers) { if (!Attributes.Test(Identifier.Name.ToString())) { Metadata->DeleteAttribute(Identifier); } }
	}

	bool Test(const UPCGMetadata* Metadata) const
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
};

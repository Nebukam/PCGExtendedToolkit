// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Utils/PCGExCompare.h"
#include "UObject/Object.h"

#include "PCGExDataFilterDetails.generated.h"

namespace PCGExData
{
	class FPointIO;
}

class FPCGMetadataAttributeBase;

namespace PCGExData
{
	struct FAttributeIdentity;
	class FAttributesInfos;
}

class UPCGMetadata;
enum class EPCGMetadataTypes : uint8;

UENUM()
enum class EPCGExAttributeFilter : uint8
{
	All     = 0 UMETA(DisplayName = "All", ToolTip="All elements", ActionIcon="All"),
	Exclude = 1 UMETA(DisplayName = "Exclude", ToolTip="Discard listed elements, keep the others", ActionIcon="Exclude"),
	Include = 2 UMETA(DisplayName = "Include", ToolTip="Keep listed elements, discard the others", ActionIcon="Include"),
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExNameFiltersDetails
{
	GENERATED_BODY()

	FPCGExNameFiltersDetails() = default;

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="FilterMode != EPCGExAttributeFilter::All", EditConditionHides))
	EPCGExStringMatchMode CommaSeparatedNameFilter = EPCGExStringMatchMode::Equals;

	/** If enabled, PCGEx attributes & tags won't be affected.  Cluster-related nodes rely on these to work! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bPreservePCGExData = true;

	void Init();

	bool Test(const FString& Name) const;
	bool Test(const FPCGMetadataAttributeBase* InAttribute) const;

	void Prune(TArray<FString>& Names, bool bInvert = false) const;
	void Prune(TSet<FName>& Names, bool bInvert = false) const;
	void Prune(PCGExData::FAttributesInfos& InAttributeInfos, const bool bInvert = false) const;
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExAttributeGatherDetails : public FPCGExNameFiltersDetails
{
	GENERATED_BODY()

	FPCGExAttributeGatherDetails();

	// TODO : Expose how to handle overlaps
};

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExCarryOverDetails
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

	void Init();

	void Prune(TSet<FString>& InValues) const;
	void Prune(TArray<FString>& InValues) const;
	void Prune(const PCGExData::FPointIO* PointIO) const;
	void Prune(TArray<PCGExData::FAttributeIdentity>& Identities) const;
	void Prune(PCGExData::FTags* InTags) const;

	bool Test(const PCGExData::FPointIO* PointIO) const;
	bool Test(PCGExData::FTags* InTags) const;

	void Prune(UPCGMetadata* Metadata) const;

	bool Test(const UPCGMetadata* Metadata) const;
};

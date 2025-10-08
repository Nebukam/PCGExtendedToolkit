﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"

#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExFilterGroup.h"


#include "Misc/Filters/PCGExFilterFactoryProvider.h"


#include "PCGExAttributeCheckFilter.generated.h"

UENUM()
enum class EPCGExAttribtueDomainCheck : uint8
{
	Any      = 0 UMETA(DisplayName = "Any", Tooltip="Ignore domain check"),
	Data     = 1 UMETA(DisplayName = "Data", Tooltip="Check data domain"),
	Elements = 2 UMETA(DisplayName = "Elements", Tooltip="Check elements domain"),
	Match    = 3 UMETA(DisplayName = "Match", Tooltip="Domains must match (must be set as part of the attribute name)"),
};

USTRUCT(BlueprintType)
struct FPCGExAttributeCheckFilterConfig
{
	GENERATED_BODY()

	FPCGExAttributeCheckFilterConfig()
	{
	}

	/** Constant tag name value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Attribute Name"))
	FString AttributeName = TEXT("Name");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExAttribtueDomainCheck Domain = EPCGExAttribtueDomainCheck::Any;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName="Match"))
	EPCGExStringMatchMode Match = EPCGExStringMatchMode::Equals;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, InlineEditConditionToggle))
	bool bDoCheckType = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, EditCondition="bDoCheckType"))
	EPCGMetadataTypes Type = EPCGMetadataTypes::Unknown;

	/** Invert the result of this filter. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bInvert = false;
};


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter", meta=(PCGExNodeLibraryDoc="filters/filters-collections/attribute-check"))
class UPCGExAttributeCheckFilterFactory : public UPCGExFilterCollectionFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExAttributeCheckFilterConfig Config;

	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const override;
};

namespace PCGExPointFilter
{
	class FAttributeCheckFilter final : public ICollectionFilter
	{
	public:
		explicit FAttributeCheckFilter(const TObjectPtr<const UPCGExAttributeCheckFilterFactory>& InDefinition)
			: ICollectionFilter(InDefinition), TypedFilterFactory(InDefinition)
		{
		}

		const TObjectPtr<const UPCGExAttributeCheckFilterFactory> TypedFilterFactory;

		virtual bool Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const override;
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class UPCGExAttributeCheckFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

protected:
	PCGEX_FACTORY_TYPE_ID(FPCGExDataTypeInfoFilterCollection)

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		AttributeCheckFilterFactory, "C-Filter : Attribute Check", "Simple attribute existence check.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExAttributeCheckFilterConfig Config;

	virtual FName GetMainOutputPin() const override { return PCGExPointFilter::OutputColFilterLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

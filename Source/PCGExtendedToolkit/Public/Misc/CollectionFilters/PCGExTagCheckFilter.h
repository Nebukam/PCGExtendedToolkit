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


#include "PCGExTagCheckFilter.generated.h"


USTRUCT(BlueprintType)
struct FPCGExTagCheckFilterConfig
{
	GENERATED_BODY()

	FPCGExTagCheckFilterConfig()
	{
	}

	/** Constant tag name value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Tag Name"))
	FString Tag = TEXT("Tag");

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName="Match"))
	EPCGExStringMatchMode Match = EPCGExStringMatchMode::Equals;

	/** In strict mode, only check tag prefix and ignore values for tags formatted as `Tag:Value`. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bStrict = false;

	/** Invert the result of this filter. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bInvert = false;
};


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class UPCGExTagCheckFilterFactory : public UPCGExFilterCollectionFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExTagCheckFilterConfig Config;

	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const override;
};

namespace PCGExPointFilter
{
	class FTagCheckFilter final : public ICollectionFilter
	{
	public:
		explicit FTagCheckFilter(const TObjectPtr<const UPCGExTagCheckFilterFactory>& InDefinition)
			: ICollectionFilter(InDefinition), TypedFilterFactory(InDefinition)
		{
		}

		const TObjectPtr<const UPCGExTagCheckFilterFactory> TypedFilterFactory;

		virtual bool Test(const TSharedPtr<PCGExData::FPointIO>& IO, const TSharedPtr<PCGExData::FPointIOCollection>& ParentCollection) const override;

		virtual ~FTagCheckFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter", meta=(PCGExNodeLibraryDoc="filters/filters-collections/tag-check"))
class UPCGExTagCheckFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

protected:
	PCGEX_FACTORY_TYPE_ID(FPCGExDataTypeInfoFilterCollection)

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		TagCheckFilterFactory, "C-Filter : Tag Check", "Simple tag check on the input collection.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExTagCheckFilterConfig Config;

	virtual FName GetMainOutputPin() const override { return PCGExPointFilter::OutputColFilterLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

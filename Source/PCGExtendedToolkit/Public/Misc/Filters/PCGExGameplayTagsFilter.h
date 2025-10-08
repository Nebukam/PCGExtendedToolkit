﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "PCGExFilterFactoryProvider.h"
#include "UObject/Object.h"

#include "Data/PCGExPointFilter.h"
#include "PCGExPointsProcessor.h"
#include "PropertyPathHelpers.h"


#include "PCGExGameplayTagsFilter.generated.h"

namespace PCGExData
{
	template <typename T>
	class TBuffer;
}

USTRUCT(BlueprintType)
struct FPCGExGameplayTagsFilterConfig
{
	GENERATED_BODY()

	FPCGExGameplayTagsFilterConfig()
	{
	}

	/** Name of the attribute that contains a path to an actor in the level, usually from a GetActorData PCG Node in point mode.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName ActorReference = FName("ActorReference");

	/** Path to the tag container to be tested, resolve from the actor reference as root. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FString PropertyPath = FString("Component.TagContainer");

	/** Query. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	FGameplayTagQuery TagQuery;

	/** Value the filter will return for point which actor reference cannot be resolved */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fallbacks")
	bool bFallbackMissingActor = false;

	/** Value the filter will return if the actor is found, but the property path could not be resolved  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fallbacks")
	bool bFallbackPropertyPath = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietMissingPropertyWarning = false;
};


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class UPCGExGameplayTagsFilterFactory : public UPCGExPointFilterFactoryData
{
	GENERATED_BODY()

public:
	FPCGExGameplayTagsFilterConfig Config;

	virtual bool SupportsCollectionEvaluation() const override { return false; }

	virtual TSharedPtr<PCGExPointFilter::IFilter> CreateFilter() const override;
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;
};

namespace PCGExPointFilter
{
	class FGameplayTagsFilter final : public ISimpleFilter
	{
	public:
		explicit FGameplayTagsFilter(const TObjectPtr<const UPCGExGameplayTagsFilterFactory>& InDefinition)
			: ISimpleFilter(InDefinition), TypedFilterFactory(InDefinition)
		{
		}

		const TObjectPtr<const UPCGExGameplayTagsFilterFactory> TypedFilterFactory;
		FCachedPropertyPath PropertyPath;
		TArray<FString> PathSegments;

		TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> ActorReferences;

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade) override;
		virtual bool Test(const int32 PointIndex) const override;

		virtual ~FGameplayTagsFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter", meta=(PCGExNodeLibraryDoc="filters/filters-points/gameplay-tags"))
class UPCGExGameplayTagsFilterProviderSettings : public UPCGExFilterProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		GameplayTagsFilterFactory, "Filter : GameplayTags", "Creates a filter definition that checks gameplay tags of an actor reference.",
		PCGEX_FACTORY_NAME_PRIORITY)
#endif
	//~End UPCGSettings

	/** Filter Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExGameplayTagsFilterConfig Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override { return TEXT("Gameplay Tags"); }
#endif
};

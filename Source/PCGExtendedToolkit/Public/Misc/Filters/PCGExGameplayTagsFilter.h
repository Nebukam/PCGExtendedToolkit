// Copyright 2024 Timothé Lapetite and contributors
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


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExGameplayTagsFilterConfig
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warning and Errors")
	bool bQuietMissingPropertyWarning = false;
};


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExGameplayTagsFilterFactory : public UPCGExFilterFactoryData
{
	GENERATED_BODY()

public:
	FPCGExGameplayTagsFilterConfig Config;

	virtual TSharedPtr<PCGExPointFilter::FFilter> CreateFilter() const override;
	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;
};

namespace PCGExPointsFilter
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FGameplayTagsFilter final : public PCGExPointFilter::FSimpleFilter
	{
	public:
		explicit FGameplayTagsFilter(const TObjectPtr<const UPCGExGameplayTagsFilterFactory>& InDefinition)
			: FSimpleFilter(InDefinition), TypedFilterFactory(InDefinition)
		{
		}

		const TObjectPtr<const UPCGExGameplayTagsFilterFactory> TypedFilterFactory;
		FCachedPropertyPath PropertyPath;
		TArray<FString> PathSegments;

#if PCGEX_ENGINE_VERSION == 503
		TSharedPtr<PCGExData::TBuffer<FString>> ActorReferences;
#else
		TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> ActorReferences;
#endif

		virtual bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade> InPointDataFacade) override;
		FORCEINLINE virtual bool Test(const int32 PointIndex) const override
		{
			AActor* TargetActor = TSoftObjectPtr<AActor>(ActorReferences->Read(PointIndex)).Get();
			if (!TargetActor) { return TypedFilterFactory->Config.bFallbackMissingActor; }

			const FCachedPropertyPath Path = FCachedPropertyPath(PathSegments);
			FGameplayTagContainer TagContainer;
			FProperty* Property = nullptr;

			if (!PropertyPathHelpers::GetPropertyValue(TargetActor, Path, TagContainer, Property))
			{
				if (!TypedFilterFactory->Config.bQuietMissingPropertyWarning)
				{
					UE_LOG(LogTemp, Warning, TEXT("GameplayTags filter could not resolve target property : \"%s\"."), *TypedFilterFactory->Config.PropertyPath);
				}
				return TypedFilterFactory->Config.bFallbackPropertyPath;
			}
			return TypedFilterFactory->Config.TagQuery.Matches(TagContainer);
		}

		virtual ~FGameplayTagsFilter() override
		{
		}
	};
}

///

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Filter")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExGameplayTagsFilterProviderSettings : public UPCGExFilterProviderSettings
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

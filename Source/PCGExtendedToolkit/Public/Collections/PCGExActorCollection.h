// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExAssetCollection.h"
#include "Engine/DataAsset.h"

#include "PCGExActorCollection.generated.h"

namespace PCGExAssetCollection
{
	enum class ELoadingFlags : uint8;
}

class UPCGExActorCollection;

USTRUCT(BlueprintType, DisplayName="[PCGEx] Actor Collection Entry")
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExActorCollectionEntry : public FPCGExAssetCollectionEntry
{
	GENERATED_BODY()

	FPCGExActorCollectionEntry()
	{
	}

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	TSoftClassPtr<AActor> Actor;

	/** If enabled, the cached bounds will only account for collicable components on the actor. */
	UPROPERTY(EditAnywhere, Category = "Settings|Bounds", meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	bool bOnlyCollidingComponents = false;

	/** If enabled, the cached bounds will also account for child actors. */
	UPROPERTY(EditAnywhere, Category = "Settings|Bounds", meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	bool bIncludeFromChildActors = true;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bIsSubCollection", EditConditionHides))
	TObjectPtr<UPCGExActorCollection> SubCollection;

	bool SameAs(const FPCGExActorCollectionEntry& Other) const
	{
		return
			SubCollection == Other.SubCollection &&
			Weight == Other.Weight &&
			Category == Other.Category &&
			Actor == Other.Actor;
	}

	virtual void GetAssetPaths(TSet<FSoftObjectPath>& OutPaths) const override;

	virtual bool Validate(const UPCGExAssetCollection* ParentCollection) override;
	virtual void UpdateStaging(const UPCGExAssetCollection* OwningCollection, int32 InInternalIndex, const bool bRecursive) override;
	virtual void SetAssetPath(const FSoftObjectPath& InPath) override;

#if WITH_EDITOR
	virtual void EDITOR_Sanitize() override;
#endif
};

UCLASS(BlueprintType, DisplayName="[PCGEx] Actor Collection")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExActorCollection : public UPCGExAssetCollection
{
	GENERATED_BODY()

	friend struct FPCGExActorCollectionEntry;

public:
#if WITH_EDITOR
	virtual void EDITOR_RefreshDisplayNames() override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(TitleProperty="DisplayName"))
	TArray<FPCGExActorCollectionEntry> Entries;

	PCGEX_ASSET_COLLECTION_BOILERPLATE(UPCGExActorCollection, FPCGExActorCollectionEntry)
};

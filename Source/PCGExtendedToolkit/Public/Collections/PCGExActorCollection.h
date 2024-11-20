// Copyright Timothé Lapetite 2024
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
	TSoftObjectPtr<AActor> Actor;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bIsSubCollection", EditConditionHides))
	TSoftObjectPtr<UPCGExActorCollection> SubCollection;

	TObjectPtr<UPCGExActorCollection> SubCollectionPtr;

	bool SameAs(const FPCGExActorCollectionEntry& Other) const
	{
		return
			SubCollectionPtr == Other.SubCollectionPtr &&
			Weight == Other.Weight &&
			Category == Other.Category &&
			Actor == Other.Actor;
	}

	virtual bool Validate(const UPCGExAssetCollection* ParentCollection) override;
	virtual void UpdateStaging(const UPCGExAssetCollection* OwningCollection, int32 InInternalIndex, const bool bRecursive) override;
	virtual void SetAssetPath(const FSoftObjectPath& InPath) override;

protected:
	virtual void OnSubCollectionLoaded() override;
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

	virtual void GetAssetPaths(TSet<FSoftObjectPath>& OutPaths, const PCGExAssetCollection::ELoadingFlags Flags) const override;
};

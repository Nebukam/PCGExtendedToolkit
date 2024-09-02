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

USTRUCT(BlueprintType, DisplayName="[PCGEx] Mesh Collection Entry")
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
	virtual void UpdateStaging(const UPCGExAssetCollection* OwningCollection, const bool bRecursive) override;
	virtual void SetAssetPath(FSoftObjectPath InPath) override;

protected:
	virtual void OnSubCollectionLoaded() override;
};

UCLASS(BlueprintType, DisplayName="[PCGEx] Mesh Collection")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExActorCollection : public UPCGExAssetCollection
{
	GENERATED_BODY()

	friend struct FPCGExActorCollectionEntry;

public:
	virtual void RebuildStagingData(const bool bRecursive) override;

#if WITH_EDITOR
	virtual bool EDITOR_IsCacheableProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void EDITOR_RefreshDisplayNames() override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(TitleProperty="DisplayName"))
	TArray<FPCGExActorCollectionEntry> Entries;

	FORCEINLINE virtual bool GetStaging(const FPCGExAssetStagingData*& OutStaging, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode) const override
	{
		return GetStagingTpl(OutStaging, Entries, Index, Seed, PickMode);
	}

	FORCEINLINE virtual bool GetStagingRandom(const FPCGExAssetStagingData*& OutStaging, const int32 Seed) const override
	{
		return GetStagingRandomTpl(OutStaging, Entries, Seed);
	}

	FORCEINLINE virtual bool GetStagingWeightedRandom(const FPCGExAssetStagingData*& OutStaging, const int32 Seed) const override
	{
		return GetStagingWeightedRandomTpl(OutStaging, Entries, Seed);
	}

	virtual UPCGExAssetCollection* GetCollectionFromAttributeSet(const FPCGContext* InContext, const UPCGParamData* InAttributeSet, const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging) const override;
	virtual UPCGExAssetCollection* GetCollectionFromAttributeSet(const FPCGContext* InContext, const FName InputPin, const FPCGExAssetAttributeSetDetails& Details, const bool bBuildStaging) const override;
	virtual void GetAssetPaths(TSet<FSoftObjectPath>& OutPaths, const PCGExAssetCollection::ELoadingFlags Flags) const override;

	virtual void BuildCache() override;
};

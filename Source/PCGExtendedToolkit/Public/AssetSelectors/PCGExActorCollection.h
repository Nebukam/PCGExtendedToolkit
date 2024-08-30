// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExAssetCollection.h"
#include "PCGExRandom.h"
#include "Engine/DataAsset.h"
#include "ISMPartition/ISMComponentDescriptor.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"

#include "PCGExActorCollection.generated.h"

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
#if WITH_EDITOR
	virtual void UpdateStaging(const UPCGExAssetCollection* OwningCollection, const bool bRecursive) override;
#endif

protected:
	virtual void OnSubCollectionLoaded() override;
};

UCLASS(BlueprintType, DisplayName="[PCGEx] Mesh Collection")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExActorCollection : public UPCGExAssetCollection
{
	GENERATED_BODY()

	friend struct FPCGExActorCollectionEntry;

public:
#if WITH_EDITOR
	virtual bool IsCacheableProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void RefreshDisplayNames() override;
	virtual void RefreshStagingData() override;
	virtual void RefreshStagingData_Recursive() override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(TitleProperty="DisplayName"))
	TArray<FPCGExActorCollectionEntry> Entries;

	FORCEINLINE virtual bool GetStaging(FPCGExAssetStagingData& OutStaging, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode) const override
	{
		return GetStagingTpl(OutStaging, Entries, Index, Seed, PickMode);
	}

	FORCEINLINE virtual bool GetStagingRandom(FPCGExAssetStagingData& OutStaging, const int32 Seed) const override
	{
		return GetStagingRandomTpl(OutStaging, Entries, Seed);
	}

	FORCEINLINE virtual bool GetStagingWeightedRandom(FPCGExAssetStagingData& OutStaging, const int32 Seed) const override
	{
		return GetStagingWeightedRandomTpl(OutStaging, Entries, Seed);
	}

	virtual void BuildCache() override;
};
// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExAssetCollection.h"
#include "PCGExRandom.h"
#include "Engine/DataAsset.h"
#include "ISMPartition/ISMComponentDescriptor.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"

#include "PCGExMeshCollection.generated.h"

class UPCGExMeshCollection;

USTRUCT(BlueprintType, DisplayName="[PCGEx] Mesh Collection Entry")
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMeshCollectionEntry : public FPCGExAssetCollectionEntry
{
	GENERATED_BODY()

	FPCGExMeshCollectionEntry()
	{
	}

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	FSoftISMComponentDescriptor Descriptor;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bIsSubCollection", EditConditionHides))
	TSoftObjectPtr<UPCGExMeshCollection> SubCollection;

	TObjectPtr<UPCGExMeshCollection> SubCollectionPtr;

	bool Matches(const FPCGMeshInstanceList& InstanceList) const
	{
		// TODO : This is way too weak
		return InstanceList.Descriptor.StaticMesh == Descriptor.StaticMesh;
	}

	bool SameAs(const FPCGExMeshCollectionEntry& Other) const
	{
		return
			SubCollectionPtr == Other.SubCollectionPtr &&
			Weight == Other.Weight &&
			Category == Other.Category &&
			Descriptor.StaticMesh == Other.Descriptor.StaticMesh;
	}

	virtual bool Validate(const UPCGExAssetCollection* ParentCollection) override;

#if WITH_EDITOR
	virtual void UpdateStaging(const bool bRecursive) override;
#endif

protected:
	virtual void OnSubCollectionLoaded() override;
};

UCLASS(BlueprintType, DisplayName="[PCGEx] Mesh Collection")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExMeshCollection : public UPCGExAssetCollection
{
	GENERATED_BODY()

	friend struct FPCGExMeshCollectionEntry;
	friend class UPCGExMeshSelectorBase;

public:
#if WITH_EDITOR
	virtual bool IsCacheableProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void RefreshDisplayNames() override;
	virtual void RefreshStagingData() override;
	virtual void RefreshStagingData_Recursive() override;
#endif

	FORCEINLINE virtual bool GetStaging(FPCGExAssetStagingData& OutStaging, const int32 Index, const int32 Seed = -1) const override
	{
		return GetStagingTpl(OutStaging, Entries, Index, Seed);
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(TitleProperty="DisplayName"))
	TArray<FPCGExMeshCollectionEntry> Entries;
};

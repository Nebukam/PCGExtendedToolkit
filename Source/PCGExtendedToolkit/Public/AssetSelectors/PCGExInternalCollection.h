// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExAssetCollection.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Engine/DataAsset.h"

#include "PCGExInternalCollection.generated.h"

class UPCGExInternalCollection;

USTRUCT(NotBlueprintable, DisplayName="[PCGEx] Untyped Collection Entry")
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExInternalCollectionEntry : public FPCGExAssetCollectionEntry
{
	GENERATED_BODY()

	FPCGExInternalCollectionEntry()
	{
	}

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	FSoftObjectPath Object;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bIsSubCollection", EditConditionHides))
	TSoftObjectPtr<UPCGExInternalCollection> SubCollection;

	TObjectPtr<UPCGExInternalCollection> SubCollectionPtr;

	bool SameAs(const FPCGExInternalCollectionEntry& Other) const
	{
		return
			SubCollectionPtr == Other.SubCollectionPtr &&
			Weight == Other.Weight &&
			Category == Other.Category &&
			Object == Other.Object;
	}

	virtual bool Validate(const UPCGExAssetCollection* ParentCollection) override;
	virtual void UpdateStaging(const UPCGExAssetCollection* OwningCollection, const bool bRecursive) override;
	virtual void SetAssetPath(FSoftObjectPath InPath) override;

protected:
	virtual void OnSubCollectionLoaded() override;
};

UCLASS(NotBlueprintable, DisplayName="[PCGEx] Untyped Collection")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExInternalCollection : public UPCGExAssetCollection
{
	GENERATED_BODY()

	friend struct FPCGExInternalCollectionEntry;
	friend class UPCGExMeshSelectorBase;

public:
	virtual void RebuildStagingData(const bool bRecursive) override;

#if WITH_EDITOR
	virtual bool EDITOR_IsCacheableProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(TitleProperty="DisplayName"))
	TArray<FPCGExInternalCollectionEntry> Entries;
};

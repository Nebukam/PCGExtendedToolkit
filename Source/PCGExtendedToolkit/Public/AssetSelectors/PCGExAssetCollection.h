// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExRandom.h"
#include "Engine/DataAsset.h"
#include "ISMPartition/ISMComponentDescriptor.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"

#include "PCGExAssetCollection.generated.h"

class UPCGExAssetCollection;

USTRUCT(BlueprintType, DisplayName="[PCGEx] Asset Staging Data")
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAssetStagingData
{
	GENERATED_BODY()
	virtual ~FPCGExAssetStagingData() = default;

	FPCGExAssetStagingData()
	{
	}

	UPROPERTY()
	FVector Pivot = FVector::ZeroVector;

	UPROPERTY()
	FBox Bounds = FBox(ForceInitToZero);
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Asset Collection Entry")
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAssetCollectionEntry
{
	GENERATED_BODY()
	virtual ~FPCGExAssetCollectionEntry() = default;

	FPCGExAssetCollectionEntry()
	{
	}

	UPROPERTY(EditAnywhere, Category = Settings)
	bool bIsSubCollection = false;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides, ClampMin=0))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	FName Category = NAME_None;

	UPROPERTY()
	FPCGExAssetStagingData Staging;

	//UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bSubCollection", EditConditionHides))
	//TSoftObjectPtr<UPCGExDataCollection> SubCollection;

	TObjectPtr<UPCGExAssetCollection> BaseSubCollectionPtr;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category=Settings, meta=(HideInDetailPanel, EditCondition="false", EditConditionHides))
	FName DisplayName = NAME_None;
#endif

	virtual bool Validate(const UPCGExAssetCollection* ParentCollection);

#if WITH_EDITOR
	virtual void UpdateStaging();
#endif

protected:
	template <typename T>
	void LoadSubCollection(TSoftObjectPtr<T> SoftPtr)
	{
		BaseSubCollectionPtr = SoftPtr.LoadSynchronous();
		if (BaseSubCollectionPtr) { OnSubCollectionLoaded(); }
	}

	virtual void OnSubCollectionLoaded();
};

namespace PCGExAssetCollection
{
	struct /*PCGEXTENDEDTOOLKIT_API*/ FCategory
	{
		FName Name = NAME_None;
		double WeightSum = 0;
		TArray<int32> Indices;
		TArray<int32> Weights;
		TArray<int32> Order;

		FCategory()
		{
		}

		explicit FCategory(const FName InName):
			Name(InName)
		{
		}

		~FCategory();

		void BuildFromIndices();
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FCache
	{
		int32 WeightSum = 0;
		TMap<FName, FCategory*> Categories;
		TArray<int32> Indices;
		TArray<int32> Weights;
		TArray<int32> Order;

		explicit FCache()
		{
		}

		~FCache()
		{
			PCGEX_DELETE_TMAP(Categories, FName)

			Indices.Empty();
			Weights.Empty();
			Order.Empty();
		}

		void Reserve(int32 Num)
		{
			Indices.Reserve(Num);
			Weights.Reserve(Num);
			Order.Reserve(Num);
		}

		void Shrink()
		{
			Indices.Shrink();
			Weights.Shrink();
			Order.Shrink();
		}

		void FinalizeCache();
	};
}

UCLASS(Abstract, BlueprintType, DisplayName="[PCGEx] Asset Collection")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExAssetCollection : public UDataAsset
{
	GENERATED_BODY()

	friend struct FPCGExAssetCollectionEntry;
	friend class UPCGExMeshSelectorBase;

public:
	PCGExAssetCollection::FCache* LoadCache();

	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostEditImport() override;

#if WITH_EDITOR

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool IsCacheableProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void RefreshDisplayNames();

	// Declare a method callable from the editor
	UFUNCTION(CallInEditor, Category = "Editor")
	virtual void RefreshStagingData();

#endif

	virtual void BeginDestroy() override;

	/** If enabled, empty mesh will still be weighted and picked as valid entries, instead of being ignored. */
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bDoNotIgnoreInvalidEntries = false;

	virtual int32 GetValidEntryNum() { return Cache ? Cache->Indices.Num() : 0; }

	virtual void BuildCache();

	template <typename T>
	FORCEINLINE bool GetEntry(T& OutEntry, TArray<T>& InEntries, const int32 Index, const int32 Seed = -1)
	{
		if (!Cache->Indices.IsValidIndex(Index)) { return false; }
		const int32 Pick = Cache->Indices[Index];
		OutEntry = InEntries[Pick];
		if (OutEntry.SubCollectionPtr) { OutEntry.SubCollectionPtr->GetRandomEntryWeighted(OutEntry, OutEntry.SubCollectionPtr->Entries, Seed); }
		return true;
	}

	template <typename T>
	FORCEINLINE bool GetRandomEntry(T& OutEntry, TArray<T>& InEntries, const int32 Seed)
	{
		const int32 Pick = Cache->Indices[Cache->Order[FRandomStream(Seed).RandRange(0, Cache->Order.Num() - 1)]];
		OutEntry = InEntries[Pick];
		if (OutEntry.SubCollectionPtr) { OutEntry.SubCollectionPtr->GetRandomEntry(OutEntry, OutEntry.SubCollectionPtr->Entries, Seed + 1); }
		return true;
	}

	template <typename T>
	FORCEINLINE bool GetRandomEntryWeighted(T& OutEntry, TArray<T>& InEntries, const int32 Seed)
	{
		const double Threshold = FRandomStream(Seed).RandRange(0, Cache->WeightSum - 1);
		int32 Pick = 0;
		while (Pick < Cache->Weights.Num() && Cache->Weights[Pick] <= Threshold) { Pick++; }
		Pick = Cache->Indices[Cache->Order[Pick]];
		OutEntry = InEntries[Pick];
		if (OutEntry.SubCollectionPtr) { OutEntry.SubCollectionPtr->GetRandomEntryWeighted(OutEntry, OutEntry.SubCollectionPtr->Entries, Seed + 1); }
		return true;
	}

protected:
	bool bCacheNeedsRebuild = true;
	PCGExAssetCollection::FCache* Cache = nullptr;

	template <typename T>
	bool BuildCache(TArray<T>& InEntries)
	{
		check(Cache)

		bCacheNeedsRebuild = false;

		const int32 NumEntries = InEntries.Num();
		Cache->Reserve(NumEntries);

		TArray<PCGExAssetCollection::FCategory*> TempCategories;

		for (int i = 0; i < NumEntries; i++)
		{
			T& Entry = InEntries[i];
			if (!Entry.Validate(this)) { continue; }

			Cache->Indices.Add(i);
			Cache->Weights.Add(Entry.Weight);
			Cache->WeightSum += Entry.Weight;

			if (Entry.Category.IsNone()) { continue; }

			PCGExAssetCollection::FCategory** CategoryPtr = Cache->Categories.Find(Entry.Category);
			PCGExAssetCollection::FCategory* Category = nullptr;

			if (!CategoryPtr)
			{
				Category = new PCGExAssetCollection::FCategory(Entry.Category);
				Cache->Categories.Add(Entry.Category, Category);
				TempCategories.Add(Category);
			}
			else { Category = *CategoryPtr; }

			Category->Indices.Add(i);
			Category->Weights.Add(Entry.Weight);
			Category->WeightSum += Entry.Weight;
		}

		for (PCGExAssetCollection::FCategory* Category : TempCategories) { Category->BuildFromIndices(); }

		return true;
	}

#if WITH_EDITOR
	void SetDirty()
	{
		bCacheNeedsRebuild = true;
	}
#endif
};

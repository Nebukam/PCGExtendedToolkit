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

	//UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bSubCollection", EditConditionHides))
	//FSoftISMComponentDescriptor Descriptor;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides, ClampMin=0))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	FName Category = NAME_None;

	//UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bSubCollection", EditConditionHides))
	//TSoftObjectPtr<UPCGExDataCollection> SubCollection;

	TObjectPtr<UPCGExAssetCollection> BaseSubCollectionPtr;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient, VisibleAnywhere, Category = Settings, meta=(HideInDetailPanel, EditConditionHides, EditCondition="false"))
	FName DisplayName = NAME_None;
#endif

	template <typename T>
	void LoadSubCollection(TSoftObjectPtr<T> SoftPtr)
	{
		if (!SoftPtr.ToSoftObjectPath().IsValid()) { return; }
		BaseSubCollectionPtr = SoftPtr.LoadSynchronous();
		if (BaseSubCollectionPtr) { OnSubCollectionLoaded(); }
	}

	virtual bool IsValid();

protected:
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

	struct /*PCGEXTENDEDTOOLKIT_API*/ FAssetTuple
	{
		const UPCGExAssetCollection* Collection = nullptr;
		int32 Index = -1;

		FAssetTuple()
		{
		}

		explicit FAssetTuple(const UPCGExAssetCollection* InCollection, int32 InIndex):
			Collection(InCollection), Index(InIndex)
		{
		}

		explicit FAssetTuple(const FAssetTuple& Other):
			Collection(Other.Collection),
			Index(Other.Index)
		{
		}

		~FAssetTuple();
	};
}

UCLASS(Abstract, BlueprintType, DisplayName="[PCGEx] Asset Collection")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExAssetCollection : public UDataAsset
{
	GENERATED_BODY()

	friend struct FPCGExAssetCollectionEntry;
	friend class UPCGExMeshSelectorBase;

public:
	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostEditImport() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void RefreshDisplayNames();
#endif

	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(TitleProperty="DisplayName"))
	//TArray<FPCGExDataCollectionEntry> Entries;

	const TArray<int32>& GetValidEntries() const { return CachedIndices; }
	const TArray<int32>& GetWeights() const { return CachedWeights; }
	const TMap<FName, PCGExAssetCollection::FCategory*>& GetCategories() const { return CachedCategories; }

	virtual int32 GetValidEntryNum() { return CachedIndices.Num(); }

	virtual void BeginDestroy() override;

	template <typename T>
	bool RebuildCachedData(TArray<T>& InEntries)
	{
		if (!bCacheDirty) { return false; }
		bCacheDirty = false;

		ClearCategories();

		CachedIndices.Empty();
		CachedIndices.Reserve(InEntries.Num());

		CachedWeights.Empty();
		CachedWeights.Reserve(InEntries.Num());

		TArray<PCGExAssetCollection::FCategory*> TempCategories;

		WeightSum = 0;

		for (int i = 0; i < InEntries.Num(); i++)
		{
			T& Entry = InEntries[i];
			if (!Entry.IsValid()) { continue; }

			CachedIndices.Add(i);
			CachedWeights.Add(Entry.Weight);
			WeightSum += Entry.Weight;

			if (Entry.Category.IsNone()) { continue; }

			PCGExAssetCollection::FCategory** CategoryPtr = CachedCategories.Find(Entry.Category);
			PCGExAssetCollection::FCategory* Category = nullptr;

			if (!CategoryPtr)
			{
				Category = new PCGExAssetCollection::FCategory(Entry.Category);
				CachedCategories.Add(Entry.Category, Category);
				TempCategories.Add(Category);
			}
			else { Category = *CategoryPtr; }

			Category->Indices.Add(i);
			Category->Weights.Add(Entry.Weight);
			Category->WeightSum += Entry.Weight;
		}

		for (PCGExAssetCollection::FCategory* Cate : TempCategories) { Cate->BuildFromIndices(); }

		const int32 NumEntries = CachedIndices.Num();

		PCGEx::ArrayOfIndices(Order, NumEntries);
		Order.Sort([&](const int32 A, const int32 B) { return CachedWeights[A] < CachedWeights[B]; });
		CachedWeights.Sort([&](const int32 A, const int32 B) { return A < B; });

		for (int32 i = 0; i < NumEntries; i++)
		{
			CachedWeights[i] = i == 0 ? CachedWeights[i] : CachedWeights[i - 1] + CachedWeights[i];
		}


		return true;
	}

	template <typename T>
	FORCEINLINE bool GetEntry(T& OutEntry, TArray<T>& InEntries, const int32 Index, const int32 Seed = -1)
	{
		if (!CachedIndices.IsValidIndex(Index)) { return false; }
		const int32 Pick = CachedIndices[Index];
		OutEntry = InEntries[Pick];
		if (OutEntry.SubCollectionPtr) { OutEntry.SubCollectionPtr->GetRandomEntryWeighted(OutEntry, OutEntry.SubCollectionPtr->Entries, Seed); }
		return true;
	}

	template <typename T>
	FORCEINLINE bool GetRandomEntry(T& OutEntry, TArray<T>& InEntries, const int32 Seed)
	{
		const int32 Pick = CachedIndices[Order[FRandomStream(Seed).RandRange(0, Order.Num() - 1)]];
		OutEntry = InEntries[Pick];
		if (OutEntry.SubCollectionPtr) { OutEntry.SubCollectionPtr->GetRandomEntry(OutEntry, OutEntry.SubCollectionPtr->Entries, Seed + 1); }
		return true;
	}

	template <typename T>
	FORCEINLINE bool GetRandomEntryWeighted(T& OutEntry, TArray<T>& InEntries, const int32 Seed)
	{
		const double Threshold = FRandomStream(Seed).RandRange(0, WeightSum-1);
		int32 Pick = 0;
		while (Pick < CachedWeights.Num() && CachedWeights[Pick] <= Threshold) { Pick++; }
		Pick = CachedIndices[Order[Pick]];
		OutEntry = InEntries[Pick];
		if (OutEntry.SubCollectionPtr) { OutEntry.SubCollectionPtr->GetRandomEntryWeighted(OutEntry, OutEntry.SubCollectionPtr->Entries, Seed + 1); }
		return true;
	}

protected:
	bool bCacheDirty = true;

	TArray<int32> CachedIndices;
	TArray<int32> CachedWeights;
	TArray<int32> Order;
	int32 WeightSum = 0;
	TMap<FName, PCGExAssetCollection::FCategory*> CachedCategories;

	void ClearCategories();
};

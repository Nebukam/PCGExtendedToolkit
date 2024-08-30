// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExRandom.h"
#include "Engine/DataAsset.h"

#include "PCGExAssetCollection.generated.h"

class UPCGExAssetCollection;

UENUM(BlueprintType)
enum class EPCGExIndexPickMode : uint8
{
	Ascending UMETA(DisplayName = "Collection order (Ascending)", Tooltip="..."),
	Descending UMETA(DisplayName = "Collection order (Descending)", Tooltip="..."),
	WeightAscending UMETA(DisplayName = "Weight (Descending)", Tooltip="..."),
	WeightDescending UMETA(DisplayName = "Weight (Ascending)", Tooltip="..."),
};

UENUM(BlueprintType)
enum class EPCGExStagedPropertyType : uint8
{
	Double UMETA(DisplayName = "Double", Tooltip="...") ,
	Integer32 UMETA(DisplayName = "Integer 32", Tooltip="...") ,
	Vector UMETA(DisplayName = "Vector", Tooltip="...") ,
	Color UMETA(DisplayName = "Color", Tooltip="...") ,
	Boolean UMETA(DisplayName = "Boolean", Tooltip="...") ,
	Name UMETA(DisplayName = "Name", Tooltip="...")
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Distribution"))
enum class EPCGExDistribution : uint8
{
	Index UMETA(DisplayName = "Index", ToolTip="Distribution by index"),
	Random UMETA(DisplayName = "Random", ToolTip="Update the point scale so final asset matches the existing point' bounds"),
	WeightedRandom UMETA(DisplayName = "Weighted random", ToolTip="Update the point bounds so it reflects the bounds of the final asset"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Distribution"))
enum class EPCGExWeightOutputMode : uint8
{
	NoOutput UMETA(DisplayName = "No Output", ToolTip="Don't output weight as an attribute"),
	Raw UMETA(DisplayName = "Raw", ToolTip="Raw integer"),
	Normalized UMETA(DisplayName = "Normalized", ToolTip="Normalized weight value (Weight / WeightSum)"),
	NormalizedInverted UMETA(DisplayName = "Normalized (Inverted)", ToolTip="One Minus normalized weight value (1 - (Weight / WeightSum))"),

	NormalizedToDensity UMETA(DisplayName = "Normalized to Density", ToolTip="Normalized weight value (Weight / WeightSum)"),
	NormalizedInvertedToDensity UMETA(DisplayName = "Normalized (Inverted) to Density", ToolTip="One Minus normalized weight value (1 - (Weight / WeightSum))"),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAssetDistributionIndexDetails
{

	GENERATED_BODY()
	
	FPCGExAssetDistributionIndexDetails()
	{
		if (IndexSource.GetName() == FName("@Last")) { IndexSource.Update(TEXT("$Index")); }
	}

	/** Index picking mode*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Distribution==EPCGExDistribution::Index", EditConditionHides))
	EPCGExIndexPickMode PickMode = EPCGExIndexPickMode::Ascending;

	/** Index sanitization behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Distribution==EPCGExDistribution::Index", EditConditionHides))
	EPCGExIndexSafety IndexSafety = EPCGExIndexSafety::Tile;

	/** The name of the attribute index to read index selection from.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Distribution==EPCGExDistribution::Index", EditConditionHides))
	FPCGAttributePropertyInputSelector IndexSource;

	/** Whether to remap index input value to collection size */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Distribution==EPCGExDistribution::Index", EditConditionHides))
	bool bRemapIndexToCollectionSize = false;

	/** Whether to remap index input value to collection size */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Distribution==EPCGExDistribution::Index", EditConditionHides))
	EPCGExTruncateMode TruncateRemap = EPCGExTruncateMode::None;
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAssetDistributionDetails
{
	GENERATED_BODY()

	FPCGExAssetDistributionDetails()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Distribution", meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExSeedComponents"))
	uint8 SeedComponents = 0;

	/** Distribution type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Distribution", meta=(PCG_Overridable))
	EPCGExDistribution Distribution = EPCGExDistribution::WeightedRandom;

	/** Index settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Distribution", meta=(PCG_Overridable, EditCondition="Distribution == EPCGExDistribution::Index"))
	FPCGExAssetDistributionIndexDetails IndexSettings;

	/** Note that this is only accounted for if selected in the seed component. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Distribution", meta=(PCG_Overridable))
	int32 LocalSeed = 0;
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Asset Staged Property")
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExStagedProperty
{
	GENERATED_BODY()
	virtual ~FPCGExStagedProperty() = default;

	FPCGExStagedProperty()
	{
	}

	UPROPERTY(EditAnywhere, Category = Settings)
	FName Name = NAME_None;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	bool bIsChildProperty = false;
#endif

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsChildProperty", EditConditionHides))
	EPCGExStagedPropertyType Type = EPCGExStagedPropertyType::Double;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName="Value", EditCondition="Type==EPCGExStagedPropertyType::Double", EditConditionHides))
	double MDouble = 0.0;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName="Value", EditCondition="Type==EPCGExStagedPropertyType::Integer32", EditConditionHides))
	int32 MInt32 = 0.0;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName="Value", EditCondition="Type==EPCGExStagedPropertyType::Vector", EditConditionHides))
	FVector MVector = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName="Value", EditCondition="Type==EPCGExStagedPropertyType::Color", EditConditionHides))
	FLinearColor MColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName="Value", EditCondition="Type==EPCGExStagedPropertyType::Boolean", EditConditionHides))
	bool MBoolean = true;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName="Value", EditCondition="Type==EPCGExStagedPropertyType::Name", EditConditionHides))
	FName MName = NAME_None;
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Asset Staging Data")
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExAssetStagingData
{
	GENERATED_BODY()
	virtual ~FPCGExAssetStagingData() = default;

	FPCGExAssetStagingData()
	{
	}

	UPROPERTY()
	FSoftObjectPath Path;

	UPROPERTY()
	int32 Weight = 1; // Dupe from parent.

	UPROPERTY()
	FName Category = NAME_None; // Dupe from parent.

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditFixedSize))
	TArray<FPCGExStagedProperty> CustomProperties;

	UPROPERTY(VisibleAnywhere, Category = Baked)
	FVector Pivot = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, Category = Baked)
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

	UPROPERTY(EditAnywhere, Category = Settings)
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
	virtual void UpdateStaging(const UPCGExAssetCollection* OwningCollection, const bool bRecursive);
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


		FORCEINLINE int32 GetPick(const int32 Index, const EPCGExIndexPickMode PickMode) const
		{
			switch (PickMode)
			{
			default:
			case EPCGExIndexPickMode::Ascending:
				return GetPickAscending(Index);
			case EPCGExIndexPickMode::Descending:
				return GetPickDescending(Index);
			case EPCGExIndexPickMode::WeightAscending:
				return GetPickWeightAscending(Index);
			case EPCGExIndexPickMode::WeightDescending:
				return GetPickWeightDescending(Index);
			}
		}

		FORCEINLINE int32 GetPickAscending(const int32 Index) const
		{
			return Indices.IsValidIndex(Index) ? Indices[(Indices.Num() - 1) - Index] : -1;
		}

		FORCEINLINE int32 GetPickDescending(const int32 Index) const
		{
			return Indices.IsValidIndex(Index) ? Indices[Index] : -1;
		}

		FORCEINLINE int32 GetPickWeightAscending(const int32 Index) const
		{
			return Order.IsValidIndex(Index) ? Indices[Order[Index]] : -1;
		}

		FORCEINLINE int32 GetPickWeightDescending(const int32 Index) const
		{
			return Order.IsValidIndex(Index) ? Indices[Order[(Order.Num() - 1) - Index]] : -1;
		}

		FORCEINLINE int32 GetPickRandom(const int32 Seed) const
		{
			return Indices[Order[FRandomStream(Seed).RandRange(0, Order.Num() - 1)]];
		}

		FORCEINLINE int32 GetPickRandomWeighted(const int32 Seed) const
		{
			const double Threshold = FRandomStream(Seed).RandRange(0, WeightSum - 1);
			int32 Pick = 0;
			while (Pick < Weights.Num() && Weights[Pick] <= Threshold) { Pick++; }
			return Indices[Order[Pick]];
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

	bool bCollectGarbage = true;

	UFUNCTION(CallInEditor, Category = Tools, meta=(DisplayName="Refresh Staging", ShortToolTip="Refresh Staging data just for this collection."))
	virtual void RefreshStagingData();

	UFUNCTION(CallInEditor, Category = Tools, meta=(DisplayName="Refresh Staging (Recursive)", ShortToolTip="Refresh Staging data for this collection and its sub-collections, recursively."))
	virtual void RefreshStagingData_Recursive();

	UFUNCTION(CallInEditor, Category = Tools, meta=(DisplayName="Refresh Staging (Project)", ShortToolTip="Refresh Staging data for all collection within this project."))
	virtual void RefreshStagingData_Project();

#endif

	virtual void BeginDestroy() override;

	/** If enabled, empty mesh will still be weighted and picked as valid entries, instead of being ignored. */
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bDoNotIgnoreInvalidEntries = false;

	virtual int32 GetValidEntryNum() { return Cache ? Cache->Indices.Num() : 0; }

	virtual void BuildCache();

#pragma region GetEntry

	template <typename T>
	FORCEINLINE bool GetEntry(T& OutEntry, TArray<T>& InEntries, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode = EPCGExIndexPickMode::Ascending) const
	{
		int32 Pick = Cache->GetPick(Index, PickMode);
		if (!InEntries.IsValidIndex(Pick)) { return false; }
		OutEntry = InEntries[Pick];
		if (OutEntry.SubCollectionPtr) { OutEntry.SubCollectionPtr->GetEntryWeightedRandom(OutEntry, OutEntry.SubCollectionPtr->Entries, Seed); }
		return true;
	}

	template <typename T>
	FORCEINLINE bool GetEntryRandom(T& OutEntry, TArray<T>& InEntries, const int32 Seed) const
	{
		OutEntry = InEntries[Cache->GetPickRandom(Seed)];
		if (OutEntry.SubCollectionPtr) { OutEntry.SubCollectionPtr->GetEntryRandom(OutEntry, OutEntry.SubCollectionPtr->Entries, Seed + 1); }
		return true;
	}

	template <typename T>
	FORCEINLINE bool GetEntryWeightedRandom(T& OutEntry, TArray<T>& InEntries, const int32 Seed) const
	{
		OutEntry = InEntries[Cache->GetPickRandomWeighted(Seed)];
		if (OutEntry.SubCollectionPtr) { OutEntry.SubCollectionPtr->GetEntryWeightedRandom(OutEntry, OutEntry.SubCollectionPtr->Entries, Seed + 1); }
		return true;
	}

#pragma endregion


	FORCEINLINE virtual bool GetStaging(FPCGExAssetStagingData& OutStaging, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode = EPCGExIndexPickMode::Ascending) const
	{
		return false;
	}

	FORCEINLINE virtual bool GetStagingRandom(FPCGExAssetStagingData& OutStaging, const int32 Seed) const
	{
		return false;
	}

	FORCEINLINE virtual bool GetStagingWeightedRandom(FPCGExAssetStagingData& OutStaging, const int32 Seed) const
	{
		return false;
	}

protected:
#pragma region GetStaging
	template <typename T>
	FORCEINLINE bool GetStagingTpl(FPCGExAssetStagingData& OutStaging, const TArray<T>& InEntries, const int32 Index, const int32 Seed, const EPCGExIndexPickMode PickMode = EPCGExIndexPickMode::Ascending) const
	{
		const int32 Pick = Cache->GetPick(Index, PickMode);
		if (!InEntries.IsValidIndex(Pick)) { return false; }
		if (const T& Entry = InEntries[Pick]; Entry.SubCollectionPtr) { Entry.SubCollectionPtr->GetStagingWeightedRandomTpl(OutStaging, Entry.SubCollectionPtr->Entries, Seed); }
		else { OutStaging = Entry.Staging; }
		return true;
	}

	template <typename T>
	FORCEINLINE bool GetStagingRandomTpl(FPCGExAssetStagingData& OutStaging, const TArray<T>& InEntries, const int32 Seed) const
	{
		const T& Entry = InEntries[Cache->GetPickRandom(Seed)];
		if (Entry.SubCollectionPtr) { Entry.SubCollectionPtr->GetStagingRandomTpl(OutStaging, Entry.SubCollectionPtr->Entries, Seed + 1); }
		else { OutStaging = Entry.Staging; }
		return true;
	}

	template <typename T>
	FORCEINLINE bool GetStagingWeightedRandomTpl(FPCGExAssetStagingData& OutStaging, const TArray<T>& InEntries, const int32 Seed) const
	{
		const T& Entry = InEntries[Cache->GetPickRandomWeighted(Seed)];
		if (Entry.SubCollectionPtr) { Entry.SubCollectionPtr->GetStagingWeightedRandomTpl(OutStaging, Entry.SubCollectionPtr->Entries, Seed + 1); }
		else { OutStaging = Entry.Staging; }
		return true;
	}

#pragma endregion

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
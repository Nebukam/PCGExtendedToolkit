// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Core/PCGExAssetCollection.h"
#include "Helpers/PCGExArrayHelpers.h"
#include "PCGExPCGDataAssetCollection.generated.h"

class UPCGDataAsset;
class UPCGExPCGDataAssetCollection;


namespace PCGExPCGDataAssetCollection
{
	// PCGDataAsset MicroCache - Handles point-level picking within data assets
	class PCGEXCOLLECTIONS_API FMicroCache : public PCGExAssetCollection::FMicroCache
	{
	public:
		FMicroCache() = default;

		virtual PCGExAssetCollection::FTypeId GetTypeId() const override
		{
			return PCGExAssetCollection::TypeIds::PCGDataAsset;
		}

		void ProcessPointWeights(const TArray<int32>& InPointWeights);
	};
}

// PCGDataAsset Collection Entry

USTRUCT(BlueprintType, DisplayName="[PCGEx] PCGDataAsset Collection Entry")
struct PCGEXCOLLECTIONS_API FPCGExPCGDataAssetCollectionEntry : public FPCGExAssetCollectionEntry
{
	GENERATED_BODY()

	FPCGExPCGDataAssetCollectionEntry() = default;

	// Type System

	virtual PCGExAssetCollection::FTypeId GetTypeId() const override
	{
		return PCGExAssetCollection::TypeIds::PCGDataAsset;
	}

	// PCGDataAsset-Specific Properties 

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	TSoftObjectPtr<UPCGDataAsset> DataAsset = nullptr;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bIsSubCollection", EditConditionHides, DisplayAfter="bIsSubCollection"))
	TObjectPtr<UPCGExPCGDataAssetCollection> SubCollection;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	bool bOverrideWeights = false;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" └─ Weights", EditCondition="!bIsSubCollection && bOverrideWeights", EditConditionHides))
	TArray<int32> PointWeights;

	// Subcollection Access

	virtual UPCGExAssetCollection* GetSubCollectionPtr() const override;

	virtual void ClearSubCollection() override;

	// Lifecycle

	virtual bool Validate(const UPCGExAssetCollection* ParentCollection) override;
	virtual void UpdateStaging(const UPCGExAssetCollection* OwningCollection, int32 InInternalIndex, bool bRecursive) override;
	virtual void SetAssetPath(const FSoftObjectPath& InPath) override;

#if WITH_EDITOR
	virtual void EDITOR_Sanitize() override;
#endif

	virtual void BuildMicroCache() override;

	// Typed MicroCache Access

	PCGExPCGDataAssetCollection::FMicroCache* GetDataAssetMicroCache() const
	{
		return static_cast<PCGExPCGDataAssetCollection::FMicroCache*>(MicroCache.Get());
	}

#pragma region DEPRECATED
	UPROPERTY()
	int32 PointWeightsCumulativeWeight_DEPRECATED = -1;

	UPROPERTY()
	TArray<int32> PointWeightsOrder_DEPRECATED;

	UPROPERTY()
	TArray<int32> ProcessedPointWeights_DEPRECATED;
#pragma endregion
};

// PCGDataAsset Collection

UCLASS(BlueprintType, DisplayName="[PCGEx] PCGDataAsset Collection")
class PCGEXCOLLECTIONS_API UPCGExPCGDataAssetCollection : public UPCGExAssetCollection
{
	GENERATED_BODY()
	PCGEX_ASSET_COLLECTION_BODY(FPCGExPCGDataAssetCollectionEntry)

public:
	friend struct FPCGExPCGDataAssetCollectionEntry;

	// Type System

	virtual PCGExAssetCollection::FTypeId GetTypeId() const override
	{
		return PCGExAssetCollection::TypeIds::PCGDataAsset;
	}

	// Entries Array

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TArray<FPCGExPCGDataAssetCollectionEntry> Entries;

	// Editor Functions

#if WITH_EDITOR
	virtual void EDITOR_AddBrowserSelectionInternal(const TArray<FAssetData>& InAssetData) override;
#endif
};

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#endif

#include "PCGDataAsset.h"
#include "PCGExAssetCollection.h"
#include "Engine/DataAsset.h"
#include "Engine/StaticMesh.h"
#include "UObject/SoftObjectPath.h"

#include "PCGExPCGDataAssetCollection.generated.h"

class UPCGExPCGDataAssetCollection;

namespace PCGExPCGDataAssetCollection
{
	class PCGEXTENDEDTOOLKIT_API FMicroCache : public PCGExAssetCollection::FMicroCache
	{
		double WeightSum = 0;
		TArray<int32> Weights;
		TArray<int32> Order;

		int32 HighestIndex = -1;

	public:
		FMicroCache() = default;

		virtual PCGExAssetCollection::EType GetType() const override { return PCGExAssetCollection::EType::PCGDataAsset; }

		virtual int32 Num() const override { return Order.Num(); }
		int32 GetHighestIndex() const { return HighestIndex; }

		virtual int32 GetPick(const int32 Index, const EPCGExIndexPickMode PickMode) const override;

		virtual int32 GetPickAscending(const int32 Index) const override;
		virtual int32 GetPickDescending(const int32 Index) const override;
		virtual int32 GetPickWeightAscending(const int32 Index) const override;
		virtual int32 GetPickWeightDescending(const int32 Index) const override;
		virtual int32 GetPickRandom(const int32 Seed) const override;
		virtual int32 GetPickRandomWeighted(const int32 Seed) const override;
	};
}

USTRUCT(BlueprintType, DisplayName="[PCGEx] PCGDataAsset Collection Entry")
struct PCGEXTENDEDTOOLKIT_API FPCGExPCGDataAssetCollectionEntry : public FPCGExAssetCollectionEntry
{
	GENERATED_BODY()

	FPCGExPCGDataAssetCollectionEntry() = default;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	TSoftObjectPtr<UPCGDataAsset> DataAsset = nullptr;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bIsSubCollection", EditConditionHides, DisplayAfter="bIsSubCollection"))
	TObjectPtr<UPCGExPCGDataAssetCollection> SubCollection;

	virtual void ClearSubCollection() override;

	virtual void GetAssetPaths(TSet<FSoftObjectPath>& OutPaths) const override;

	virtual bool Validate(const UPCGExAssetCollection* ParentCollection) override;

	virtual UPCGExAssetCollection* GetSubCollectionVoid() const override;

	virtual void UpdateStaging(const UPCGExAssetCollection* OwningCollection, int32 InInternalIndex, const bool bRecursive) override;
	virtual void SetAssetPath(const FSoftObjectPath& InPath) override;

#if WITH_EDITOR
	virtual void EDITOR_Sanitize() override;
#endif

	virtual void BuildMicroCache() override;
};

UCLASS(Hidden, BlueprintType, DisplayName="[PCGEx] PCGDataAsset Collection")
class PCGEXTENDEDTOOLKIT_API UPCGExPCGDataAssetCollection : public UPCGExAssetCollection
{
	GENERATED_BODY()

	friend struct FPCGExPCGDataAssetCollectionEntry;

public:
	virtual PCGExAssetCollection::EType GetType() const override { return PCGExAssetCollection::EType::PCGDataAsset; }

#if WITH_EDITOR
	virtual void EDITOR_AddBrowserSelectionInternal(const TArray<FAssetData>& InAssetData) override;
#endif

	PCGEX_ASSET_COLLECTION_BOILERPLATE(UPCGExPCGDataAssetCollection, FPCGExPCGDataAssetCollectionEntry)

	virtual void EDITOR_RegisterTrackingKeys(FPCGExContext* Context) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TArray<FPCGExPCGDataAssetCollectionEntry> Entries;
};

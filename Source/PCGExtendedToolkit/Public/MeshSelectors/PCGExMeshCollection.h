// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExRandom.h"
#include "Engine/DataAsset.h"
#include "ISMPartition/ISMComponentDescriptor.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"

#include "PCGExMeshCollection.generated.h"

class UPCGExMeshCollection;

USTRUCT(BlueprintType, DisplayName="[PCGEx] Mesh Collection Entry")
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMeshCollectionEntry
{
	GENERATED_BODY()

	FPCGExMeshCollectionEntry()
	{
	}

	UPROPERTY(EditAnywhere, Category = Settings)
	bool bSubCollection = false;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bSubCollection", EditConditionHides))
	FSoftISMComponentDescriptor Descriptor;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bSubCollection", EditConditionHides, ClampMin=0))
	double Weight = 1;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bSubCollection", EditConditionHides))
	FName Category = NAME_None;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bSubCollection", EditConditionHides))
	TSoftObjectPtr<UPCGExMeshCollection> SubCollection;

	TObjectPtr<UPCGExMeshCollection> SubCollectionPtr;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient, VisibleAnywhere, Category = Settings, meta=(HideInDetailPanel, EditConditionHides, EditCondition="false"))
	FName DisplayName = NAME_None;
#endif

	void LoadSubCollection();

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
};

namespace PCGExMeshCollection
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
}

UCLASS(BlueprintType, DisplayName="[PCGEx] Mesh Collection")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExMeshCollection : public UDataAsset
{
	GENERATED_BODY()

	friend struct FPCGExMeshCollectionEntry;
	friend class UPCGExMeshSelectorBase;

public:
	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void PostEditImport() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	void RefreshDisplayNames();
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(TitleProperty="DisplayName"))
	TArray<FPCGExMeshCollectionEntry> Entries;

	const TArray<int32>& GetValidEntries() const { return CachedIndices; }
	const TArray<double>& GetWeights() const { return CachedWeights; }
	const TMap<FName, PCGExMeshCollection::FCategory*>& GetCategories() const { return CachedCategories; }

	virtual void BeginDestroy() override;

	FORCEINLINE int32 GetWeightedIndexFromPoint(
		const FPCGPoint& Point,
		const int32 Offset,
		const UPCGSettings* Settings = nullptr,
		const UPCGComponent* Component = nullptr)
	{
		const double Threshold = PCGExRandom::GetRandomStreamFromPoint(Point, Offset, Settings, Component).RandRange(0, 1);
		int32 Pick = 0;
		while (Pick < CachedWeights.Num() && CachedWeights[Order[Pick]] <= Threshold) { ++Pick; }
		return Order[Pick];
	}

protected:
	bool bCacheDirty = true;

	TArray<int32> CachedIndices;
	TArray<double> CachedWeights;
	TArray<int32> Order;
	TMap<FName, PCGExMeshCollection::FCategory*> CachedCategories;

	void ClearCategories();
	virtual bool RebuildCachedData();
};

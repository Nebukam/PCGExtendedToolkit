// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ISMPartition/ISMComponentDescriptor.h"

#include "PCGExMeshCollection.generated.h"

class UPCGExMeshCollection;

USTRUCT(BlueprintType, DisplayName="[PCGEx] Mesh Collection Entry")
struct PCGEXTENDEDTOOLKIT_API FPCGExMeshCollectionEntry
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
};

namespace PCGExMeshCollection
{
	struct PCGEXTENDEDTOOLKIT_API FPCGExMeshCollectionCategory
	{
		FName Name = NAME_None;
		double WeightSum = 0;
		TArray<int32> Indices;
		TArray<int32> Weights;
		TArray<int32> Order;

		FPCGExMeshCollectionCategory()
		{
		}

		explicit FPCGExMeshCollectionCategory(const FName InName):
			Name(InName)
		{
		}

		~FPCGExMeshCollectionCategory();

		void BuildFromIndices();
	};
}

UCLASS(BlueprintType, DisplayName="[PCGEx] Mesh Collection")
class PCGEXTENDEDTOOLKIT_API UPCGExMeshCollection : public UDataAsset
{
	GENERATED_BODY()

	friend struct FPCGExMeshCollectionEntry;

public:
#if WITH_EDITOR
	// Override PostEditChangeProperty to react to property changes
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(TitleProperty="DisplayName"))
	TArray<FPCGExMeshCollectionEntry> Entries;

	const TArray<int32>& GetValidEntries() const { return CachedIndices; }
	const TArray<double>& GetWeights() const { return CachedWeights; }
	const TMap<FName, PCGExMeshCollection::FPCGExMeshCollectionCategory*>& GetCategories() const { return CachedCategories; }

	virtual void BeginDestroy() override;

protected:
	bool bCacheDirty = true;

	TArray<int32> CachedIndices;
	TArray<double> CachedWeights;
	TArray<int32> Order;
	TMap<FName, PCGExMeshCollection::FPCGExMeshCollectionCategory*> CachedCategories;

	void ClearCategories();
	virtual bool RebuildCachedData();
};

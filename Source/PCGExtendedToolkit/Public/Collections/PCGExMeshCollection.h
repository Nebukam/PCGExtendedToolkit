// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExComponentDescriptors.h"
#include "PCGExAssetCollection.h"
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
	TSoftObjectPtr<UStaticMesh> StaticMesh = nullptr;

	/** Config used when this entry is consumed as an instanced static mesh */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	FSoftISMComponentDescriptor ISMDescriptor;

	/** Config used when this entry is consumed as a regular static mesh primitive (i.e Spline Mesh)*/
	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	FPCGExStaticMeshComponentDescriptor SMDescriptor;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bIsSubCollection", EditConditionHides))
	TSoftObjectPtr<UPCGExMeshCollection> SubCollection;

	TObjectPtr<UPCGExMeshCollection> SubCollectionPtr;
	
	bool Matches(const FPCGMeshInstanceList& InstanceList) const
	{
		// TODO : This is way too weak
		return InstanceList.Descriptor.StaticMesh == ISMDescriptor.StaticMesh;
	}

	bool SameAs(const FPCGExMeshCollectionEntry& Other) const
	{
		return
			SubCollectionPtr == Other.SubCollectionPtr &&
			Weight == Other.Weight &&
			Category == Other.Category &&
			StaticMesh == Other.StaticMesh;
	}

	virtual bool Validate(const UPCGExAssetCollection* ParentCollection) override;
	virtual void UpdateStaging(const UPCGExAssetCollection* OwningCollection, int32 InInternalIndex, const bool bRecursive) override;
	virtual void SetAssetPath(const FSoftObjectPath& InPath) override;

#if PCGEX_ENGINE_VERSION > 504
	void InitPCGSoftISMDescriptor(FPCGSoftISMComponentDescriptor& TargetDescriptor) const;
#endif
	
#if WITH_EDITOR
	virtual void EDITOR_Sanitize() override;
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
	virtual void EDITOR_RefreshDisplayNames() override;

	UFUNCTION(CallInEditor, Category = Utils, meta=(DisplayName="Disable Collisions", ShortToolTip="Disabbe collision on all entries.", DisplayOrder=100))
	void EDITOR_DisableCollisions();
	
#endif

	PCGEX_ASSET_COLLECTION_BOILERPLATE(UPCGExMeshCollection, FPCGExMeshCollectionEntry)

	virtual void GetAssetPaths(TSet<FSoftObjectPath>& OutPaths, const PCGExAssetCollection::ELoadingFlags Flags) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(TitleProperty="DisplayName"))
	TArray<FPCGExMeshCollectionEntry> Entries;
};

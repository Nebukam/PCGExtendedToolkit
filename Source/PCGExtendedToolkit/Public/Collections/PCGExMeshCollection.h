// Copyright 2024 Timothé Lapetite and contributors
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

UENUM()
enum class EPCGExMaterialVariantsMode : uint8
{
	None   = 0 UMETA(DisplayName = "None", ToolTip="No variants."),
	Single = 1 UMETA(DisplayName = "Single Slot", ToolTip="Single-slot variants, for when there is only a single material slot override."),
	Multi  = 2 UMETA(DisplayName = "Multi Slots", ToolTip="Multi-slot variants, more admin, for when there is multiple material slots for the entry."),
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Material Override Entry")
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMaterialOverrideEntry
{
	GENERATED_BODY()

	FPCGExMaterialOverrideEntry()
	{
	}

	/** Material slot index. -1 uses the index inside the container. */
	UPROPERTY(EditAnywhere, Category = Settings)
	int32 SlotIndex = -1;

	UPROPERTY(EditAnywhere, Category = Settings)
	TSoftObjectPtr<UMaterialInterface> Material = nullptr;
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Material Override Collection")
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMaterialOverrideCollection
{
	GENERATED_BODY()

	virtual ~FPCGExMaterialOverrideCollection() = default;

	FPCGExMaterialOverrideCollection()
	{
	}

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition = "bEnabled", ClampMin=1))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition = "bEnabled"))
	TArray<FPCGExMaterialOverrideEntry> Overrides;

	virtual void GetAssetPaths(TSet<FSoftObjectPath>& OutPaths) const;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category=Settings, meta=(HideInDetailPanel, EditCondition="false", EditConditionHides))
	FName DisplayName = NAME_None;

	void UpdateDisplayName()
	{
	}

#endif
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Material Override Single Entry")
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMaterialOverrideSingleEntry
{
	GENERATED_BODY()

	FPCGExMaterialOverrideSingleEntry()
	{
	}

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition = "bEnabled", ClampMin=1))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition = "bEnabled"))
	TSoftObjectPtr<UMaterialInterface> Material = nullptr;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category=Settings, meta=(HideInDetailPanel, EditCondition="false", EditConditionHides))
	FName DisplayName = NAME_None;

	void UpdateDisplayName()
	{
		DisplayName = FName(Material.GetAssetName());
	}
#endif
};

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

	/** A list of material variants */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	EPCGExMaterialVariantsMode MaterialVariants = EPCGExMaterialVariantsMode::None;

	/** Material slot index. -1 uses the index inside the container. */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" └─ Slot Index", EditCondition="!bIsSubCollection  && MaterialVariants==EPCGExMaterialVariantsMode::Single", EditConditionHides))
	int32 SlotIndex = 0;

	/** A list of single material variants */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" └─ Variants", EditCondition="!bIsSubCollection && MaterialVariants==EPCGExMaterialVariantsMode::Single", TitleProperty="DisplayName", EditConditionHides))
	TArray<FPCGExMaterialOverrideSingleEntry> MaterialOverrideVariants;

	/** A list of material variants */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" └─ Variants", EditCondition="!bIsSubCollection && MaterialVariants==EPCGExMaterialVariantsMode::Multi", TitleProperty="DisplayName", EditConditionHides))
	TArray<FPCGExMaterialOverrideCollection> MaterialOverrideVariantsList;

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

	virtual void GetAssetPaths(TSet<FSoftObjectPath>& OutPaths) const override;

	virtual bool Validate(const UPCGExAssetCollection* ParentCollection) override;

	UPROPERTY()
	int32 MaterialVariantsCumulativeWeight = -1;

	UPROPERTY()
	TArray<int32> MaterialVariantsOrder;

	UPROPERTY()
	TArray<int32> MaterialVariantsWeights;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(TitleProperty="DisplayName"))
	TArray<FPCGExMeshCollectionEntry> Entries;
};

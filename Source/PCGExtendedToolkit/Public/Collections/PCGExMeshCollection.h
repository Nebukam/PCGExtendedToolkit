// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetData.h"
#endif

#include "PCGExComponentDescriptors.h"
#include "PCGExAssetCollection.h"
#include "Engine/DataAsset.h"
#include "ISMPartition/ISMComponentDescriptor.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"
#include "Engine/StaticMesh.h"
#include "UObject/SoftObjectPath.h"

#include "PCGExMeshCollection.generated.h"

class UPCGExMeshCollection;

UENUM()
enum class EPCGExMaterialVariantsMode : uint8
{
	None   = 0 UMETA(DisplayName = "None", ToolTip="No variants.", ActionIcon="STF_None"),
	Single = 1 UMETA(DisplayName = "Single Slot", ToolTip="Single-slot variants, for when there is only a single material slot override.", ActionIcon="SingleMat"),
	Multi  = 2 UMETA(DisplayName = "Multi Slots", ToolTip="Multi-slot variants, more admin, for when there is multiple material slots for the entry.", ActionIcon="MultiMat"),
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Material Override Entry")
struct PCGEXTENDEDTOOLKIT_API FPCGExMaterialOverrideEntry
{
	GENERATED_BODY()

	FPCGExMaterialOverrideEntry() = default;

	/** Material slot index. -1 uses the index inside the container. */
	UPROPERTY(EditAnywhere, Category = Settings)
	int32 SlotIndex = -1;

	UPROPERTY(EditAnywhere, Category = Settings)
	TSoftObjectPtr<UMaterialInterface> Material = nullptr;
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Material Override Collection")
struct PCGEXTENDEDTOOLKIT_API FPCGExMaterialOverrideCollection
{
	GENERATED_BODY()

	virtual ~FPCGExMaterialOverrideCollection() = default;

	FPCGExMaterialOverrideCollection() = default;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(ClampMin=1))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, Category = Settings)
	TArray<FPCGExMaterialOverrideEntry> Overrides;

	virtual void GetAssetPaths(TSet<FSoftObjectPath>& OutPaths) const;
	int32 GetHighestIndex() const;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category=Settings, meta=(HideInDetailPanel, EditCondition="false", EditConditionHides))
	FName DisplayName = NAME_None;
#endif

#if WITH_EDITOR
	void UpdateDisplayName();
#endif
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Material Override Single Entry")
struct PCGEXTENDEDTOOLKIT_API FPCGExMaterialOverrideSingleEntry
{
	GENERATED_BODY()

	FPCGExMaterialOverrideSingleEntry() = default;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(ClampMin=1))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, Category = Settings)
	TSoftObjectPtr<UMaterialInterface> Material = nullptr;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category=Settings, meta=(HideInDetailPanel, EditCondition="false", EditConditionHides))
	FName DisplayName = NAME_None;
#endif
#if WITH_EDITOR
	void UpdateDisplayName();
#endif
};

namespace PCGExMeshCollection
{
	class PCGEXTENDEDTOOLKIT_API FMicroCache : public PCGExAssetCollection::FMicroCache
	{
		double WeightSum = 0;
		TArray<int32> Weights;
		TArray<int32> Order;

		int32 HighestIndex = -1;

	public:
		FMicroCache()
		{
		}

		virtual PCGExAssetCollection::EType GetType() const override { return PCGExAssetCollection::EType::Mesh; }

		virtual int32 Num() const override { return Order.Num(); }
		int32 GetHighestIndex() const { return HighestIndex; }

		void ProcessMaterialOverrides(const TArray<FPCGExMaterialOverrideSingleEntry>& Overrides, const int32 InSlotIndex = -1);
		void ProcessMaterialOverrides(const TArray<FPCGExMaterialOverrideCollection>& Overrides);

		virtual int32 GetPick(const int32 Index, const EPCGExIndexPickMode PickMode) const override;

		virtual int32 GetPickAscending(const int32 Index) const override;
		virtual int32 GetPickDescending(const int32 Index) const override;
		virtual int32 GetPickWeightAscending(const int32 Index) const override;
		virtual int32 GetPickWeightDescending(const int32 Index) const override;
		virtual int32 GetPickRandom(const int32 Seed) const override;
		virtual int32 GetPickRandomWeighted(const int32 Seed) const override;
	};
}

USTRUCT(BlueprintType, DisplayName="[PCGEx] Mesh Collection Entry")
struct PCGEXTENDEDTOOLKIT_API FPCGExMeshCollectionEntry : public FPCGExAssetCollectionEntry
{
	GENERATED_BODY()

	FPCGExMeshCollectionEntry()
	{
	}

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	TSoftObjectPtr<UStaticMesh> StaticMesh = nullptr;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bIsSubCollection", EditConditionHides, DisplayAfter="bIsSubCollection"))
	TObjectPtr<UPCGExMeshCollection> SubCollection;
	
	/** A list of material variants */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	EPCGExMaterialVariantsMode MaterialVariants = EPCGExMaterialVariantsMode::None;

	/** Material slot index. -1 uses the index inside the container. */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" ├─ Slot Index", EditCondition="!bIsSubCollection && MaterialVariants == EPCGExMaterialVariantsMode::Single", EditConditionHides))
	int32 SlotIndex = 0;

	/** A list of single material variants */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" └─ Variants", EditCondition="!bIsSubCollection && MaterialVariants == EPCGExMaterialVariantsMode::Single", TitleProperty="DisplayName", EditConditionHides))
	TArray<FPCGExMaterialOverrideSingleEntry> MaterialOverrideVariants;

	/** A list of material variants */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" └─ Variants", EditCondition="!bIsSubCollection && MaterialVariants == EPCGExMaterialVariantsMode::Multi", TitleProperty="DisplayName", EditConditionHides))
	TArray<FPCGExMaterialOverrideCollection> MaterialOverrideVariantsList;

	
	/** Descriptor source. */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides, DisplayAfter="Variations"))
	EPCGExEntryVariationMode DescriptorSource = EPCGExEntryVariationMode::Local;
	
	/** Config used when this entry is consumed as an instanced static mesh */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" ├─ ISM Settings", EditCondition="!bIsSubCollection && DescriptorSource == EPCGExEntryVariationMode::Local", EditConditionHides, DisplayAfter="DescriptorSource"))
	FSoftISMComponentDescriptor ISMDescriptor;

	/** Config used when this entry is consumed as a regular static mesh primitive (i.e Spline Mesh)*/
	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" └─ SM Settings", EditCondition="!bIsSubCollection && DescriptorSource == EPCGExEntryVariationMode::Local", EditConditionHides, DisplayAfter="ISMDescriptor"))
	FPCGExStaticMeshComponentDescriptor SMDescriptor;

	bool Matches(const FPCGMeshInstanceList& InstanceList) const
	{
		// TODO : This is way too weak
		return InstanceList.Descriptor.StaticMesh == ISMDescriptor.StaticMesh;
	}

	bool SameAs(const FPCGExMeshCollectionEntry& Other) const
	{
		return
			SubCollection == Other.SubCollection &&
			Weight == Other.Weight &&
			Category == Other.Category &&
			StaticMesh == Other.StaticMesh;
	}

	virtual void GetAssetPaths(TSet<FSoftObjectPath>& OutPaths) const override;

	virtual void GetMaterialPaths(const int32 PickIndex, TSet<FSoftObjectPath>& OutPaths) const;
	virtual void ApplyMaterials(const int32 PickIndex, UStaticMeshComponent* TargetComponent) const;
	virtual void ApplyMaterials(const int32 PickIndex, FPCGSoftISMComponentDescriptor& Descriptor) const;

	virtual bool Validate(const UPCGExAssetCollection* ParentCollection) override;

#pragma region DEPRECATED

	// DEPRECATED -- Moved to macro cache instead.
	UPROPERTY()
	int32 MaterialVariantsCumulativeWeight_DEPRECATED = -1;

	UPROPERTY()
	TArray<int32> MaterialVariantsOrder_DEPRECATED;

	UPROPERTY()
	TArray<int32> MaterialVariantsWeights_DEPRECATED;

#pragma endregion

	virtual void UpdateStaging(const UPCGExAssetCollection* OwningCollection, int32 InInternalIndex, const bool bRecursive) override;
	virtual void SetAssetPath(const FSoftObjectPath& InPath) override;

	void InitPCGSoftISMDescriptor(const UPCGExMeshCollection* ParentCollection, FPCGSoftISMComponentDescriptor& TargetDescriptor) const;

#if WITH_EDITOR
	virtual void EDITOR_Sanitize() override;
#endif

	virtual void BuildMicroCache() override;
};

UCLASS(BlueprintType, DisplayName="[PCGEx] Mesh Collection")
class PCGEXTENDEDTOOLKIT_API UPCGExMeshCollection : public UPCGExAssetCollection
{
	GENERATED_BODY()

	friend struct FPCGExMeshCollectionEntry;
	friend class UPCGExMeshSelectorBase;

public:
	virtual PCGExAssetCollection::EType GetType() const override { return PCGExAssetCollection::EType::Mesh; }

	/** Global descriptor rule. */
	UPROPERTY(EditAnywhere, Category = Settings)
	EPCGExGlobalVariationRule GlobalDescriptorMode = EPCGExGlobalVariationRule::PerEntry;

	/** Config used when this entry is consumed as an instanced static mesh */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" ├─ Global ISM Settings"))
	FSoftISMComponentDescriptor GlobalISMDescriptor;

	/** Config used when this entry is consumed as a regular static mesh primitive (i.e Spline Mesh)*/
	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" └─ Global SM Settings"))
	FPCGExStaticMeshComponentDescriptor GlobalSMDescriptor;
		
#if WITH_EDITOR
	virtual void EDITOR_AddBrowserSelectionInternal(const TArray<FAssetData>& InAssetData) override;
	virtual void EDITOR_RefreshDisplayNames() override;

	/** Disable collision on all entries. */
	UFUNCTION()
	void EDITOR_DisableCollisions();
	
	/** Set Descriptor source on all entries. */
	UFUNCTION()
	void EDITOR_SetDescriptorSourceAll(EPCGExEntryVariationMode DescriptorSource);

#endif

	PCGEX_ASSET_COLLECTION_BOILERPLATE(UPCGExMeshCollection, FPCGExMeshCollectionEntry)

	virtual void EDITOR_RegisterTrackingKeys(FPCGExContext* Context) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta=(TitleProperty="DisplayName"))
	TArray<FPCGExMeshCollectionEntry> Entries;
};

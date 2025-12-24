// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Core/PCGExAssetCollection.h"

#include "Data/Descriptors/PCGExComponentDescriptors.h"
#include "ISMPartition/ISMComponentDescriptor.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"
#include "Engine/StaticMesh.h"
#include "Helpers/PCGExArrayHelpers.h"
#include "UObject/SoftObjectPath.h"

#include "PCGExMeshCollection.generated.h"

class UPCGExMeshCollection;

// Material Override Structures

UENUM()
enum class EPCGExMaterialVariantsMode : uint8
{
	None   = 0 UMETA(DisplayName = "None", ToolTip="No variants.", ActionIcon="STF_None"),
	Single = 1 UMETA(DisplayName = "Single Slot", ToolTip="Single-slot variants, for when there is only a single material slot override.", ActionIcon="SingleMat"),
	Multi  = 2 UMETA(DisplayName = "Multi Slots", ToolTip="Multi-slot variants, more admin, for when there is multiple material slots for the entry.", ActionIcon="MultiMat"),
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Material Override Entry")
struct PCGEXCOLLECTIONS_API FPCGExMaterialOverrideEntry
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
struct PCGEXCOLLECTIONS_API FPCGExMaterialOverrideCollection
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
};

USTRUCT(BlueprintType, DisplayName="[PCGEx] Material Override Single Entry")
struct PCGEXCOLLECTIONS_API FPCGExMaterialOverrideSingleEntry
{
	GENERATED_BODY()

	FPCGExMaterialOverrideSingleEntry() = default;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(ClampMin=1))
	int32 Weight = 1;

	UPROPERTY(EditAnywhere, Category = Settings)
	TSoftObjectPtr<UMaterialInterface> Material = nullptr;
};

namespace PCGExMeshCollection
{
	// Mesh MicroCache - Handles material variant picking
	class PCGEXCOLLECTIONS_API FMicroCache : public PCGExAssetCollection::FMicroCache
	{
		int32 HighestMaterialIndex = -1;

	public:
		FMicroCache() = default;

		virtual PCGExAssetCollection::FTypeId GetTypeId() const override
		{
			return PCGExAssetCollection::TypeIds::Mesh;
		}

		int32 GetHighestIndex() const { return HighestMaterialIndex; }

		void ProcessMaterialOverrides(const TArray<FPCGExMaterialOverrideSingleEntry>& Overrides, int32 InSlotIndex = -1);
		void ProcessMaterialOverrides(const TArray<FPCGExMaterialOverrideCollection>& Overrides);
	};
}

// Mesh Collection Entry
USTRUCT(BlueprintType, DisplayName="[PCGEx] Mesh Collection Entry")
struct PCGEXCOLLECTIONS_API FPCGExMeshCollectionEntry : public FPCGExAssetCollectionEntry
{
	GENERATED_BODY()

	FPCGExMeshCollectionEntry() = default;

	// Type System
	virtual PCGExAssetCollection::FTypeId GetTypeId() const override
	{
		return PCGExAssetCollection::TypeIds::Mesh;
	}

	// Mesh-Specific Properties 

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	TSoftObjectPtr<UStaticMesh> StaticMesh = nullptr;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="bIsSubCollection", EditConditionHides, DisplayAfter="bIsSubCollection"))
	TObjectPtr<UPCGExMeshCollection> SubCollection;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides))
	EPCGExMaterialVariantsMode MaterialVariants = EPCGExMaterialVariantsMode::None;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" ├─ Slot Index", EditCondition="!bIsSubCollection && MaterialVariants == EPCGExMaterialVariantsMode::Single", EditConditionHides))
	int32 SlotIndex = 0;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" └─ Variants", EditCondition="!bIsSubCollection && MaterialVariants == EPCGExMaterialVariantsMode::Single", EditConditionHides))
	TArray<FPCGExMaterialOverrideSingleEntry> MaterialOverrideVariants;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" └─ Variants", EditCondition="!bIsSubCollection && MaterialVariants == EPCGExMaterialVariantsMode::Multi", EditConditionHides))
	TArray<FPCGExMaterialOverrideCollection> MaterialOverrideVariantsList;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(EditCondition="!bIsSubCollection", EditConditionHides, DisplayAfter="Variations"))
	EPCGExEntryVariationMode DescriptorSource = EPCGExEntryVariationMode::Local;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" ├─ ISM Settings", EditCondition="!bIsSubCollection && DescriptorSource == EPCGExEntryVariationMode::Local", EditConditionHides, DisplayAfter="DescriptorSource"))
	FSoftISMComponentDescriptor ISMDescriptor;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" └─ SM Settings", EditCondition="!bIsSubCollection && DescriptorSource == EPCGExEntryVariationMode::Local", EditConditionHides, DisplayAfter="ISMDescriptor"))
	FPCGExStaticMeshComponentDescriptor SMDescriptor;

	// Subcollection Access

	virtual UPCGExAssetCollection* GetSubCollectionPtr() const override;

	virtual void ClearSubCollection() override;

	// Asset & Material Handling

	virtual void GetAssetPaths(TSet<FSoftObjectPath>& OutPaths) const override;
	void GetMaterialPaths(int32 PickIndex, TSet<FSoftObjectPath>& OutPaths) const;
	void ApplyMaterials(int32 PickIndex, UStaticMeshComponent* TargetComponent) const;
	void ApplyMaterials(int32 PickIndex, FPCGSoftISMComponentDescriptor& Descriptor) const;

	// Lifecycle

	virtual bool Validate(const UPCGExAssetCollection* ParentCollection) override;
	virtual void UpdateStaging(const UPCGExAssetCollection* OwningCollection, int32 InInternalIndex, bool bRecursive) override;
	virtual void SetAssetPath(const FSoftObjectPath& InPath) override;

	void InitPCGSoftISMDescriptor(const UPCGExMeshCollection* ParentCollection, FPCGSoftISMComponentDescriptor& TargetDescriptor) const;

#if WITH_EDITOR
	virtual void EDITOR_Sanitize() override;
#endif

	virtual void BuildMicroCache() override;

	// Typed MicroCache Access
	PCGExMeshCollection::FMicroCache* GetMeshMicroCache() const
	{
		return static_cast<PCGExMeshCollection::FMicroCache*>(MicroCache.Get());
	}

#pragma region DEPRECATED
	UPROPERTY()
	int32 MaterialVariantsCumulativeWeight_DEPRECATED = -1;

	UPROPERTY()
	TArray<int32> MaterialVariantsOrder_DEPRECATED;

	UPROPERTY()
	TArray<int32> MaterialVariantsWeights_DEPRECATED;
#pragma endregion
};

// Mesh Collection
UCLASS(BlueprintType, DisplayName="[PCGEx] Mesh Collection")
class PCGEXCOLLECTIONS_API UPCGExMeshCollection : public UPCGExAssetCollection
{
	GENERATED_BODY()
	PCGEX_ASSET_COLLECTION_BODY(FPCGExMeshCollectionEntry)

public:
	friend struct FPCGExMeshCollectionEntry;

	// Type System
	virtual PCGExAssetCollection::FTypeId GetTypeId() const override
	{
		return PCGExAssetCollection::TypeIds::Mesh;
	}

	// Mesh-Specific Properties

	UPROPERTY(EditAnywhere, Category = Settings)
	EPCGExGlobalVariationRule GlobalDescriptorMode = EPCGExGlobalVariationRule::PerEntry;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" ├─ Global ISM Settings"))
	FSoftISMComponentDescriptor GlobalISMDescriptor;

	UPROPERTY(EditAnywhere, Category = Settings, meta=(DisplayName=" └─ Global SM Settings"))
	FPCGExStaticMeshComponentDescriptor GlobalSMDescriptor;

	// Entries Array
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	TArray<FPCGExMeshCollectionEntry> Entries;


#if WITH_EDITOR
	// Editor Functions

	virtual void EDITOR_AddBrowserSelectionInternal(const TArray<FAssetData>& InAssetData) override;

	UFUNCTION()
	void EDITOR_DisableCollisions();

	UFUNCTION()
	void EDITOR_SetDescriptorSourceAll(EPCGExEntryVariationMode DescriptorSource);
#endif
};

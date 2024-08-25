// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "MeshSelectors/PCGMeshSelectorBase.h"

#include "PCGExMeshSelectorBase.generated.h"

struct FPCGExMeshCollectionEntry;
class UPCGExMeshCollection;

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural))
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExMeshSelectorBase : public UPCGMeshSelectorBase
{
	GENERATED_BODY()

public:
	// ~Begin UObject interface
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// ~End UObject interface

	virtual bool SelectInstances(
		FPCGStaticMeshSpawnerContext& Context,
		const UPCGStaticMeshSpawnerSettings* Settings,
		const UPCGPointData* InPointData,
		TArray<FPCGMeshInstanceList>& OutMeshInstances,
		UPCGPointData* OutPointData) const override;

	virtual void BeginDestroy() override;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSelector, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExMeshCollection> MainCollection;

	TObjectPtr<UPCGExMeshCollection> MainCollectionPtr;

	// TODO : Expose material overrides when API is available

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSelector, meta = (InlineEditConditionToggle))
	bool bUseAttributeMaterialOverrides = false;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName = "By Attribute Material Overrides", Category = MeshSelector, meta = (EditCondition = "bUseAttributeMaterialOverrides"))
	TArray<FName> MaterialOverrideAttributes;

protected:
	virtual void RefreshInternal();

	virtual bool Setup(
		FPCGStaticMeshSpawnerContext& Context,
		const UPCGStaticMeshSpawnerSettings* Settings,
		const UPCGPointData* InPointData,
		UPCGPointData* OutPointData) const;

	virtual bool Execute(
		FPCGStaticMeshSpawnerContext& Context,
		const UPCGStaticMeshSpawnerSettings* Settings,
		const UPCGPointData* InPointData,
		TArray<FPCGMeshInstanceList>& OutMeshInstances,
		UPCGPointData* OutPointData,
		TArray<FPCGPoint>* OutPoints = nullptr,
		FPCGMetadataAttribute<FString>* OutAttributeId = nullptr) const;

	//virtual void RegisterPick(FPCGMetadataAttribute<FString>* OutAttributeId, const FPCGExMeshCollectionEntry& Pick,)
	void CollapseInstances(TArray<TArray<FPCGMeshInstanceList>>& MeshInstances, TArray<FPCGMeshInstanceList>& OutMeshInstances) const;

	virtual FPCGMeshInstanceList& GetInstanceList(
		TArray<FPCGMeshInstanceList>& InstanceLists,
		const FPCGExMeshCollectionEntry& Pick,
		bool bReverseCulling,
		const int AttributePartitionIndex = INDEX_NONE) const;
};

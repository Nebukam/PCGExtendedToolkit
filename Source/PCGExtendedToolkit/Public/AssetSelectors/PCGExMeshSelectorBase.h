// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGExRandom.h"
#include "MeshSelectors/PCGMeshSelectorBase.h"

#include "PCGExMeshSelectorBase.generated.h"

struct FPCGExMeshCollectionEntry;
class UPCGExMeshCollection;

namespace PCGExMeshSelection
{
	struct /*PCGEXTENDEDTOOLKIT_API*/ FCtx
	{
		FPCGStaticMeshSpawnerContext* Context = nullptr;
		const UPCGStaticMeshSpawnerSettings* Settings = nullptr;
		const UPCGPointData* InPointData = nullptr;
		TArray<FPCGMeshInstanceList>* OutMeshInstances = nullptr;
		UPCGPointData* OutPointData = nullptr;
		TArray<FPCGPoint>* OutPoints = nullptr;
		FPCGMetadataAttribute<FString>* OutAttribute = nullptr;
	};
}

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExSeedComponents"))
	uint8 SeedComponents = 0;

	/** Note that this is only accounted for if selected in the seed component. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSelector, meta=(PCG_Overridable))
	int32 LocalSeed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSelector, meta=(PCG_Overridable))
	TSoftObjectPtr<UPCGExMeshCollection> MainCollection;

	TObjectPtr<UPCGExMeshCollection> MainCollectionPtr;

	// TODO : Expose material overrides when API is available

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = MeshSelector, meta = (InlineEditConditionToggle))
	bool bUseAttributeMaterialOverrides = false;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, DisplayName = "By Attribute Material Overrides", Category = MeshSelector, meta = (EditCondition = "bUseAttributeMaterialOverrides"))
	TArray<FName> MaterialOverrideAttributes;

protected:
	const int32 TimeSlicingCheckFrequency = 1024;

	virtual void RefreshInternal();

	virtual bool Setup(
		FPCGStaticMeshSpawnerContext& Context,
		const UPCGStaticMeshSpawnerSettings* Settings,
		const UPCGPointData* InPointData,
		UPCGPointData* OutPointData) const;

	virtual bool Execute(PCGExMeshSelection::FCtx& Ctx) const;
	virtual FPCGMeshInstanceList& RegisterPick(const FPCGExMeshCollectionEntry& Entry, const FPCGPoint& Point, const int32 PointIndex, PCGExMeshSelection::FCtx& Ctx) const;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3

	virtual FPCGMeshInstanceList& GetInstanceList(
		TArray<FPCGMeshInstanceList>& InstanceLists,
		const FPCGExMeshCollectionEntry& Pick,
		bool bReverseCulling,
		const int AttributePartitionIndex = INDEX_NONE) const;

#else
	
	virtual FPCGMeshInstanceList& GetInstanceList(
		TArray<FPCGMeshInstanceList>& InstanceLists,
		const FPCGExMeshCollectionEntry& Pick,
		bool bReverseCulling) const;
	
#endif
};

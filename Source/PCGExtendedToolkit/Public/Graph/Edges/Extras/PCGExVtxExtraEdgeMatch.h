// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "PCGExVtxExtraFactoryProvider.h"
#include "Graph/PCGExCluster.h"
#include "Graph/PCGExGraph.h"

#include "PCGExVtxExtraEdgeMatch.generated.h"

///

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExEdgeMatchSettings
{
	GENERATED_BODY()

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExFetchType DirectionSource = EPCGExFetchType::Constant;

	/** Whether to transform the direction source by the vtx' transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bTransformSource = false;

	/** Matching edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExEdgeOutputWithIndexSettings MatchingEdge = FPCGExEdgeOutputWithIndexSettings(TEXT("Matching"));

	/** Dot comparison settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExDotComparisonSettings DotComparisonSettings;
};

/**
 * ó
 */
UCLASS()
class PCGEXTENDEDTOOLKIT_API UPCGExVtxExtraEdgeMatch : public UPCGExVtxExtraOperation
{
	GENERATED_BODY()

public:
	FPCGExEdgeMatchSettings Descriptor;

	virtual void CopySettingsFrom(const UPCGExOperation* Other) override;
	virtual bool PrepareForVtx(const FPCGContext* InContext, PCGExData::FPointIO* InVtx) override;
	virtual void ProcessNode(const PCGExCluster::FCluster* Cluster, PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency) override;
	virtual void Write() override;
	virtual void Write(PCGExMT::FTaskManager* AsyncManager) override;
	virtual void Cleanup() override;

protected:
	PCGEx::TFAttributeWriter<FVector>* MatchingDirWriter = nullptr;
	PCGEx::TFAttributeWriter<double>* MatchingLenWriter = nullptr;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExVtxExtraEdgeMatchFactory : public UPCGExVtxExtraFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExEdgeMatchSettings Descriptor;
	virtual UPCGExVtxExtraOperation* CreateOperation() const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|VtxExtra")
class PCGEXTENDEDTOOLKIT_API UPCGExVtxExtraEdgeMatchSettings : public UPCGExVtxExtraProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		NeighborSamplerAttribute, "Vtx Extra : Edge Match", "Find the edge that matches the closest provided direction.",
		FName(GetDisplayName()))

#endif
	//~End UPCGSettings

public:
	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Direction Settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExEdgeMatchSettings Descriptor;
};

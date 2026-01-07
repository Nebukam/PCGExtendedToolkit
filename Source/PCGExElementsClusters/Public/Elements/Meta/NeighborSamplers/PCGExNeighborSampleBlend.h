// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "Factories/PCGExFactoryProvider.h"
#include "PCGExNeighborSampleFactoryProvider.h"


#include "PCGExNeighborSampleBlend.generated.h"

///

class UPCGExNeighborSamplerFactoryBlend;

/**
 * 
 */
class FPCGExNeighborSampleBlend : public FPCGExNeighborSampleOperation
{
public:
	const UPCGExNeighborSamplerFactoryBlend* Factory = nullptr;
	TSharedPtr<PCGExBlending::FBlendOpsManager> BlendOpsManager;

	virtual void PrepareForCluster(FPCGExContext* InContext, TSharedRef<PCGExClusters::FCluster> InCluster, TSharedRef<PCGExData::FFacade> InVtxDataFacade, TSharedRef<PCGExData::FFacade> InEdgeDataFacade) override;

	virtual void PrepareForLoops(const TArray<PCGExMT::FScope>& Loops) override;
	virtual void PrepareNode(const PCGExClusters::FNode& TargetNode, const PCGExMT::FScope& Scope) const override;

	virtual void SampleNeighborNode(const PCGExClusters::FNode& TargetNode, const PCGExGraphs::FLink Lk, const double Weight, const PCGExMT::FScope& Scope) override;
	virtual void SampleNeighborEdge(const PCGExClusters::FNode& TargetNode, const PCGExGraphs::FLink Lk, const double Weight, const PCGExMT::FScope& Scope) override;

	virtual void FinalizeNode(const PCGExClusters::FNode& TargetNode, const int32 Count, const double TotalWeight, const PCGExMT::FScope& Scope) override;
	virtual void CompleteOperation() override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExNeighborSamplerFactoryBlend : public UPCGExNeighborSamplerFactoryData
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExNeighborSampleOperation> CreateOperation(FPCGExContext* InContext) const override;

	UPROPERTY()
	TArray<TObjectPtr<const UPCGExBlendOpFactory>> BlendingFactories;

	virtual bool RegisterConsumableAttributes(FPCGExContext* InContext) const override;
	virtual void RegisterVtxBuffersDependencies(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, PCGExData::FFacadePreloader& FacadePreloader) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|NeighborSample", meta=(PCGExNodeLibraryDoc="sampling/sample-neighbors/sampler-vtx-properties"))
class UPCGExNeighborSampleBlendSettings : public UPCGExNeighborSampleProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(NeighborSamplerAttribute, "Sampler : Vtx Blend", "Create a vtx attribute sampler that uses blend operations to blend values from neighbors.")
#endif
	//~End UPCGSettings

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

public:
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

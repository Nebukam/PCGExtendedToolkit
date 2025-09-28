﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "PCGExVtxPropertyFactoryProvider.h"

#include "PCGExVtxPropertySpecialEdges.generated.h"

USTRUCT(BlueprintType)
struct FPCGExSpecialEdgesConfig
{
	GENERATED_BODY()

	/** Shortest edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExEdgeOutputWithIndexSettings ShortestEdge = FPCGExEdgeOutputWithIndexSettings(TEXT("Shortest"));

	/** Longest edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExEdgeOutputWithIndexSettings LongestEdge = FPCGExEdgeOutputWithIndexSettings(TEXT("Longest"));

	/** Average edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExSimpleEdgeOutputSettings AverageEdge = FPCGExSimpleEdgeOutputSettings(TEXT("Average"));
};

/**
 * 
 */
class FPCGExVtxPropertySpecialEdges : public FPCGExVtxPropertyOperation
{
public:
	FPCGExSpecialEdgesConfig Config;

	virtual bool PrepareForCluster(
		FPCGExContext* InContext,
		TSharedPtr<PCGExCluster::FCluster> InCluster,
		const TSharedPtr<PCGExData::FFacade>& InVtxDataFacade,
		const TSharedPtr<PCGExData::FFacade>& InEdgeDataFacade) override;
	virtual void ProcessNode(PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency) override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExVtxPropertySpecialEdgesFactory : public UPCGExVtxPropertyFactoryData
{
	GENERATED_BODY()

public:
	FPCGExSpecialEdgesConfig Config;
	virtual TSharedPtr<FPCGExVtxPropertyOperation> CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|VtxProperty", meta=(PCGExNodeLibraryDoc="clusters/metadata/vtx-properties/vtx-special-edges"))
class UPCGExVtxPropertySpecialEdgesSettings : public UPCGExVtxPropertyProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(VtxSpecialEdges, "Vtx : Special Edges", "Edge' edge cases (pun not intended)")
#endif
	//~End UPCGSettings

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Direction Settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExSpecialEdgesConfig Config;
};

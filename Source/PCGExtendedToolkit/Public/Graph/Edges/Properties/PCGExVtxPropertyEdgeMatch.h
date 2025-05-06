// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "PCGExDetailsData.h"
#include "UObject/Object.h"

#include "PCGExFactoryProvider.h"
#include "PCGExVtxPropertyFactoryProvider.h"
#include "Data/PCGExPointFilter.h"




#include "Graph/PCGExCluster.h"
#include "Graph/PCGExGraph.h"

#include "PCGExVtxPropertyEdgeMatch.generated.h"

///

USTRUCT(BlueprintType)
struct FPCGExEdgeMatchConfig
{
	GENERATED_BODY()

	/** Direction orientation */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExAdjacencyDirectionOrigin Origin = EPCGExAdjacencyDirectionOrigin::FromNode;

	/** Where to read the compared direction from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType DirectionInput = EPCGExInputValueType::Constant;

	/** Operand B for testing -- Will be translated to `double` under the hood. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Direction (Attr)", EditCondition="DirectionInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector Direction;

	/** Direction for computing the dot product against the edge's. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Direction", EditCondition="DirectionInput==EPCGExInputValueType::Constant", EditConditionHides))
	FVector DirectionConstant = FVector::ForwardVector;

	/** Whether to transform the direction source by the vtx' transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bTransformDirection = true;

	/** Dot comparison settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExDotComparisonDetails DotComparisonDetails;

	/** Matching edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Output"))
	FPCGExEdgeOutputWithIndexSettings MatchingEdge = FPCGExEdgeOutputWithIndexSettings(TEXT("Matching"));

	void Sanitize()
	{
		DirectionConstant = DirectionConstant.GetSafeNormal();
	}
};

/**
 * 
 */
class FPCGExVtxPropertyEdgeMatch : public FPCGExVtxPropertyOperation
{
public:
	FPCGExEdgeMatchConfig Config;

	TArray<TObjectPtr<const UPCGExFilterFactoryData>>* FilterFactories = nullptr;

	virtual bool PrepareForCluster(
		const FPCGExContext* InContext,
		TSharedPtr<PCGExCluster::FCluster> InCluster,
		const TSharedPtr<PCGExData::FFacade>& InVtxDataFacade,
		const TSharedPtr<PCGExData::FFacade>& InEdgeDataFacade) override;
	virtual void ProcessNode(PCGExCluster::FNode& Node, const TArray<PCGExCluster::FAdjacencyData>& Adjacency) override;

protected:
	TSharedPtr<PCGExDetails::TSettingValue<FVector>> DirCache;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExVtxPropertyEdgeMatchFactory : public UPCGExVtxPropertyFactoryData
{
	GENERATED_BODY()

public:
	FPCGExEdgeMatchConfig Config;
	virtual TSharedPtr<FPCGExVtxPropertyOperation> CreateOperation(FPCGExContext* InContext) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|VtxProperty")
class UPCGExVtxPropertyEdgeMatchSettings : public UPCGExVtxPropertyProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		VtxEdgeMatch, "Vtx : Edge Match", "Find the edge that matches the closest provided direction.",
		FName(GetDisplayName()))
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

public:
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif

	/** Direction Settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExEdgeMatchConfig Config;
};

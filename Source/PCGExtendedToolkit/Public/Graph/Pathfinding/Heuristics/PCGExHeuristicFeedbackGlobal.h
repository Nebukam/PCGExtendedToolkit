// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExHeuristicFeedback.h"
#include "PCGExHeuristicsFactoryProvider.h"
#include "UObject/Object.h"
#include "Graph/PCGExCluster.h"

#include "PCGExHeuristicFeedbackGlobal.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExHeuristicDescriptorFeedbackGlobal : public FPCGExHeuristicDescriptorFeedback
{
	GENERATED_BODY()

	FPCGExHeuristicDescriptorFeedbackGlobal() :
		FPCGExHeuristicDescriptorFeedback()
	{
	}
};

/**
 * 
 */
UCLASS(DisplayName = "Global Feedback")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicFeedbackGlobal : public UPCGExHeuristicFeedback
{
	GENERATED_BODY()

public:
	virtual void PrepareForData(PCGExCluster::FCluster* InCluster) override;
};

////

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGHeuristicsFactoryFeedbackGlobal : public UPCGHeuristicsFactoryFeedback
{
	GENERATED_BODY()

public:
	virtual bool IsGlobal() const override { return true; }
	
	FPCGExHeuristicDescriptorFeedbackGlobal Descriptor;
	virtual UPCGExHeuristicOperation* CreateOperation() const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicFeedbackGlobalProviderSettings : public UPCGExHeuristicsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(NodeFilter, "Heuristics : Global Feedback", "Heuristics based on visited score FeedbackGlobal.")
#endif
	//~End UPCGSettings

	/** Filter Descriptor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExHeuristicDescriptorFeedbackGlobal Descriptor;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;
};

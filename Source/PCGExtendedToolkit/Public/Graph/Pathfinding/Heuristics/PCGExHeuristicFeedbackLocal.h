// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExHeuristicFeedback.h"
#include "PCGExHeuristicsFactoryProvider.h"
#include "UObject/Object.h"
#include "PCGExHeuristicOperation.h"
#include "Graph/PCGExCluster.h"

#include "PCGExHeuristicFeedbackLocal.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExHeuristicDescriptorFeedbackLocal : public FPCGExHeuristicDescriptorFeedback
{
	GENERATED_BODY()

	FPCGExHeuristicDescriptorFeedbackLocal() :
		FPCGExHeuristicDescriptorFeedback()
	{
	}
	
};

/**
 * 
 */
UCLASS(Abstract, DisplayName = "Local Feedback")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicFeedbackLocal : public UPCGExHeuristicOperation
{
	GENERATED_BODY()

};

////

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGHeuristicsFactoryFeedbackLocal : public UPCGHeuristicsFactoryBase
{
	GENERATED_BODY()

public:
	FPCGExHeuristicDescriptorFeedbackLocal Descriptor;	
	virtual UPCGExHeuristicOperation* CreateOperation() const override;
};

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExHeuristicFeedbackLocalProviderSettings : public UPCGExHeuristicsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(NodeFilter, "Heuristics : Local Feedback", "Heuristics based on visited score FeedbackLocal.")
#endif
	//~End UPCGSettings

	/** Filter Descriptor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExHeuristicDescriptorFeedbackLocal Descriptor;

	virtual UPCGExParamFactoryBase* CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const override;
	
};

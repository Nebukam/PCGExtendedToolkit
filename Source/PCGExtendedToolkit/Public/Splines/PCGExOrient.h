// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "SubPoints/Orient/PCGExSubPointsOrient.h"
#include "PCGExOrient.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExOrientSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExOrientSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Orient, "Orient", "Orient paths segments.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, Instanced)
	UPCGExSubPointsOrient* Orientation;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExOrientContext : public FPCGExPathProcessorContext
{
	friend class FPCGExOrientElement;

public:
	UPCGExSubPointsOrient* Orientation;
};

class PCGEXTENDEDTOOLKIT_API FPCGExOrientElement : public FPCGExPathProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

class PCGEXTENDEDTOOLKIT_API FOrientTask : public FPCGExAsyncTask
{
public:
	FOrientTask(UPCGExAsyncTaskManager* InManager, const PCGExMT::FTaskInfos& InInfos) :
		FPCGExAsyncTask(InManager, InInfos)
	{
	}

	virtual void ExecuteTask() override;

};
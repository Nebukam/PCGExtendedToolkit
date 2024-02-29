// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"

#include "Data/PCGExGraphDefinition.h"

#include "PCGExCreateNodeTest.generated.h"

/** Outputs a single GraphParam to be consumed by other nodes */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExCreateNodeTestSettings : public UPCGSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	bool bCacheResult = false;
	PCGEX_NODE_INFOS_CUSTOM_TASKNAME(
		NodeTest, "Node Test Definition", "Creates a single node test condition to be used by a Node State.",
		FName(FString(Descriptor.GetDisplayName())))
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorFilter; }

#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

public:
	/** Test Descriptor.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExAdjacencyTestDescriptor Descriptor;
};

class PCGEXTENDEDTOOLKIT_API FPCGExCreateNodeTestElement : public IPCGElement
{
public:
#if WITH_EDITOR
	virtual bool ShouldLog() const override { return false; }
#endif

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
};

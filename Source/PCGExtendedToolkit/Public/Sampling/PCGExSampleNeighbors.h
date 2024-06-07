// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Geometry/PCGExGeo.h"
#include "Graph/PCGExEdgesProcessor.h"

#include "PCGExSampleNeighbors.generated.h"

class UPCGExNeighborSampleOperation;

namespace PCGExSampleNeighbors
{
	constexpr PCGExMT::AsyncState State_ReadyForNextOperation = __COUNTER__;
	constexpr PCGExMT::AsyncState State_Sampling = __COUNTER__;
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExSampleNeighborsSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExSampleNeighborsSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleNeighbors, "Sample : Neighbors", "Sample graph node' neighbors values.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExEditorSettings>()->NodeColorSampler; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

private:
	friend class FPCGExSampleNeighborsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSampleNeighborsContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExSampleNeighborsElement;
	virtual ~FPCGExSampleNeighborsContext() override;

	TArray<UPCGExNeighborSampleOperation*> SamplingOperations;
	TArray<UPCGExNeighborSampleOperation*> ToBePreparedOperations;
	UPCGExNeighborSampleOperation* CurrentOperation = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSampleNeighborsElement : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

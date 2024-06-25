// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Graph/PCGExIntersections.h"
#include "PCGExConnectPoints.generated.h"

class UPCGExProbeFactoryBase;
class UPCGExProbeOperation;

namespace PCGExGraph
{
	struct FCompoundProcessor;
}

namespace PCGExDataBlending
{
	class FMetadataBlender;
	class FCompoundBlender;
}

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExConnectPointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathsToEdgeClusters, "Graph : Connect Points", "Connect points according to a set of probes");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorGraphGen; }
#endif
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual FName GetMainOutputLabel() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Graph Output Settings", meta = (PCG_Overridable, DisplayName="Graph Output Settings", ShowOnlyInnerProperties))
	FPCGExGraphBuilderSettings GraphBuilderSettings;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExConnectPointsContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExConnectPointsElement;

	virtual ~FPCGExConnectPointsContext() override;

	TArray<UPCGExProbeFactoryBase*> ProbeFactories;

	PCGExData::FPointIOCollection* MainVtx = nullptr;
	PCGExData::FPointIOCollection* MainEdges = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExConnectPointsElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExConnectPoints
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
		TArray<UPCGExProbeOperation*> ProbeOperations;
		TArray<UPCGExProbeOperation*> DirectProbeOperations;
		bool bUseVariableRadius = false;
		double MaxRadiusSquared = TNumericLimits<double>::Min();

		const UPCGPointData::PointOctree* Octree = nullptr;
		
		const FPCGPoint* StartPtr = nullptr;
		const TArray<FPCGPoint>* InPoints = nullptr;
		TArray<FVector> Positions;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints)
			: FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}

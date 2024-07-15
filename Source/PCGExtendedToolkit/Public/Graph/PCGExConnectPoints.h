// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Geometry/PCGExGeo.h"
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
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ConnectPoints, "Cluster : Connect Points", "Connect points according to a set of probes");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterGen; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual FName GetMainOutputLabel() const override { return PCGExGraph::OutputVerticesLabel; }
	//~End UPCGExPointsProcessorSettings

public:
	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bPreventStacking = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bPreventStacking", ClampMin=0.00001, ClampMax=1))
	double StackingPreventionTolerance = 0.001;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bProjectPoints = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bProjectPoints", DisplayName="Project Points"))
	FPCGExGeo2DProjectionDetails ProjectionDetails = FPCGExGeo2DProjectionDetails(false);

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExConnectPointsContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExConnectPointsElement;

	virtual ~FPCGExConnectPointsContext() override;

	TArray<UPCGExProbeFactoryBase*> ProbeFactories;
	TArray<UPCGExFilterFactoryBase*> GeneratorsFiltersFactories;
	TArray<UPCGExFilterFactoryBase*> ConnetablesFiltersFactories;

	PCGExData::FPointIOCollection* MainVtx = nullptr;
	PCGExData::FPointIOCollection* MainEdges = nullptr;

	FVector CWStackingTolerance = FVector::ZeroVector;
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
	struct PCGEXTENDEDTOOLKIT_API FPositionRef
	{
		int32 Index;
		FBoxSphereBounds Bounds;

		FPositionRef(const int32 InItemIndex, const FBoxSphereBounds& InBounds)
			: Index(InItemIndex), Bounds(InBounds)
		{
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FPositionRefSemantics
	{
		enum { MaxElementsPerLeaf = 16 };

		enum { MinInclusiveElementsPerNode = 7 };

		enum { MaxNodeDepth = 12 };

		using ElementAllocator = TInlineAllocator<MaxElementsPerLeaf>;

		FORCEINLINE static const FBoxSphereBounds& GetBoundingBox(const FPositionRef& InPosition)
		{
			return InPosition.Bounds;
		}

		FORCEINLINE static const bool AreElementsEqual(const FPositionRef& A, const FPositionRef& B)
		{
			return A.Index == B.Index;
		}

		FORCEINLINE static void ApplyOffset(FPositionRef& InNode)
		{
			ensureMsgf(false, TEXT("Not implemented"));
		}

		FORCEINLINE static void SetElementId(const FPositionRef& Element, FOctreeElementId2 OctreeElementID)
		{
		}
	};

	using PositionOctree = TOctree2<FPositionRef, FPositionRefSemantics>;

	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		PCGExPointFilter::TManager* GeneratorsFilter = nullptr;
		PCGExPointFilter::TManager* ConnectableFilter = nullptr;
		
		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
		TArray<UPCGExProbeOperation*> ProbeOperations;
		TArray<UPCGExProbeOperation*> DirectProbeOperations;
		TArray<UPCGExProbeOperation*> ChainProbeOperations;
		TArray<UPCGExProbeOperation*> SharedProbeOperations;
		bool bUseVariableRadius = false;
		int32 NumChainedOps = 0;
		double SharedSearchRadius = TNumericLimits<double>::Min();

		TArray<bool> CanGenerate;
		PositionOctree* Octree = nullptr;

		const TArray<FPCGPoint>* InPoints = nullptr;
		TArray<FTransform> CachedTransforms;

		TArray<TSet<uint64>*> DistributedEdgesSet;
		FPCGExGeo2DProjectionDetails ProjectionDetails;

		bool bPreventStacking = false;
		bool bUseProjection = false;
		FVector CWStackingTolerance = FVector::ZeroVector;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints)
			: FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		void OnPreparationComplete();
		virtual void PrepareLoopScopesForPoints(const TArray<uint64>& Loops) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}

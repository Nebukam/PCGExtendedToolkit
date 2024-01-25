// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPathsToEdgeClusters.generated.h"

namespace PCGExGraph
{
	struct FEdgeCrossingsHandler;
	class FGraph;

	struct PCGEXTENDEDTOOLKIT_API FLooseNode
	{
		FVector Center;
		int32 Index;

		TArray<int32> Neighbors;
		TArray<uint64> FusedPoints;

		FLooseNode(const FVector& InCenter, const int32 InIndex)
			: Center(InCenter),
			  Index(InIndex)
		{
			Neighbors.Empty();
			FusedPoints.Empty();
		}

		bool Add(FLooseNode* OtherNode)
		{
			if (OtherNode->Index == Index) { return false; }
			if (Neighbors.Contains(OtherNode->Index)) { return true; }

			Neighbors.Add(OtherNode->Index);
			OtherNode->Add(this);

			return true;
		}

		void Add(const uint64 Point)
		{
			FusedPoints.AddUnique(Point);
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FLooseNetwork
	{
		TArray<FLooseNode*> Nodes;

		double Tolerance;

		explicit FLooseNetwork(const double InTolerance)
			: Tolerance(InTolerance)
		{
			Nodes.Empty();
		}

		~FLooseNetwork()
		{
			PCGEX_DELETE_TARRAY(Nodes)
		}

		FLooseNode* GetLooseNode(const FPCGPoint& Point)
		{
			const FVector Position = Point.Transform.GetLocation();

			for (FLooseNode* LNode : Nodes)
			{
				if ((Position - LNode->Center).IsNearlyZero(Tolerance)) { return LNode; }
			}
			return Nodes.Add_GetRef(new FLooseNode(Position, Nodes.Num()));
		}
	};
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExPathsToEdgeClustersSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPathsToEdgeClustersSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathsToEdgeClusters, "Path : To Edge Clusters", "Merge paths to edge clusters for glorious pathfinding inception");
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual FName GetMainInputLabel() const override;
	virtual FName GetMainOutputLabel() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Distance at which points are fused */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0.001))
	double FuseDistance = 10;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Graph Output Settings"))
	FPCGExGraphBuilderSettings GraphBuilderSettings;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPathsToEdgeClustersContext : public FPCGExPathProcessorContext
{
	friend class FPCGExPathsToEdgeClustersElement;

	virtual ~FPCGExPathsToEdgeClustersContext() override;

	PCGExGraph::FLooseNetwork* LooseNetwork;
	TMap<PCGExData::FPointIO*, int32> IOIndices;

	PCGExData::FPointIO* ConsolidatedPoints = nullptr;

	FPCGExGraphBuilderSettings GraphBuilderSettings;
	PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPathsToEdgeClustersElement : public FPCGExPathProcessorElement
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

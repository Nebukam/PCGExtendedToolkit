﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExData.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPathsToEdgeIslands.generated.h"

namespace PCGExGraph
{
	struct FEdgeCrossingsHandler;
	struct FEdgeNetwork;

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
class PCGEXTENDEDTOOLKIT_API UPCGExPathsToEdgeIslandsSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPathsToEdgeIslandsSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathsToEdgeIslands, "Path : To Edge Islands", "Merge paths to edge islands for glorious pathfinding inception");
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

	/** If two edges are close enough, create a "crossing" point. !!! VERY EXPENSIVE !!! */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bFindCrossings = false;

	/** Distance at which segments are considered crossing. !!! VERY EXPENSIVE !!!*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bFindCrossings", ClampMin=0.001))
	double CrossingTolerance = 10;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPathsToEdgeIslandsContext : public FPCGExPathProcessorContext
{
	friend class FPCGExPathsToEdgeIslandsElement;

	virtual ~FPCGExPathsToEdgeIslandsContext() override;

	bool bFindCrossings;
	double CrossingTolerance;

	PCGExGraph::FLooseNetwork* LooseNetwork;
	TMap<PCGExData::FPointIO*, int32> IOIndices;

	mutable FRWLock NetworkLock;
	TSet<int32> VisitedNodes;
	PCGExGraph::FEdgeNetwork* EdgeNetwork = nullptr;
	PCGExGraph::FEdgeCrossingsHandler* EdgeCrossings = nullptr;
	PCGExData::FPointIOGroup* IslandsIO;

	PCGExData::FKPointIOMarkedBindings<int32>* Markings = nullptr;

	PCGExData::FPointIO* ConsolidatedPoints = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPathsToEdgeIslandsElement : public FPCGExPathProcessorElement
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

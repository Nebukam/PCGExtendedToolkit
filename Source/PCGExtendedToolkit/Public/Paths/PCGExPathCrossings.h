// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"
#include "PCGExPaths.h"

#include "PCGExPointsProcessor.h"
#include "Geometry/PCGExGeo.h"
#include "PCGExPathCrossings.generated.h"


class UPCGExSubPointsBlendOperation;
/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPathCrossingsSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathCrossings, "Path × Path Crossings", "Find crossing points between & inside paths.");
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** Consider paths to be closed -- processing will wrap between first and last points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedPath = false;

	/** If enabled, crossings are only computed per path, against themselves only. Note: this ignores the "bEnableSelfIntersection" from details below. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	bool bSelfIntersectionOnly = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExPathEdgeIntersectionDetails IntersectionDetails;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault))
	TObjectPtr<UPCGExSubPointsBlendOperation> Blending;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bFlagCrossing = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bFlagCrossing"))
	FName CrossingFlagAttributeName = "bIsCrossing";
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathCrossingsContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExPathCrossingsElement;

	virtual ~FPCGExPathCrossingsContext() override;

	TArray<UPCGExFilterFactoryBase*> CanCutFilterFactories;
	TArray<UPCGExFilterFactoryBase*> CanBeCutFilterFactories;

	UPCGExSubPointsBlendOperation* Blending = nullptr;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathCrossingsElement final : public FPCGExPathProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExPathCrossings
{
	struct /*PCGEXTENDEDTOOLKIT_API*/ FCrossing
	{
		int32 Index = -1;

		TArray<uint64> Crossings; // Point Index | IO Index
		TArray<FVector> Positions;
		TArray<double> Alphas;

		FCrossing(const int32 InIndex):
			Index(InIndex)
		{
		}
	};

	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		FPCGExPathCrossingsContext* LocalTypedContext = nullptr;

		bool bClosedPath = false;
		bool bSelfIntersectionOnly = false;

		int32 NumPoints = 0;
		int32 LastIndex = 0;

		TArray<FVector> Positions;
		TArray<double> Lengths;
		TArray<PCGExPaths::FPathEdge*> Edges;
		TArray<FCrossing*> Crossings;

		PCGExPointFilter::TManager* CanCutFilterManager = nullptr;
		PCGExPointFilter::TManager* CanBeCutFilterManager = nullptr;

		UPCGExSubPointsBlendOperation* Blending = nullptr;

		using TEdgeOctree = TOctree2<PCGExPaths::FPathEdge*, PCGExPaths::FPathEdgeSemantics>;
		TEdgeOctree* EdgeOctree = nullptr;

		FPCGExPathEdgeIntersectionDetails Details;

		PCGEx::TAttributeWriter<bool>* FlagWriter = nullptr;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints)
			: FPointsProcessor(InPoints)
		{
		}

		virtual bool IsTrivial() const override { return false; } // Force non-trivial because this shit is expensive

		virtual ~FProcessor() override;
		const TEdgeOctree* GetEdgeOctree() const { return EdgeOctree; }

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount) override;
		void OnSearchComplete();
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}

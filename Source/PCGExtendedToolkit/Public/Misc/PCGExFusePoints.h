// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSettings.h"
#include "Graph/PCGExClusterMT.h"
#include "Graph/PCGExGraph.h"

#include "PCGExFusePoints.generated.h"


namespace PCGExDataBlending
{
	class FCompoundBlender;
}

namespace PCGExGraph
{
	struct FCompoundGraph;
}

namespace PCGExFuse
{
	PCGEX_ASYNC_STATE(State_FindingFusePoints)
	PCGEX_ASYNC_STATE(State_MergingPoints)

	struct PCGEXTENDEDTOOLKIT_API FFusedPoint
	{
		mutable FRWLock IndicesLock;
		int32 Index = -1;
		FVector Position = FVector::ZeroVector;
		TArray<int32> Fused;
		TArray<double> Distances;
		double MaxDistance = 0;

		FFusedPoint(const int32 InIndex, const FVector& InPosition)
			: Index(InIndex), Position(InPosition)
		{
			Fused.Empty();
			Distances.Empty();
		}

		~FFusedPoint()
		{
			Fused.Empty();
			Distances.Empty();
		}

		FORCEINLINE void Add(const int32 InIndex, const double Distance)
		{
			FWriteScopeLock WriteLock(IndicesLock);
			Fused.Add(InIndex);
			Distances.Add(Distance);
			MaxDistance = FMath::Max(MaxDistance, Distance);
		}
	};
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExFusePointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExFusePointsSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FusePoints, "Fuse Points", "Fuse points based on distance.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscRemove; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Fuse Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Point/Point Settings"))
	FPCGExPointPointIntersectionSettings PointPointIntersectionSettings;

	/** Preserve the order of input points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bPreserveOrder = true;

	/** Defines how fused point properties and attributes are merged together. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExBlendingSettings BlendingSettings;

private:
	friend class FPCGExFusePointsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFusePointsContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExFusePointsElement;
	virtual ~FPCGExFusePointsContext() override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExFusePointsElement final : public FPCGExPointsProcessorElement
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExFusePoints
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		PCGExGraph::FGraphMetadataSettings GraphMetadataSettings;
		PCGExGraph::FCompoundGraph* CompoundGraph = nullptr;
		PCGExDataBlending::FCompoundBlender* CompoundPointsBlender = nullptr;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints)
			: FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}

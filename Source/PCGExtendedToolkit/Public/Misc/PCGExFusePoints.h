// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExDetailsIntersection.h"
#include "Data/PCGExDataFilter.h"
#include "Data/Blending/PCGExUnionBlender.h"
#include "Data/Blending/PCGExDataBlending.h"


#include "Graph/PCGExGraph.h"
#include "Graph/PCGExIntersections.h"

#include "PCGExFusePoints.generated.h"

namespace PCGExFuse
{
	struct /*PCGEXTENDEDTOOLKIT_API*/ FFusedPoint
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
		}

		~FFusedPoint() = default;

		FORCEINLINE void Add(const int32 InIndex, const double Distance)
		{
			FWriteScopeLock WriteLock(IndicesLock);
			Fused.Add(InIndex);
			Distances.Add(Distance);
			MaxDistance = FMath::Max(MaxDistance, Distance);
		}
	};
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFusePointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FusePoints, "Fuse Points", "Fuse points based on distance.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscRemove; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Fuse Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Point/Point Settings"))
	FPCGExPointPointIntersectionDetails PointPointIntersectionDetails = FPCGExPointPointIntersectionDetails(false);

	/** Preserve the order of input points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bPreserveOrder = true;

	/** Defines how fused point properties and attributes are merged together. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExBlendingDetails BlendingDetails;

	/** Meta filter settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Carry Over Settings"))
	FPCGExCarryOverDetails CarryOverDetails;

private:
	friend class FPCGExFusePointsElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFusePointsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExFusePointsElement;
	FPCGExCarryOverDetails CarryOverDetails;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFusePointsElement final : public FPCGExPointsProcessorElement
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExFusePoints
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExFusePointsContext, UPCGExFusePointsSettings>
	{
		PCGExGraph::FGraphMetadataDetails GraphMetadataDetails;
		TSharedPtr<PCGExGraph::FUnionGraph> UnionGraph;
		TSharedPtr<PCGExDataBlending::FUnionBlender> UnionBlender;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TPointsProcessor(InPointDataFacade)
		{
			bInlineProcessPoints = true;
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSettings.h"
#include "Data/Blending/PCGExMetadataBlender.h"
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
	constexpr PCGExMT::AsyncState State_FindingFusePoints = __COUNTER__;
	constexpr PCGExMT::AsyncState State_MergingPoints = __COUNTER__;

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
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorMiscRemove; }
#endif

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

struct PCGEXTENDEDTOOLKIT_API FPCGExFusePointsContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExFusePointsElement;

	virtual ~FPCGExFusePointsContext() override;

	PCGExGraph::FGraphMetadataSettings GraphMetadataSettings;
	PCGExGraph::FCompoundGraph* CompoundGraph = nullptr;
	PCGExDataBlending::FCompoundBlender* CompoundPointsBlender = nullptr;

	bool bPreserveOrder;

	mutable FRWLock PointsLock;
};

class PCGEXTENDEDTOOLKIT_API FPCGExFusePointsElement : public FPCGExPointsProcessorElementBase
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

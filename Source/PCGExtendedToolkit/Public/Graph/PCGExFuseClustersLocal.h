// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCluster.h"
#include "PCGExEdgesProcessor.h"

#include "PCGExFuseClustersLocal.generated.h"

namespace PCGExDataBlending
{
	class FCompoundBlender;
}

/**
 * A Base node to process a set of point using GraphParams.
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExFuseClustersLocalSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FuseClustersLocal, "Graph : Fuse Clusters Local", "Finds per-cluster Point/Edge and Edge/Edge intersections");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorGraph; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExEdgesProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;
	//~End UPCGExEdgesProcessorSettings interface

	/** Fuse Settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Point/Point Settings"))
	FPCGExPointPointIntersectionSettings PointPointIntersectionSettings;

	/** Point-Edge intersection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bDoPointEdgeIntersection;

	/** Point-Edge intersection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Point/Edge Settings", EditCondition="bDoPointEdgeIntersection"))
	FPCGExPointEdgeIntersectionSettings PointEdgeIntersectionSettings;

	/** Edge-Edge intersection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bDoEdgeEdgeIntersection;

	/** Edge-Edge intersection */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Edge/Edge Settings", EditCondition="bDoEdgeEdgeIntersection"))
	FPCGExEdgeEdgeIntersectionSettings EdgeEdgeIntersectionSettings;

	/** Defines how fused point properties and attributes are merged together for fused points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending")
	FPCGExBlendingSettings PointsBlendingSettings;

	/** Defines how fused point properties and attributes are merged together for fused edges. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data Blending")
	FPCGExBlendingSettings EdgesBlendingSettings;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable, DisplayName="Graph Output Settings", ShowOnlyInnerProperties))
	FPCGExGraphBuilderSettings GraphBuilderSettings;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExFuseClustersLocalContext : public FPCGExEdgesProcessorContext
{
	friend class UPCGExFuseClustersLocalSettings;
	friend class FPCGExFuseClustersLocalElement;

	virtual ~FPCGExFuseClustersLocalContext() override;

	FPCGExPointPointIntersectionSettings PointPointIntersectionSettings;
	FPCGExPointEdgeIntersectionSettings PointEdgeIntersectionSettings;
	FPCGExEdgeEdgeIntersectionSettings EdgeEdgeIntersectionSettings;

	PCGExGraph::FCompoundGraph* CompoundGraph = nullptr;

	FPCGExGraphBuilderSettings GraphBuilderSettings;
	PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;

	PCGExGraph::FPointEdgeIntersections* PointEdgeIntersections = nullptr;
	PCGExGraph::FEdgeEdgeIntersections* EdgeEdgeIntersections = nullptr;

	PCGExGraph::FGraphMetadataSettings GraphMetadataSettings;
	PCGExDataBlending::FCompoundBlender* CompoundPointsBlender = nullptr;
	PCGExDataBlending::FCompoundBlender* CompoundEdgesBlender = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExFuseClustersLocalElement : public FPCGExEdgesProcessorElement
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

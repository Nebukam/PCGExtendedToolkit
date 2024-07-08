// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"


#include "Geometry/PCGExGeo.h"

#include "PCGExBuildDelaunayGraph.generated.h"

namespace PCGExGeo
{
	class TDelaunay3;
}

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExBuildDelaunayGraphSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildDelaunayGraph, "Cluster : Delaunay 3D", "Create a 3D delaunay tetrahedralization for each input dataset.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterGen; }
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual FName GetMainOutputLabel() const override { return PCGExGraph::OutputVerticesLabel; }
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** Output the Urquhart graph of the Delaunay triangulation (removes the longest edge of each Delaunay cell) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bUrquhart = false;

	/** Mark points & edges that lie on the hull */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bMarkHull = true;

	/** Name of the attribute to output the Hull boolean to. True if point is on the hull, otherwise false. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bMarkHull"))
	FName HullAttributeName = "bIsOnHull";

	/** When true, edges that have at least a point on the Hull as marked as being on the hull. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bMarkEdgeOnTouch = false;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails = FPCGExGraphBuilderDetails(false);

private:
	friend class FPCGExBuildDelaunayGraphElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBuildDelaunayGraphContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExBuildDelaunayGraphElement;

	virtual ~FPCGExBuildDelaunayGraphContext() override;
};


class PCGEXTENDEDTOOLKIT_API FPCGExBuildDelaunayGraphElement final : public FPCGExPointsProcessorElement
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

namespace PCGExBuildDelaunay
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
	protected:
		PCGExGeo::TDelaunay3* Delaunay = nullptr;
		PCGExGraph::FGraphBuilder* GraphBuilder = nullptr;

		PCGEx::TFAttributeWriter<bool>* HullMarkPointWriter = nullptr;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}

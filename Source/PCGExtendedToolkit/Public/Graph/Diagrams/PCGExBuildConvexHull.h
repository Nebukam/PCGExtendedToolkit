﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"


#include "Geometry/PCGExGeoDelaunay.h"
#include "Graph/PCGExGraph.h"


#include "PCGExBuildConvexHull.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/diagrams/convex-hull-3d"))
class UPCGExBuildConvexHullSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildConvexHull, "Cluster : Convex Hull 3D", "Create a 3D Convex Hull triangulation for each input dataset.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorClusterGenerator; }
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual PCGExData::EIOInit GetMainDataInitializationPolicy() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual FName GetMainOutputPin() const override;
	//~End UPCGExPointsProcessorSettings

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails = FPCGExGraphBuilderDetails();

private:
	friend class FPCGExBuildConvexHullElement;
};

struct FPCGExBuildConvexHullContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBuildConvexHullElement;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExBuildConvexHullElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(BuildConvexHull)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExBuildConvexHull
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExBuildConvexHullContext, UPCGExBuildConvexHullSettings>
	{
	protected:
		TSharedPtr<TArray<int32>> OutputIndices;
		TUniquePtr<PCGExGeo::TDelaunay3> Delaunay;
		TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder;

		TArray<uint64> Edges;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
		virtual void Output() override;
	};
}

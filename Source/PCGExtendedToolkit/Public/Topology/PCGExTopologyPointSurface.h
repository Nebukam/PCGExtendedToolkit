﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "PCGExTopology.h"
#include "Data/PCGDynamicMeshData.h"

#include "Geometry/PCGExGeo.h"

#include "PCGExTopologyPointSurface.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(Keywords = "triangulation", PCGExNodeLibraryDoc="clusters/diagrams/delaunay-2d"))
class UPCGExTopologyPointSurfaceSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(TopologyPointSurface, "Topology : Point Surface", "Create a delaunay triangulated surface for each input dataset.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::DynamicMesh; }
	virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor::White; }
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Projection settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, InlineEditConditionToggle))
	bool bAttemptRepair = false;

	/** Degeneration settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bAttemptRepair"))
	FGeometryScriptDegenerateTriangleOptions RepairDegenerate;

	/** Topology settings. Some settings will be ignored based on selected output mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExTopologyDetails Topology;

private:
	friend class FPCGExTopologyPointSurfaceElement;
};

struct FPCGExTopologyPointSurfaceContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExTopologyPointSurfaceElement;
	virtual void RegisterAssetDependencies() override;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExTopologyPointSurfaceElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(TopologyPointSurface)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExTopologyPointSurface
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExTopologyPointSurfaceContext, UPCGExTopologyPointSurfaceSettings>
	{
	protected:
		bool bIsPreviewMode = false;

		TObjectPtr<UDynamicMesh> InternalMesh;
		UPCGDynamicMeshData* InternalMeshData = nullptr;
		FPCGExGeo2DProjectionDetails ProjectionDetails;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void Output() override;
	};
}

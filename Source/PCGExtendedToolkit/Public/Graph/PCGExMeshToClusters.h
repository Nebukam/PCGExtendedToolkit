// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExDataForward.h"


#include "Geometry/PCGExGeo.h"
#include "Geometry/PCGExGeoMesh.h"

#include "PCGExMeshToClusters.generated.h"

namespace PCGExGeo
{
	class FGeoStaticMesh;
}

namespace PCGExGeo
{
	class FGeoStaticMeshMap;
}

namespace PCGExGeo
{
	class TVoronoiMesh3;
}

namespace PCGExGeo
{
	class TConvexHull3;
	class TDelaunayTriangulation3;
}

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Mesh Source Type"))
enum class EPCGExMeshAttributeHandling : uint8
{
	StaticMeshSoftPath UMETA(DisplayName = "StaticMesh Soft Path", ToolTip="Read the attribute as a StaticMesh soft path."),
	ActorReference UMETA(DisplayName = "Actor Reference", ToolTip="Read the attribute as an actor reference to extract primitive from."),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExMeshToClustersSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MeshToClusters, "Mesh to Clusters", "Creates clusters from mesh topology.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterGen; }
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual FName GetMainInputLabel() const override { return PCGEx::SourceTargetsLabel; }
	virtual FName GetMainOutputLabel() const override { return PCGExGraph::OutputVerticesLabel; }
	virtual bool GetMainAcceptMultipleData() const override { return false; }
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** Triangulation type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExTriangulationType GraphOutputType = EPCGExTriangulationType::Raw;

	/** Mesh source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExFetchType StaticMeshSource = EPCGExFetchType::Constant;

	/** Static mesh constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="StaticMeshSource==EPCGExFetchType::Constant", EditConditionHides))
	TSoftObjectPtr<UStaticMesh> StaticMeshConstant;

	/** Static mesh path attribute -- Either FString, FName or FSoftObjectPath*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="StaticMeshSource==EPCGExFetchType::Attribute", EditConditionHides))
	FName StaticMeshAttribute;

	/** Static mesh path attribute type*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="StaticMeshSource==EPCGExFetchType::Attribute", EditConditionHides))
	EPCGExMeshAttributeHandling AttributeHandling;

	/** Target inherit behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExTransformDetails TransformDetails;

	/** Skip invalid meshes & do not throw warning about them. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bIgnoreMeshWarnings = false;

	/** Graph & Edges output properties. Only available if bPruneOutsideBounds as it otherwise generates a complete graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails;

	/** Which input points attributes to forward on clusters. NOTE : Not implemented */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExForwardDetails AttributesForwarding;

private:
	friend class FPCGExMeshToClustersElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMeshToClustersContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExMeshToClustersElement;

	FPCGExGraphBuilderDetails GraphBuilderDetails;
	FPCGExTransformDetails TransformDetails;

	PCGExGeo::FGeoStaticMeshMap* StaticMeshMap = nullptr;
	TArray<int32> MeshIdx;

	PCGExData::FPointIOCollection* RootVtx = nullptr;
	PCGExData::FPointIOCollection* VtxChildCollection = nullptr;
	PCGExData::FPointIOCollection* EdgeChildCollection = nullptr;

	TArray<PCGExGraph::FGraphBuilder*> GraphBuilders;

	virtual ~FPCGExMeshToClustersContext() override;
};


class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMeshToClustersElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExMeshToCluster
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FExtractMeshAndBuildGraph final : public PCGExMT::FPCGExTask
	{
	public:
		FExtractMeshAndBuildGraph(
			PCGExData::FPointIO* InPointIO,
			PCGExGeo::FGeoStaticMesh* InMesh) :
			FPCGExTask(InPointIO),
			Mesh(InMesh)
		{
		}

		PCGExGeo::FGeoStaticMesh* Mesh = nullptr;

		virtual bool ExecuteTask() override;
	};
}

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExCustomGraphProcessor.h"
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
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph")
class PCGEXTENDEDTOOLKIT_API UPCGExMeshToClustersSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MeshToClusters, "Graph : Mesh To Clusters", "Creates clusters from mesh topology.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExEditorSettings>()->NodeColorGraphGen; }
#endif
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual FName GetMainInputLabel() const override;
	virtual FName GetMainOutputLabel() const override;
	virtual bool GetMainAcceptMultipleData() const override;
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

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
	FPCGExTransformSettings TransformSettings;
	
	/** Skip invalid meshes & do not throw warning about them. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bIgnoreMeshWarnings = false;

	/** Graph & Edges output properties. Only available if bPruneOutsideBounds as it otherwise generates a complete graph. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditConditionHides, DisplayName="Graph Output Settings"))
	FPCGExGraphBuilderSettings GraphBuilderSettings;

private:
	friend class FPCGExMeshToClustersElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExMeshToClustersContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExMeshToClustersElement;

	FPCGExGraphBuilderSettings GraphBuilderSettings;
	FPCGExTransformSettings TransformSettings;

	PCGExGeo::FGeoStaticMeshMap* StaticMeshMap = nullptr;
	TArray<int32> MeshIdx;

	PCGExData::FPointIOCollection* RootVtx = nullptr;
	PCGExData::FPointIOCollection* VtxChildCollection = nullptr;
	PCGExData::FPointIOCollection* EdgeChildCollection = nullptr;
	

	TArray<PCGExGraph::FGraphBuilder*> GraphBuilders;

	FPCGExGraphBuilderSettings BuilderSettings;

	virtual ~FPCGExMeshToClustersContext() override;
};


class PCGEXTENDEDTOOLKIT_API FPCGExMeshToClustersElement : public FPCGExPointsProcessorElementBase
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

namespace PCGExMeshToCluster
{
	class PCGEXTENDEDTOOLKIT_API FExtractMeshAndBuildGraph : public FPCGExNonAbandonableTask
	{
	public:
		FExtractMeshAndBuildGraph(
			FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
			PCGExGeo::FGeoStaticMesh* InMesh) :
			FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
			Mesh(InMesh)
		{
		}

		PCGExGeo::FGeoStaticMesh* Mesh = nullptr;

		virtual bool ExecuteTask() override;
	};
}

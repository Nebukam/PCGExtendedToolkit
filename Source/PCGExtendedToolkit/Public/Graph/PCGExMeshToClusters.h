// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExDataForward.h"


#include "Geometry/PCGExGeoMesh.h"

#include "PCGExMeshToClusters.generated.h"

UENUM()
enum class EPCGExMeshAttributeHandling : uint8
{
	StaticMeshSoftPath = 0 UMETA(DisplayName = "StaticMesh Soft Path", ToolTip="Read the attribute as a StaticMesh soft path."),
	ActorReference     = 1 UMETA(DisplayName = "Actor Reference", ToolTip="Read the attribute as an actor reference to extract primitive from."),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
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
	virtual FName GetMainInputPin() const override { return PCGEx::SourceTargetsLabel; }
	virtual FName GetMainOutputPin() const override { return PCGExGraph::OutputVerticesLabel; }
	virtual bool GetMainAcceptMultipleData() const override { return false; }
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/** Triangulation type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExTriangulationType GraphOutputType = EPCGExTriangulationType::Raw;

	/** Mesh source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType StaticMeshInput = EPCGExInputValueType::Constant;

	/** Static mesh constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Static Mesh", EditCondition="StaticMeshInput==EPCGExInputValueType::Constant", EditConditionHides))
	TSoftObjectPtr<UStaticMesh> StaticMeshConstant;

	/** Static mesh path attribute -- Either FString, FName or FSoftObjectPath*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Static Mesh", EditCondition="StaticMeshInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FName StaticMeshAttribute;

	/** Static mesh path attribute type*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="StaticMeshInput!=EPCGExInputValueType::Constant", EditConditionHides))
	EPCGExMeshAttributeHandling AttributeHandling; // TODO : Refactor this to support both. We care about primitives, not where they come from.

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

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExMeshToClustersContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExMeshToClustersElement;

	FPCGExGraphBuilderDetails GraphBuilderDetails;
	FPCGExTransformDetails TransformDetails;

	TSharedPtr<PCGExData::FFacade> TargetsDataFacade;
	TSharedPtr<PCGExGeo::FGeoStaticMeshMap> StaticMeshMap;
	TArray<int32> MeshIdx;

	TSharedPtr<PCGExData::FPointIOCollection> RootVtx;
	TSharedPtr<PCGExData::FPointIOCollection> VtxChildCollection;
	TSharedPtr<PCGExData::FPointIOCollection> EdgeChildCollection;

	TArray<TSharedPtr<PCGExGraph::FGraphBuilder>> GraphBuilders;
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
	class /*PCGEXTENDEDTOOLKIT_API*/ FExtractMeshAndBuildGraph final : public PCGExMT::FPCGExIndexedTask
	{
	public:
		FExtractMeshAndBuildGraph(
			const int32 InTaskIndex,
			const TSharedPtr<PCGExGeo::FGeoStaticMesh>& InMesh) :
			FPCGExIndexedTask(InTaskIndex),
			Mesh(InMesh)
		{
		}

		TSharedPtr<PCGExGeo::FGeoStaticMesh> Mesh;

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};
}

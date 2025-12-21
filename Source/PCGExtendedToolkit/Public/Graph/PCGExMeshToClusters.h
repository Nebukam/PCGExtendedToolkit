// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExLabels.h"
#include "Transform/PCGExFitting.h"
#include "Core/PCGExPointsProcessor.h"
#include "Data/Utils/PCGExDataForward.h"
#include "Core/PCGExGraphDetails.h"


#include "Geometry/PCGExGeoMesh.h"

#include "PCGExMeshToClusters.generated.h"

namespace PCGExGraph
{
	class FGraphBuilder;
}

UENUM()
enum class EPCGExMeshAttributeHandling : uint8
{
	StaticMeshSoftPath = 0 UMETA(DisplayName = "StaticMesh Soft Path", ToolTip="Read the attribute as a StaticMesh soft path."),
	ActorReference     = 1 UMETA(DisplayName = "Actor Reference", ToolTip="Read the attribute as an actor reference to extract primitive from."),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="clusters/mesh-to-clusters"))
class UPCGExMeshToClustersSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(MeshToClusters, "Mesh to Clusters", "Creates clusters from mesh topology.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorClusterGenerator; }
	virtual bool CanDynamicallyTrackKeys() const override { return true; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual FName GetMainInputPin() const override { return PCGEx::SourceTargetsLabel; }
	virtual FName GetMainOutputPin() const override { return PCGExGraph::OutputVerticesLabel; }
	virtual bool GetMainAcceptMultipleData() const override { return false; }
	//~End UPCGExPointsProcessorSettings

	/** Triangulation type */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExTriangulationType GraphOutputType = EPCGExTriangulationType::Raw;

	/** Mesh source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExInputValueType StaticMeshInput = EPCGExInputValueType::Constant;

	/** Static mesh path attribute -- Either FString, FName or FSoftObjectPath*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Static Mesh (Attr)", EditCondition="StaticMeshInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName StaticMeshAttribute = "Mesh";

	/** Static mesh constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Static Mesh", EditCondition="StaticMeshInput == EPCGExInputValueType::Constant", EditConditionHides))
	TSoftObjectPtr<UStaticMesh> StaticMeshConstant;

	/** Static mesh path attribute type*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="StaticMeshInput != EPCGExInputValueType::Constant", EditConditionHides))
	EPCGExMeshAttributeHandling AttributeHandling; // TODO : Refactor this to support both. We care about primitives, not where they come from.

	/** Target inherit behavior */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExTransformDetails TransformDetails;

	/** Which data should be imported from the static mesh onto the generated points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeoMeshImportDetails ImportDetails;

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

struct FPCGExMeshToClustersContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExMeshToClustersElement;

	FPCGExGraphBuilderDetails GraphBuilderDetails;
	FPCGExTransformDetails TransformDetails;
	FPCGExGeoMeshImportDetails ImportDetails;

	TSharedPtr<PCGExData::FFacade> TargetsDataFacade;
	TSharedPtr<PCGExMath::FGeoStaticMeshMap> StaticMeshMap;
	TArray<int32> MeshIdx;

	TSharedPtr<PCGExData::FPointIOCollection> RootVtx;
	TSharedPtr<PCGExData::FPointIOCollection> VtxChildCollection;
	TSharedPtr<PCGExData::FPointIOCollection> EdgeChildCollection;
	TSharedPtr<PCGExData::FPointIOCollection> BaseMeshDataCollection;

	TArray<TSharedPtr<PCGExGraph::FGraphBuilder>> GraphBuilders;

	bool bWantsImport = false;
};


class FPCGExMeshToClustersElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(MeshToClusters)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;

	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
};

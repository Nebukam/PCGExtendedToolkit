// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Graph/PCGExIntersections.h"
#include "PCGExFindClustersData.generated.h"

UENUM(/*E--BlueprintType, meta=(BDisplayName="[PCGEx] Cluster Data Search Mode")--E*/)
enum class EPCGExClusterDataSearchMode : uint8
{
	All          = 0 UMETA(DisplayName = "All"),
	VtxFromEdges = 1 UMETA(DisplayName = "Vtx from Edges"),
	EdgesFromVtx = 2 UMETA(DisplayName = "Edges from Vtx"),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExFindClustersDataSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(FindClustersData, "Find Clusters", "Find vtx/edge pairs inside a soup of data collections");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorEdge; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual FName GetMainOutputPin() const override { return PCGExGraph::OutputVerticesLabel; }
	FName GetSearchOutputPin() const { return SearchMode == EPCGExClusterDataSearchMode::VtxFromEdges ? FName("Edges") : FName("Vtx"); }
	//~End UPCGExPointsProcessorSettings

	/** Search mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	EPCGExClusterDataSearchMode SearchMode = EPCGExClusterDataSearchMode::VtxFromEdges;

	/** Warning about inputs mismatch and triage */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	bool bSkipTrivialWarnings = false;

	/** Warning that you'll get anyway if you try these inputs in a cluster node*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	bool bSkipImportantWarnings = false;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFindClustersDataContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExFindClustersDataElement;

	FString SearchKey = TEXT("");
	TSharedPtr<PCGExData::FPointIO> SearchKeyIO;
	TSharedPtr<PCGExData::FPointIOCollection> MainEdges;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFindClustersDataElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

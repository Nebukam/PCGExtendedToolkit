// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"

#include "Graph/PCGExGraph.h"
#include "PCGExBuildCustomGraph.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Custom Graph Actor Source Mode"))
enum class EPCGExCustomGraphActorSourceMode : uint8
{
	Owner           = 0 UMETA(DisplayName = "Owner", ToolTip="PCG Component owner"),
	ActorReferences = 1 UMETA(DisplayName = "Actor References", ToolTip="Point data with an actor reference property."),
};

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, Abstract, DisplayName = "[PCGEx] Custom Graph Settings")
class PCGEXTENDEDTOOLKIT_API UPCGExCustomGraphSettings : public UObject
{
	GENERATED_BODY()

public:
	/** Maximum number of node in the graph. The final number can be less, as isolated points will be pruned; but no edge endpoint' index should be greater that this number. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "PCGEx|Data")
	int32 MaxNodesNum = 0;

	/** Internal index of these settings. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "PCGEx|Data")
	int32 Index = 0;

	TSet<uint64> UniqueEdges;
	int32 MaxIndex = -1;
	TSharedPtr<PCGExGraph::FGraphBuilder> GraphBuilder;
	
	/**
	 * Create an edge between two nodes in an indexed graph.
	 * @param InStartIndex 
	 * @param InEndIndex 
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Data")
	void AddEdge(const int32 InStartIndex, const int32 InEndIndex);

	/**
	 * Initialization method. It is called right before Build Graph -- this is where you must set the max number of nodes.
	 * @param InContext Context of the execution
	 * @param OutMaxNodesNum The maximum number of node this graph will be working with.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "PCGEx|Execution")
	void InitializeSettings(const FPCGContext& InContext, int32& OutMaxNodesNum);
	
	/**
	 * Main execution function. Called once per requested graphs. This method is executed in a multi-threaded context, Graph Settings are safe but the custom builder wrapper itself isn't.
	 * @param InContext Context of the execution
	 * @param OutSuccess Whether building was successful or not
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "PCGEx|Execution")
	void BuildGraph(const FPCGContext& InContext, bool& OutSuccess);
	
	/**
	 * Update Node Point is called on each node point after BuildGraph has been, and edges added. This method is executed in a multi-threaded context.
	 * This is where point transform & properties should be set.
	 * @param InNodeIndex Index of the node the given point matches with
	 * @param InPoint PCG Point that represents the node
	 * @param OutPoint Muted PCG Point that represents the node.
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "PCGEx|Execution")
	void UpdateNodePoint(const int32 InNodeIndex, const FPCGPoint& InPoint, FPCGPoint& OutPoint) const;
};

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, Abstract, MinimalAPI, DisplayName = "[PCGEx] Custom Graph Builder")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExCustomGraphBuilder : public UPCGExOperation
{
	GENERATED_BODY()

public:
	/**
	 * Main initialization function. Called once, and is responsible for populating graph builder settings.
	 * At least one setting is expected to be found in the GraphSettings array. This is executed on the main thread.
	 * @param InContext - Context of the initialization
	 * @param OutSuccess
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "PCGEx|Execution")
	void InitializeWithContext(const FPCGContext& InContext, bool& OutSuccess);

	/**
	 * Create an edge between two nodes in an indexed graph. This method is executed in a multi-threaded context
	 * @param SettingsClass
	 * @param InMaxNumNodes
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Execution")
	UPCGExCustomGraphSettings* CreateGraphSettings(TSubclassOf<UPCGExCustomGraphSettings> SettingsClass);

	/**
	 * Main execution function. Called once per requested graphs. This method is executed in a multi-threaded context, Graph Settings are safe but the custom builder wrapper itself isn't.
	 * @param InContext - Context of the execution
	 * @param InCustomGraphSettings
	 * @param OutSuccess
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "PCGEx|Execution")
	void BuildGraph(const FPCGContext& InContext, UPCGExCustomGraphSettings* InCustomGraphSettings, bool& OutSuccess);

	/**
	 * Update Node Point is called on each node point after BuildGraph has been, and edges added. This method is executed in a multi-threaded context.
	 * This is where point transform & properties should be set.
	 * @param InCustomGraphSettings
	 * @param InNodeIndex 
	 * @param InPoint
	 * @param OutPoint 
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "PCGEx|Execution")
	void UpdateNodePoint(const UPCGExCustomGraphSettings* InCustomGraphSettings, const int32 InNodeIndex, const FPCGPoint& InPoint, FPCGPoint& OutPoint) const;

	virtual void Cleanup() override
	{
		InputActors.Empty();

		for (UPCGExCustomGraphSettings* GSettings : GraphSettings) { GSettings->GraphBuilder.Reset(); }

		GraphSettings.Empty();
		Super::Cleanup();
	}

	UPROPERTY(BlueprintReadOnly, Category = "PCGEx|Inputs")
	TArray<TObjectPtr<AActor>> InputActors;

	UPROPERTY(BlueprintReadOnly, Category = "PCGEx|Outputs")
	TArray<TObjectPtr<UPCGExCustomGraphSettings>> GraphSettings;
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBuildCustomGraphSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BuildCustomGraph, "Cluster : Build Custom Graph", "Create clusters using custom blueprint objects");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorClusterGen; }
#endif

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual FName GetMainOutputLabel() const override { return PCGExGraph::OutputVerticesLabel; }
	virtual bool IsInputless() const override { return Mode == EPCGExCustomGraphActorSourceMode::Owner; }
	//~End UPCGExPointsProcessorSettings

	/** Actor fetching mode. These actors will be forwarded to the builder so it can fetch components and data from there during its initialization. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExCustomGraphActorSourceMode Mode = EPCGExCustomGraphActorSourceMode::Owner;

	/** Actor reference */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Mode==EPCGExCustomGraphActorSourceMode::ActorReferences", EditConditionHides))
	FName ActorReferenceAttribute = FName(TEXT("ActorReference"));

	/** Builder instance. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta = (PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExCustomGraphBuilder> Builder;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bMuteUnprocessedSettingsWarning = false;

	/** Graph & Edges output properties */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Cluster Output Settings"))
	FPCGExGraphBuilderDetails GraphBuilderDetails;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBuildCustomGraphContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBuildCustomGraphElement;
	UPCGExCustomGraphBuilder* Builder = nullptr;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBuildCustomGraphElement final : public FPCGExPointsProcessorElement
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

namespace PCGExBuildCustomGraph
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FBuildGraph final : public PCGExMT::FPCGExTask
	{
	public:
		FBuildGraph(const TSharedPtr<PCGExData::FPointIO>& InPointIO,
		            UPCGExCustomGraphSettings* InGraphSettings) :
			FPCGExTask(InPointIO), GraphSettings(InGraphSettings)
		{
		}

		UPCGExCustomGraphSettings* GraphSettings = nullptr;
		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};
}

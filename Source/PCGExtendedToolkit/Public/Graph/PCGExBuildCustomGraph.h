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

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCustomGraphSettings
{
	GENERATED_BODY()

	FPCGExCustomGraphSettings()
	{
	}

	/** Maximum number of node in the graph. The final number can be less, as isolated points will be pruned; but no edge endpoint' index should be greater that this number. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	int32 MaxNumNodes = 0;
	
	TSet<uint64> UniqueEdges;
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
	 * At least one setting is expected to be found in the GraphSettings array.
	 * @param InContext - Context of the initialization
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "PCGEx|Execution")
	void InitializeWithContext(const FPCGContext& InContext);

	/**
	 * Main execution function. Called once per requested graphs.
	 * @param InContext - Context of the execution
	 * @param InGraphIndex
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "PCGEx|Execution")
	void BuildGraph(const FPCGContext& InContext, const int32 InGraphIndex);

	/**
	 * Create an edge between two nodes in an indexed graph.
	 * @param InGraphIndex 
	 * @param InStartIndex 
	 * @param InEndIndex 
	 */
	UFUNCTION(BlueprintCallable, Category = "PCGEx|Execution")
	void AddEdge(int32 InGraphIndex, int32 InStartIndex, int32 InEndIndex);

	/**
	 * Update Node Point is called on each node point after BuildGraph has been called. This method is executed asynchronously, and in parallel.
	 * This is where point transform & properties should be set.
	 * @param InGraphIndex 
	 * @param InNodeIndex 
	 * @param InPoint
	 * @param OutPoint 
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "PCGEx|Execution")
	void UpdateNodePoint(const int32 InGraphIndex, const int32 InNodeIndex, const FPCGPoint& InPoint, FPCGPoint& OutPoint);

	virtual void Cleanup() override
	{
		InputActors.Empty();
		Super::Cleanup();
	}

	UPROPERTY(BlueprintReadOnly, Category = "PCGEx|Inputs")
	TArray<TObjectPtr<AActor>> InputActors;

	UPROPERTY(BlueprintReadWrite, Category = "PCGEx|Outputs")
	TArray<FPCGExCustomGraphSettings> GraphSettings;

	
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	EPCGExCustomGraphActorSourceMode Mode = EPCGExCustomGraphActorSourceMode::Owner;

	/** Actor reference */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="Mode==EPCGExCustomGraphActorSourceMode::ActorReferences", EditConditionHides))
	FName ActorReferenceAttribute;

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
	TSharedPtr<TArray<TSharedPtr<PCGExGraph::FGraphBuilder>>> GraphBuilders;
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
		FBuildGraph(const TSharedPtr<PCGExData::FPointIO>& InPointIO) :
			FPCGExTask(InPointIO)
		{
		}

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};
}

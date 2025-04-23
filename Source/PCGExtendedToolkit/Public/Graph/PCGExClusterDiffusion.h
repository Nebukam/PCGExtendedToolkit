// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDetailsData.h"
#include "PCGExScopedContainers.h"


#include "Graph/PCGExEdgesProcessor.h"
#include "Sampling/PCGExSampling.h"
#include "PCGExClusterDiffusion.generated.h"

#define PCGEX_FOREACH_FIELD_CLUSTER_DIFF(MACRO)\
MACRO(DiffusionDepth, int32, 0)\
MACRO(DiffusionDistance, double, 0)

UCLASS(Hidden, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters")
class UPCGExClusterDiffusionSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

	//~Begin UObject interface
public:
	//~End UObject interface

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ClusterDiffusion, "Cluster : Diffusion", "Diffuses vtx attributes onto their neighbors.");
#endif

	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=1))
	int32 Iterations = 10;

	/** Influence Settings*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExInfluenceDetails InfluenceDetails;

	// Notes
	// - Max Range mode
	//		- Count
	//		- Distance
	//		- Radius
	// - Stop conditions (optional)
	// - Priority management (which vtx overrides which)
	//		- Fallback to next best candidate, if available, otherwise stop "growth"
	//		- Rely on this to sort diffusions and process capture in order
	//		- 
	//
	// - First compute growth map with reverse parent lookup
	//		- Maintain list of active candidates "layer"
	//			- Find direct neighbors
	//			- Sort them by depth then score? depth only? score only? -> Need sort mode
	//			- Since there is no goal, using heuristics may prove tricky
	//				- Might need new detail to override roaming coordinates per-vtx (should be useful elsewhere, i.e cluster refine)

	// Many formal steps required
	// - Build layers and growth, sorting based on score (can be parallelized)
	// - Capture node (must be inlined, can use large batches)
	// - Once all growths have been stopped, blending can happen

	/** Write the diffusion depth the vtx was subjected to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDiffusionDepth = false;

	/** Name of the 'int32' attribute to write diffusion depth to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Diffusion Depth", PCG_Overridable, EditCondition="bWriteDiffusionDepth"))
	FName DiffusionDepthAttributeName = FName("DiffusionDepth");

	/** Write the final diffusion distance. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteDiffusionDistance = false;

	/** Name of the 'double' attribute to write diffusion distance to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Diffusion Distance", PCG_Overridable, EditCondition="bWriteDiffusionDistance"))
	FName DiffusionDistanceAttributeName = FName("DiffusionDistance");

private:
	friend class FPCGExClusterDiffusionElement;
};

struct FPCGExClusterDiffusionContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExClusterDiffusionElement;

	PCGEX_FOREACH_FIELD_CLUSTER_DIFF(PCGEX_OUTPUT_DECL_TOGGLE)

};

class FPCGExClusterDiffusionElement final : public FPCGExEdgesProcessorElement
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

namespace PCGExClusterDiffusion
{

	struct FCandidate
	{
		int32 Index;
		int32 Depth;
		double Score;
		double Distance;
	};
	
	class FDiffusion : public TSharedFromThis<FDiffusion>
	{
		TArray<FCandidate> Candidates;
		TSharedPtr<PCGEx::FMapHashLookup> TravelStack; // Required for heuristics
		// use map hash lookup to reduce memory overhead of a shared map + thread safety yay

	public:
		bool bStopped = false;
		
		FDiffusion() = default;
		~FDiffusion() = default;

	};
	
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExClusterDiffusionContext, UPCGExClusterDiffusionSettings>
	{
	protected:
		TArray<TSharedPtr<FDiffusion>> OngoingDiffusions; // Ongoing diffusions
		TArray<TSharedPtr<FDiffusion>> Diffusions; // Stopped diffusions, as to not iterate over them needlessly

		TSharedPtr<TArray<int8>> Visited; // Whether that node has been visited and captured already

		TSharedPtr<PCGExMT::TScopedValue<double>> MaxDistanceValue;

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		PCGEX_FOREACH_FIELD_CLUSTER_DIFF(PCGEX_OUTPUT_DECL)

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		PCGEX_FOREACH_FIELD_CLUSTER_DIFF(PCGEX_OUTPUT_DECL)

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges);
		virtual ~FBatch() override;

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual bool PrepareSingle(const TSharedPtr<FProcessor>& ClusterProcessor) override;
		virtual void Write() override;
	};
}

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"
#include "PCGExPaths.h"

#include "PCGExPointsProcessor.h"
#include "Data/Blending/PCGExUnionBlender.h"
#include "Data/Blending/PCGExDataBlending.h"


#include "SubPoints/DataBlending/PCGExSubPointsBlendOperation.h"
#include "PCGExPathStitch.generated.h"

UENUM()
enum class EPCGExStitchMethod : uint8
{
	Connect = 0 UMETA(DisplayName = "Connect", ToolTip="Connect existing point with a segment (preserve all input points)"),
	Merge   = 1 UMETA(DisplayName = "Merge", ToolTip="Merge points that should be connected, only leaving a single one."),
};

UENUM()
enum class EPCGExStitchMergeMethod : uint8
{
	KeepStart = 0 UMETA(DisplayName = "Keep Start", ToolTip="Keep start point during the merge"),
	KeepEnd   = 1 UMETA(DisplayName = "Keep End", ToolTip="Keep end point during the merge"),
};

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/crossings"))
class UPCGExPathStitchSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathStitch, "Path Stitch", "Stitch paths together by their endpoints.");
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	// TODO : Stitch only start with ends
	// TODO : Sort collections for stitching priorities
	// TODO : Prioritize least amount of candidates first
	// Work like overlap check : first process all possibilities and then resolve them until there is none left
	// - Process
	// - 

public:
	/** Choose how paths are connected. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExStitchMethod Method = EPCGExStitchMethod::Connect;

	/** Choose how paths are connected. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, DisplayName=" ├─ Keep ", EditCondition="Method == EPCGExStitchMethod::Merge", EditConditionHides))
	EPCGExStitchMergeMethod MergeMethod = EPCGExStitchMergeMethod::KeepStart;

	/**  */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Average ", EditCondition="Method == EPCGExStitchMethod::Merge", EditConditionHides))
	bool bAverageMergedPoints = false;

	/** If enabled, stitching will only happen between a path's end point and another path start point. Otherwise, it's based on spatial proximity alone. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bOnlyMatchStartAndEnds = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bDoRequireAlignment = false;

	/** If enabled, foreign segments must be aligned within a given angular threshold. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Requires Alignment", EditCondition="bDoRequireAlignment"))
	FPCGExDotComparisonDetails DotComparisonDetails;
};

struct FPCGExPathStitchContext final : FPCGExPathProcessorContext
{
	friend class FPCGExPathStitchElement;

	UPCGExSubPointsBlendInstancedFactory* Blending = nullptr;

	TSharedPtr<PCGExDetails::FDistances> Distances;
	FPCGExBlendingDetails CrossingBlending;
};

class FPCGExPathStitchElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(PathStitch)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExPathStitch
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExPathStitchContext, UPCGExPathStitchSettings>
	{
		TSharedPtr<PCGExPaths::FPath> Path;
		TSharedPtr<PCGExPaths::FPathEdgeLength> PathLength;

		TArray<TSharedPtr<PCGExPaths::FPathEdgeCrossings>> EdgeCrossings;

		TSharedPtr<PCGExPointFilter::FManager> CanCutFilterManager;
		TSharedPtr<PCGExPointFilter::FManager> CanBeCutFilterManager;

		TBitArray<> CanCut;
		TBitArray<> CanBeCut;

		TSet<FName> ProtectedAttributes;
		TSharedPtr<FPCGExSubPointsBlendOperation> SubBlending;

		TSet<int32> CrossIOIndices;
		TSharedPtr<PCGExDataBlending::IUnionBlender> UnionBlender;

		FPCGExPathEdgeIntersectionDetails Details;

		TSharedPtr<PCGExData::TBuffer<bool>> FlagWriter;
		TSharedPtr<PCGExData::TBuffer<double>> AlphaWriter;
		TSharedPtr<PCGExData::TBuffer<FVector>> CrossWriter;
		TSharedPtr<PCGExData::TBuffer<bool>> IsPointCrossingWriter;

		int32 FoundCrossingsNum = 0;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TPointsProcessor(InPointDataFacade)
		{
		}

		virtual bool IsTrivial() const override { return false; } // Force non-trivial because this shit is expensive

		const PCGExPaths::FPathEdgeOctree* GetEdgeOctree() const { return Path->GetEdgeOctree(); }

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void CompleteWork() override;
		virtual void ProcessRange(const PCGExMT::FScope& Scope) override;
		virtual void OnRangeProcessingComplete() override;

		void CollapseCrossings(const PCGExMT::FScope& Scope);
		void CrossBlend(const PCGExMT::FScope& Scope);

		virtual void Write() override;
	};
}

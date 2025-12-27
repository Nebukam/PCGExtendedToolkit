// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Factories/PCGExFactories.h"
#include "Core/PCGExPathProcessor.h"
#include "Core/PCGExCCShapeOffset.h"
#include "Details/PCGExCCDetails.h"
#include "Details/PCGExInputShorthandsDetails.h"
#include "Details/PCGExSettingsMacros.h"
#include "Paths/PCGExPath.h"

#include "PCGExCavalierParallelOffset.generated.h"

namespace PCGExCavalier
{
	class FPolyline;
}

/**
 * Applies parallel offset to a set of polylines that form a shape (outer boundaries with holes).
 * Unlike the regular Offset node, this handles interactions between multiple polylines.
 */
UCLASS(Hidden, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/cavalier-contours/cavalier-parallel-offset"))
class UPCGExCavalierParallelOffsetSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(CavalierParallelOffset, "Cavalier : Parallel Offset (Shape)", "Applies parallel offset to a shape composed of multiple polylines (supports holes).");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	//~End UPCGExPointsProcessorSettings

	/** Projection settings for 2D operations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionDetails ProjectionDetails;

	/** If enabled, also generates offsets in the opposite direction (dual offset). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExInputShorthandNameBoolean DualOffset = FPCGExInputShorthandNameBoolean(FName("@Data.DualOffset"), true, false);

	/** The offset distance. Positive values offset outward (for CCW polylines) or inward (for CW/holes). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExInputShorthandNameDouble Offset = FPCGExInputShorthandNameDouble(FName("@Data.Offset"), 10, false);

	/** Offset options controlling algorithm behavior. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExCCOffsetOptions OffsetOptions;

	/** Number of offset iterations to perform. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExInputShorthandNameInteger32Abs Iterations = FPCGExInputShorthandNameInteger32Abs(FName("@Data.Iterations"), 1, false);

	/** If enabled, tessellate arcs in the output. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bTessellateArcs = true;

	/** Arc tessellation settings. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bTessellateArcs"))
	FPCGExCCArcTessellationSettings TessellationSettings;

	/** If enabled, blend transforms from source paths for output vertices. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bBlendTransforms = true;

	/** Add small random offset to mitigate degenerate geometry issues. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bAddFuzzinessToPositions = false;

	/** Skip open paths (shape offset requires closed paths). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bSkipOpenPaths = true;

	/** If enabled, write the iteration index to a data attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bWriteIteration = false;

	/** Attribute name to write the iteration index to. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bWriteIteration"))
	FString IterationAttributeName = TEXT("@Data.Iteration");

	/** If enabled, tag outputs with the iteration number. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagIteration = false;

	/** Tag format for iteration number. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagIteration"))
	FString IterationTag = TEXT("OffsetNum");

	/** If enabled, tag dual offset outputs. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagDual = false;

	/** Tag to apply to dual offset outputs. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagDual"))
	FString DualTag = TEXT("Dual");

	/** If enabled, tag outputs based on their orientation (outer vs hole). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagOrientation = false;

	/** Tag to apply to outer (CCW) boundaries. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagOrientation"))
	FString OuterTag = TEXT("Outer");

	/** Tag to apply to hole (CW) boundaries. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagOrientation"))
	FString HoleTag = TEXT("Hole");
};

struct FPCGExCavalierParallelOffsetContext final : FPCGExPathProcessorContext
{
	friend class FPCGExCavalierParallelOffsetElement;

	// Source data for 3D reconstruction
	TMap<int32, PCGExCavalier::FRootPath> RootPaths;

	// Input polylines grouped as a shape
	TArray<PCGExCavalier::FPolyline> InputPolylines;

	// Path IDs for input polylines
	TArray<int32> InputPathIds;

	// Maps polyline path ID to its source FPointIO
	TMap<int32, TSharedPtr<PCGExData::FPointIO>> SourceIOs;

	FPCGExGeo2DProjectionDetails ProjectionDetails;

	int32 NextPathId = 0;
	FCriticalSection PathIdLock;

	int32 AllocatePathId()
	{
		FScopeLock Lock(&PathIdLock);
		return NextPathId++;
	}
};

class FPCGExCavalierParallelOffsetElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(CavalierParallelOffset)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;

private:
	// Build polylines from input collection
	void BuildPolylinesFromCollection(
		FPCGExCavalierParallelOffsetContext* Context,
		const UPCGExCavalierParallelOffsetSettings* Settings) const;

	// Output a result polyline
	TSharedPtr<PCGExData::FPointIO> OutputPolyline(
		FPCGExCavalierParallelOffsetContext* Context,
		const UPCGExCavalierParallelOffsetSettings* Settings,
		PCGExCavalier::FPolyline& Polyline) const;

	// Process and tag output
	void ProcessOutput(
		FPCGExCavalierParallelOffsetContext* Context,
		const UPCGExCavalierParallelOffsetSettings* Settings,
		const TSharedPtr<PCGExData::FPointIO>& IO,
		int32 Iteration,
		bool bIsDual,
		bool bIsHole) const;

	// Find source IO for a polyline
	TSharedPtr<PCGExData::FPointIO> FindSourceIO(
		FPCGExCavalierParallelOffsetContext* Context,
		const PCGExCavalier::FPolyline& Polyline) const;
};
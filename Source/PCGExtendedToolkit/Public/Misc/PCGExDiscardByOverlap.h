// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "PCGExPointsToBounds.h"
#include "PCGExSortPoints.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/Blending/PCGExMetadataBlender.h"

#include "PCGExDiscardByOverlap.generated.h"

UENUM(BlueprintType)
enum class EPCGExOverlapTestMode : uint8
{
	Fast UMETA(DisplayName = "Fast", ToolTip="Only test using datasets' overall bounds"),
	Precise UMETA(DisplayName = "Precise", ToolTip="Test every points' bounds"),
};

UENUM(BlueprintType)
enum class EPCGExOverlapPruningOrder : uint8
{
	OverlapCount UMETA(DisplayName = "Overlap Count", ToolTip="Removes high overlap count first"),
	OverlapAmount UMETA(DisplayName = "Overlap Amount", ToolTip="Removes larger overlap area first"),
};

namespace PCGExDiscardByOverlap
{
	constexpr PCGExMT::AsyncState State_InitialOverlap = __COUNTER__;
	constexpr PCGExMT::AsyncState State_PreciseOverlap = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessFastOverlap = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessPreciseOverlap = __COUNTER__;
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExDiscardByOverlapSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExDiscardByOverlapSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DiscardByOverlap, "Points to Bounds", "Merge points group to a single point representing their bounds.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Overlap overlap test mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOverlapTestMode TestMode = EPCGExOverlapTestMode::Fast;

	/** Point bounds to be used to compute overlaps */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledExtents;

	/** Pruning order & prioritization */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOverlapPruningOrder PruningOrder = EPCGExOverlapPruningOrder::OverlapCount;

	/** Pruning order & prioritization */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSortDirection Order = EPCGExSortDirection::Ascending;

private:
	friend class FPCGExDiscardByOverlapElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExDiscardByOverlapContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExDiscardByOverlapElement;
	virtual ~FPCGExDiscardByOverlapContext() override;

	TArray<PCGExPointsToBounds::FBounds*> IOBounds;
};

class PCGEXTENDEDTOOLKIT_API FPCGExDiscardByOverlapElement : public FPCGExPointsProcessorElementBase
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExComputePreciseOverlap : public FPCGExNonAbandonableTask
{
public:
	FPCGExComputePreciseOverlap(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
	                            EPCGExPointBoundsSource InBoundsSource, PCGExPointsToBounds::FBounds* InBounds) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		BoundsSource(InBoundsSource), Bounds(InBounds)
	{
	}

	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledExtents;
	PCGExPointsToBounds::FBounds* Bounds = nullptr;

	virtual bool ExecuteTask() override;
};

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
	OverlapCount UMETA(DisplayName = "Overlap Count", ToolTip="Dataset overlap count"),
	OverlapAmount UMETA(DisplayName = "Overlap Amount", ToolTip="Volume overlap"),
};

UENUM(BlueprintType)
enum class EPCGExExpandPointsBoundsMode : uint8
{
	None UMETA(DisplayName = "Don't expand", ToolTip="Use vanilla point bounds"),
	Static UMETA(DisplayName = "Static value", ToolTip="Expand point bounds by a static value"),
	Attribute UMETA(DisplayName = "Local attribute", ToolTip="Expand point bounds by a local attribute"),
};

namespace PCGExDiscardByOverlap
{
	constexpr PCGExMT::AsyncState State_InitialOverlap = __COUNTER__;
	constexpr PCGExMT::AsyncState State_PreciseOverlap = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessFastOverlap = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessPreciseOverlap = __COUNTER__;

	static void SortOverlapCount(TArray<PCGExPointsToBounds::FBounds*>& IOBounds,
	                          const EPCGExSortDirection Order)
	{
		IOBounds.Sort(
			[&](const PCGExPointsToBounds::FBounds& A, const PCGExPointsToBounds::FBounds& B)
			{
				return Order == EPCGExSortDirection::Ascending ?
					       A.Overlaps.Num() < B.Overlaps.Num() :
					       A.Overlaps.Num() < B.Overlaps.Num();
			});
	}

	static void SortFastAmount(TArray<PCGExPointsToBounds::FBounds*>& IOBounds,
	                           const EPCGExSortDirection Order)
	{
		IOBounds.Sort(
			[&](const PCGExPointsToBounds::FBounds& A, const PCGExPointsToBounds::FBounds& B)
			{
				return Order == EPCGExSortDirection::Ascending ?
					       A.FastOverlapAmount < B.FastOverlapAmount :
					       A.FastOverlapAmount < B.FastOverlapAmount;
			});
	}

	static void SortPreciseCount(TArray<PCGExPointsToBounds::FBounds*>& IOBounds,
	                             const EPCGExSortDirection Order)
	{
		IOBounds.Sort(
			[&](const PCGExPointsToBounds::FBounds& A, const PCGExPointsToBounds::FBounds& B)
			{
				return Order == EPCGExSortDirection::Ascending ?
						   A.TotalPreciseOverlapCount < B.TotalPreciseOverlapCount :
						   A.TotalPreciseOverlapCount < B.TotalPreciseOverlapCount;
			});
	}

	static void SortPreciseAmount(TArray<PCGExPointsToBounds::FBounds*>& IOBounds, const EPCGExSortDirection Order)
	{
		IOBounds.Sort(
			[&](const PCGExPointsToBounds::FBounds& A, const PCGExPointsToBounds::FBounds& B)
			{
				return Order == EPCGExSortDirection::Ascending ?
					       A.TotalPreciseOverlapAmount < B.TotalPreciseOverlapAmount :
					       A.TotalPreciseOverlapAmount < B.TotalPreciseOverlapAmount;
			});
	}
	

}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExDiscardByOverlapSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExDiscardByOverlapSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DiscardByOverlap, "Discard By Overlap", "Discard entire datasets based on how they overlap with each other.");
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

	/** Static value to be used as bound expansion */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="TestMode==EPCGExOverlapTestMode::Precise", EditConditionHides))
	bool bUsePerPointsValues = false;
	
	/** Pruning order & prioritization */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSortDirection Order = EPCGExSortDirection::Ascending;

	/** Expand local point bounds when doing precise testing */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExExpandPointsBoundsMode ExpansionMode = EPCGExExpandPointsBoundsMode::None;

	/** Static value to be used as bound expansion */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="ExpansionMode==EPCGExExpandPointsBoundsMode::Static", EditConditionHides))
	double ExpansionValue = 10;

	/** Local point value to be used as bound expansion */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="ExpansionMode==EPCGExExpandPointsBoundsMode::Attribute", EditConditionHides))
	FPCGExInputDescriptor ExpansionLocalValue;

private:
	friend class FPCGExDiscardByOverlapElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExDiscardByOverlapContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExDiscardByOverlapElement;
	virtual ~FPCGExDiscardByOverlapContext() override;

	TArray<PCGExPointsToBounds::FBounds*> IOBounds;

	void OutputFBounds(const PCGExPointsToBounds::FBounds* Bounds, const int32 RemoveAt = -1);
	static void RemoveFBounds(const PCGExPointsToBounds::FBounds* Bounds, TArray<PCGExPointsToBounds::FBounds*>& OutAffectedBounds);
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

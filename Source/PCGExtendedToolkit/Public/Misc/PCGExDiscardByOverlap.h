// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "PCGExPointsToBounds.h"
#include "PCGExSortPoints.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "PCGExtendedToolkit/Public/Transform/PCGExTransform.h"

#include "PCGExDiscardByOverlap.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Overlap Test Mode"))
enum class EPCGExOverlapTestMode : uint8
{
	Fast UMETA(DisplayName = "Fast", ToolTip="Only test using datasets' overall bounds"),
	Precise UMETA(DisplayName = "Precise", ToolTip="Test every points' bounds"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Overlap Pruning Order"))
enum class EPCGExOverlapPruningOrder : uint8
{
	OverlapCount UMETA(DisplayName = "Count > Amount", ToolTip="Overlap count"),
	OverlapAmount UMETA(DisplayName = "Amount > Count", ToolTip="Overlap amount"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Expand Points Bounds Mode"))
enum class EPCGExExpandPointsBoundsMode : uint8
{
	None UMETA(DisplayName = "Don't expand", ToolTip="Use vanilla point bounds"),
	Static UMETA(DisplayName = "Static value", ToolTip="Expand point bounds by a static value"),
	Attribute UMETA(DisplayName = "Local attribute", ToolTip="Expand point bounds by a local attribute"),
};

namespace PCGExDiscardByOverlap
{
	PCGEX_ASYNC_STATE(State_InitialOverlap)
	PCGEX_ASYNC_STATE(State_PreciseOverlap)
	PCGEX_ASYNC_STATE(State_ProcessFastOverlap)
	PCGEX_ASYNC_STATE(State_ProcessPreciseOverlap)
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExDiscardByOverlapSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DiscardByOverlap, "Discard By Overlap", "Discard entire datasets based on how they overlap with each other.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscRemove; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UObject interface
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** Overlap overlap test mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOverlapTestMode TestMode = EPCGExOverlapTestMode::Fast;

	/** Point bounds to be used to compute overlaps */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledBounds;

	/** Pruning order & prioritization */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOverlapPruningOrder PruningOrder = EPCGExOverlapPruningOrder::OverlapCount;

	/** Static value to be used as bound expansion */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=1))
	double AmountFMod = 10;

	/** Static value to be used as bound expansion */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="TestMode==EPCGExOverlapTestMode::Precise", EditConditionHides))
	bool bUsePerPointsValues = false;

	/** Pruning order & prioritization */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExSortDirection Order = EPCGExSortDirection::Ascending;

	/** Expand local point bounds when doing precise testing */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExExpandPointsBoundsMode ExpansionMode = EPCGExExpandPointsBoundsMode::None;

	/** Static value to be used as bound expansion */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="ExpansionMode==EPCGExExpandPointsBoundsMode::Static", EditConditionHides))
	double ExpansionValue = 10;

	/** Local point value to be used as bound expansion */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="ExpansionMode==EPCGExExpandPointsBoundsMode::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector ExpansionLocalValue;

private:
	friend class FPCGExDiscardByOverlapElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExDiscardByOverlapContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExDiscardByOverlapElement;
	virtual ~FPCGExDiscardByOverlapContext() override;

	TArray<PCGExPointsToBounds::FBounds*> IOBounds;

	void OutputFBounds(const PCGExPointsToBounds::FBounds* Bounds, const int32 RemoveAt = -1);
	static void RemoveFBounds(const PCGExPointsToBounds::FBounds* Bounds, TArray<PCGExPointsToBounds::FBounds*>& OutAffectedBounds);
};

class PCGEXTENDEDTOOLKIT_API FPCGExDiscardByOverlapElement final : public FPCGExPointsProcessorElement
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExComputePreciseOverlap final : public PCGExMT::FPCGExTask
{
public:
	FPCGExComputePreciseOverlap(PCGExData::FPointIO* InPointIO,
	                            const EPCGExPointBoundsSource InBoundsSource, PCGExPointsToBounds::FBounds* InBounds) :
		FPCGExTask(InPointIO),
		BoundsSource(InBoundsSource), Bounds(InBounds)
	{
	}

	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledBounds;
	PCGExPointsToBounds::FBounds* Bounds = nullptr;

	virtual bool ExecuteTask() override;
};

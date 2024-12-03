// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExFilterGroup.h"
#include "PCGExDiscardSame.generated.h"

UENUM()
enum class EPCGExDiscardSameMode : uint8
{
	FIFO = 0 UMETA(DisplayName = "FIFO", ToolTip="First in, first out"),
	LIFO = 1 UMETA(DisplayName = "LIFO", ToolTip="Last in, first out"),
	All  = 2 UMETA(DisplayName = "All", ToolTip="Discard all collections that have found duplicates (does not keep any)"),
};

namespace PCGExDiscardSame
{
	class FProcessor;
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExDiscardSameSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DiscardSame, "Discard Same", "Discard entire datasets based on a selection of parameters");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscRemove; }
#endif

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExDiscardSameMode Mode = EPCGExDiscardSameMode::FIFO;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFilterGroupMode TestMode = EPCGExFilterGroupMode::AND;


	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTestBounds = false;

	/** Test collection bounds equality, within tolerance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bTestBounds", ClampMin = 0))
	double TestBoundsTolerance = 0.1;


	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTestPointCount = true;

	/** Test collection point count equality, within tolerance */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bTestPointCount", ClampMin = 0))
	int32 TestPointCountTolerance = 0;

	
	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTestPositions = true;

	/** Test point positions equality, within tolerance. Note that it computes space occupation, and does not account for point count. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bTestPositions", ClampMin = 0))
	double TestPositionTolerance = 0.1;

private:
	friend class FPCGExDiscardSameElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDiscardSameContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExDiscardSameElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDiscardSameElement final : public FPCGExPointsProcessorElement
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExDiscardSame
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExDiscardSameContext, UPCGExDiscardSameSettings>
	{
		friend struct FPCGExDiscardSameContext;

	public:
		double HashPointsCount = 0;
		uint32 HashBounds = 0;
		uint32 HashPositions = 0;
		
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TPointsProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void CompleteWork() override;
	};
}

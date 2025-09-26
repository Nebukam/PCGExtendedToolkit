﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHasher.h"
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

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="filters/discard-same"))
class UPCGExDiscardSameSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(DiscardSame, "Discard Same", "Discard entire datasets based on a selection of parameters");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorMiscRemove; }
#endif

protected:
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
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

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTestAttributeHash = false;

	/** Build a hash from a single attribute and test it against the others. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bTestAttributeHash"))
	FPCGExAttributeHashConfig AttributeHashConfig;

private:
	friend class FPCGExDiscardSameElement;
};

struct FPCGExDiscardSameContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExDiscardSameElement;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExDiscardSameElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(DiscardSame)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExDiscardSame
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExDiscardSameContext, UPCGExDiscardSameSettings>
	{
		friend struct FPCGExDiscardSameContext;

		TSharedPtr<PCGEx::FAttributeHasher> Hasher;

	public:
		double HashPointsCount = 0;
		uint32 HashBounds = 0;
		uint32 HashPositions = 0;

		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void CompleteWork() override;
	};
}

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Clipper2Lib/clipper.export.h"
#include "Core/PCGExPathProcessor.h"
#include "Details/PCGExMatchingDetails.h"
#include "Math/PCGExProjectionDetails.h"
#include "PCGExClipper2Processor.generated.h"

namespace PCGExClipper2
{
	class FOpData : public TSharedFromThis<FOpData>
	{
	public:
		TSharedPtr<TArray<TSharedPtr<PCGExData::FFacade>>> Facades;
		TArray<PCGExClipper2Lib::Path64> Paths;
		TArray<bool> IsClosedLoop;
		TArray<FPCGExGeo2DProjectionDetails> Projections;

		explicit FOpData(const int32 InReserve);
		void AddReserve(const int32 InReserve);

		int32 Num() const { return Facades->Num(); }
	};
}

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Cavalier")
class UPCGExClipper2ProcessorSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExClipper2ProcessorSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(Path); }
#endif
	//~End UPCGSettings

protected:
	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

public:
	/** If enabled, lets you to create sub-groups to operate on. If disabled, data is processed individually. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExMatchingDetails MainDataMatching = FPCGExMatchingDetails(EPCGExMatchingDetailsUsage::Default);

	/** If enabled, lets you to pick which are matched with which main data.
	 * Note that the match is done against every single data within a group and then consolidated;
	 * this means if you have a [A,B,C] group, ABC will be executed against operands for A, B and C individually. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="NeedsOperands()", EditConditionHides))
	FPCGExMatchingDetails OperandsDataMatching = FPCGExMatchingDetails(EPCGExMatchingDetailsUsage::Default);

	/** Skip paths that aren't closed */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable), AdvancedDisplay)
	bool bSkipOpenPaths = false;

	/** Decimal precision 
	 * Clipper2 Uses int64 under the hood to preserve extreme precision, so we scale floating point values then back. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=1), AdvancedDisplay)
	int32 Precision = 10;

	/** Cleanup */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable), AdvancedDisplay)
	bool bSimplifyPaths = true;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagHoles = false;

	/** Write this tag on paths that are holes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagHoles"))
	FString HoleTag = TEXT("Hole");

	virtual bool NeedsOperands() const;
	virtual FPCGExGeo2DProjectionDetails GetProjectionDetails() const;
};

struct FPCGExClipper2ProcessorContext : FPCGExPathProcessorContext
{
	friend class FPCGExClipper2ProcessorElement;

	std::atomic<int32> NextSourceId = 0;

	int32 AllocateSourceIdx() { return NextSourceId.fetch_add(1); }

	TSharedPtr<PCGExData::FPointIOCollection> OperandsCollection;

	TSharedPtr<PCGExClipper2::FOpData> AllOpData;

	TArray<TArray<int32>> MainOpDataPartitions;
	TArray<TArray<int32>> OperandsOpDataPartitions;

	FPCGExGeo2DProjectionDetails ProjectionDetails;

	void OutputPaths64(PCGExClipper2Lib::Paths64& InPaths, TArray<TSharedPtr<PCGExData::FPointIO>>& OutPaths) const;

	virtual void Process(const TArray<int32>& Subjects, const TArray<int32>* Operands = nullptr);
	
};

class FPCGExClipper2ProcessorElement : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(Clipper2Processor)
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;

	int32 BuildDataFromCollection(
		FPCGExClipper2ProcessorContext* Context,
		const UPCGExClipper2ProcessorSettings* Settings,
		const TSharedPtr<PCGExData::FPointIOCollection>& Collection,
		TArray<TArray<int32>>& OutData) const;
};

namespace PCGExClipper2
{
}

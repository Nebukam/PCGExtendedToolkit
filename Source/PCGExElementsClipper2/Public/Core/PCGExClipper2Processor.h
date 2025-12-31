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
	/** If enabled, allows you to filter out which targets get sampled by which data */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FPCGExMatchingDetails DataMatching = FPCGExMatchingDetails(EPCGExMatchingDetailsUsage::Default);
	
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
	
};

class FPCGExClipper2ProcessorElement : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(Clipper2Processor)
	virtual bool Boot(FPCGExContext* InContext) const override;
	
	virtual bool WantsDataFromMainInput() const;

	int32 BuildDataFromCollection(
		FPCGExClipper2ProcessorContext* Context,
		const UPCGExClipper2ProcessorSettings* Settings,
		const TSharedPtr<PCGExData::FPointIOCollection>& Collection,
		TArray<TArray<int32>>& OutData) const;
};

namespace PCGExClipper2
{
	
}

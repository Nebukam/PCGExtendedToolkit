// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Clipper2Lib/clipper.export.h"
#include "Core/PCGExPathProcessor.h"
#include "Math/PCGExProjectionDetails.h"
#include "PCGExClipper2Processor.generated.h"


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
	/** Skip paths that aren't closed */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable), AdvancedDisplay)
	bool bSkipOpenPaths = false;

	/** Decimal precision 
	 * Clipper2 Uses int64 under the hood to preserve extreme precision, so we scale floating point values then back. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=1), AdvancedDisplay)
	int32 Precision = 10;


	virtual bool NeedsOperands() const;
	virtual FPCGExGeo2DProjectionDetails GetProjectionDetails() const;
};

struct FPCGExClipper2ProcessorContext : FPCGExPathProcessorContext
{
	friend class FPCGExClipper2ProcessorElement;

	std::atomic<int32> NextSourceId = 0;

	int32 AllocateSourceIdx() { return NextSourceId.fetch_add(1); }

	TSharedPtr<PCGExData::FPointIOCollection> OperandsCollection;

	TArray<TSharedPtr<PCGExData::FFacade>> AllSources;
	
	PCGExClipper2Lib::CPaths64 MainPaths64;
	TArray<TSharedPtr<PCGExData::FFacade>> MainSources;
	
	PCGExClipper2Lib::CPaths64 MainOperands64;
	TArray<TSharedPtr<PCGExData::FFacade>> OperandsSources;

	FPCGExGeo2DProjectionDetails ProjectionDetails;
};

class FPCGExClipper2ProcessorElement : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(Clipper2Processor)
	virtual bool Boot(FPCGExContext* InContext) const override;

	virtual bool WantsRootPathsFromMainInput() const;

	void BuildRootPathsFromCollection(
		FPCGExClipper2ProcessorContext* Context,
		const UPCGExClipper2ProcessorSettings* Settings,
		const TSharedPtr<PCGExData::FPointIOCollection>& Collection,
		PCGExClipper2Lib::CPaths64& OutPaths,
		TArray<TSharedPtr<PCGExData::FFacade>>& OutSourceFacades) const;
};

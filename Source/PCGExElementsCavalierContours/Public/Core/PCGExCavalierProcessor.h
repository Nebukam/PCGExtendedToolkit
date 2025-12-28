// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCCPolyline.h"
#include "PCGExCCTypes.h"
#include "Core/PCGExPathProcessor.h"
#include "Details/PCGExCCDetails.h"
#include "Math/PCGExProjectionDetails.h"
#include "PCGExCavalierProcessor.generated.h"


/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Cavalier")
class PCGEXELEMENTSCAVALIERCONTOURS_API UPCGExCavalierProcessorSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExCavalierProcessorSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(Path); }
#endif
	//~End UPCGSettings

protected:
	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;

public:
	/** Tessellate arcs in results */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bTessellateArcs = true;

	/** Arc tessellation settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bTessellateArcs"))
	FPCGExCCArcTessellationSettings ArcTessellationSettings;


	/** If enabled, output negative space (holes) as separate paths */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable))
	bool bOutputNegativeSpace = true;

	/** Tag to apply to negative space outputs */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable, EditCondition="bOutputNegativeSpace"))
	FString NegativeSpaceTag = TEXT("Hole");


	/** Skip paths that aren't closed */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable), AdvancedDisplay)
	bool bSkipOpenPaths = false;

	/** Add small random offset to mitigate degenerate geometry issues */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable), AdvancedDisplay)
	bool bAddFuzzinessToPositions = false;

	/** Blend transforms from source paths for output vertices */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable), AdvancedDisplay)
	bool bBlendTransforms = true;


	virtual bool NeedsOperands() const;
	virtual FPCGExGeo2DProjectionDetails GetProjectionDetails() const;
};

struct PCGEXELEMENTSCAVALIERCONTOURS_API FPCGExCavalierProcessorContext : FPCGExPathProcessorContext
{
	friend class FPCGExCavalierProcessorElement;

	int32 NextSourceId = 0;
	FCriticalSection SourceIdLock;

	int32 AllocateSourceIdx()
	{
		// TODO : Make this atomic, lock is dumb here
		FScopeLock Lock(&SourceIdLock);
		return NextSourceId++;
	}

	TSharedPtr<PCGExData::FPointIOCollection> OperandsCollection;

	// Source data for 3D reconstruction
	TMap<int32, PCGExCavalier::FRootPath> RootPathsMap;

	// Polylines built from main input
	TArray<PCGExCavalier::FPolyline> MainPolylines;

	// Polylines built from operands input
	TArray<PCGExCavalier::FPolyline> OperandPolylines;

	FPCGExGeo2DProjectionDetails ProjectionDetails;
	FPCGExCCArcTessellationSettings ArcTessellationSettings;

	// Cached facades
	TArray<TSharedPtr<PCGExData::FFacade>> MainFacades;
	TArray<TSharedPtr<PCGExData::FFacade>> OperandsFacades;

	TSharedPtr<PCGExData::FFacade> FindSourceFacade(
		const PCGExCavalier::FPolyline& Polyline,
		const TMap<int32, PCGExCavalier::FRootPath>* RootPathsMapOverride = nullptr) const;

	// Output a result polyline
	TSharedPtr<PCGExData::FPointIO> OutputPolyline(
		PCGExCavalier::FPolyline& Polyline, bool bIsNegativeSpace,
		const FPCGExGeo2DProjectionDetails& InProjectionDetails,
		const TMap<int32, PCGExCavalier::FRootPath>* RootPathsMapOverride = nullptr) const;
};

class PCGEXELEMENTSCAVALIERCONTOURS_API FPCGExCavalierProcessorElement : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(CavalierProcessor)
	virtual bool Boot(FPCGExContext* InContext) const override;

	virtual bool WantsRootPathsFromMainInput() const;

	void BuildRootPathsFromCollection(
		FPCGExCavalierProcessorContext* Context,
		const UPCGExCavalierProcessorSettings* Settings,
		const TSharedPtr<PCGExData::FPointIOCollection>& Collection,
		TArray<PCGExCavalier::FPolyline>& OutPolylines,
		TArray<TSharedPtr<PCGExData::FFacade>>& OutSourceFacades) const;
};

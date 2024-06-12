// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExEditorSettings.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExPathProcessor.generated.h"

class UPCGExNodeStateFactory;

namespace PCGExCluster
{
	class FNodeStateHandler;
}

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Subdivide Mode"))
enum class EPCGExSubdivideMode : uint8
{
	Distance UMETA(DisplayName = "Distance", ToolTip="Number of subdivisions depends on segment' length"),
	Count UMETA(DisplayName = "Count", ToolTip="Number of subdivisions is static"),
};

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExPathProcessorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPathProcessorSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExEditorSettings>()->NodeColorPath; }
#endif

	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings interface


	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual FName GetMainInputLabel() const override;
	virtual FName GetMainOutputLabel() const override;
	//~End UPCGExPointsProcessorSettings interface

	virtual FName GetPointFilterLabel() const;
	bool SupportsPointFilters() const;
	virtual bool RequiresPointFilters() const;
		
};

struct PCGEXTENDEDTOOLKIT_API FPCGExPathProcessorContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExPathProcessorElement;

	~FPCGExPathProcessorContext();

	virtual bool ProcessorAutomation() override;

	virtual bool AdvancePointsIO() override;

	bool ProcessFilters();

	virtual bool DefaultPointFilterResult() const;
	virtual bool PrepareFiltersWithAdvance() const;

	TArray<UPCGExFilterFactoryBase*> FilterFactories;

	PCGExData::FPointIOCollection* MainPaths = nullptr;

	PCGExDataFilter::TEarlyExitFilterManager* CreatePointFilterManagerInstance(PCGExData::FPointIO* PointIO) const;

	PCGExDataFilter::TEarlyExitFilterManager* PointFiltersManager = nullptr;	
	bool bRequirePointFilterPreparation = false;
	bool bWaitingOnFilterWork = false;
};

class PCGEXTENDEDTOOLKIT_API FPCGExPathProcessorElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
};

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"
#include "PCGExPaths.h"
#include "PCGExPointsProcessor.h"

#include "PCGExPathProcessor.generated.h"

class UPCGExFilterFactoryBase;

class UPCGExNodeStateFactory;

namespace PCGExCluster
{
	class FNodeStateHandler;
}

/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPathProcessorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPathProcessorSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorPath; }
#endif
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual FName GetMainInputPin() const override { return PCGExGraph::SourcePathsLabel; }
	virtual FName GetMainOutputPin() const override { return PCGExGraph::OutputPathsLabel; }
	virtual FString GetPointFilterTooltip() const override { return TEXT("Path points processing filters"); }

	//~End UPCGExPointsProcessorSettings

	UPROPERTY()
	bool bSupportClosedLoops = true;

	/** Closed loop handling.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayPriority=-1, EditCondition="bSupportClosedLoops", EditConditionHides, HideEditConditionToggle))
	FPCGExPathClosedLoopDetails ClosedLoop;

	/** If enabled, collections that have less than 2 points won't be processed and be omitted from the output. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable), AdvancedDisplay)
	bool bOmitInvalidPathsOutputs = true;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathProcessorContext : FPCGExPointsProcessorContext
{
	friend class FPCGExPathProcessorElement;

	FPCGExPathClosedLoopDetails ClosedLoop;
	TSharedPtr<PCGExData::FPointIOCollection> MainPaths;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathProcessorElement : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
};

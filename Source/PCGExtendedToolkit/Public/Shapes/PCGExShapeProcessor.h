// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"
#include "PCGExShapes.h"
#include "PCGExPointsProcessor.h"
#include "PCGExShapeBuilderFactoryProvider.h"
#include "Data/PCGExDataForward.h"

#include "PCGExShapeProcessor.generated.h"

class UPCGExFilterFactoryBase;
/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExShapeProcessorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExShapeProcessorSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorPath; }
#endif
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual FName GetMainInputLabel() const override { return PCGExGraph::SourceSeedsLabel; }
	virtual FString GetPointFilterTooltip() const override { return TEXT("Path points processing filters"); }

	//~End UPCGExPointsProcessorSettings
	
	/** How to pick which path endpoint to start with */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExShapeOutputMode OutputMode = EPCGExShapeOutputMode::PerSeed;

};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExShapeProcessorContext : FPCGExPointsProcessorContext
{
	friend class FPCGExShapeProcessorElement;
	TArray<TObjectPtr<const UPCGExShapeBuilderFactoryBase>> BuilderFactories;
	
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExShapeProcessorElement : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
};

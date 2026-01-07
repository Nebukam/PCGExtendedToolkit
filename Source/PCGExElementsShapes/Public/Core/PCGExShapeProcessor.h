// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExShapesCommon.h"
#include "PCGExShapeBuilderFactoryProvider.h"
#include "Core/PCGExPointsProcessor.h"
#include "PCGExShapeProcessor.generated.h"

class UPCGExPointFilterFactoryData;
/**
 * 
 */
UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXELEMENTSSHAPES_API UPCGExShapeProcessorSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExShapeProcessorSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_NAME(Path); }
#endif
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FName GetMainInputPin() const override;
	virtual FString GetPointFilterTooltip() const override { return TEXT("Path points processing filters"); }

	//~End UPCGExPointsProcessorSettings

	/** How to pick which path endpoint to start with */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExShapeOutputMode OutputMode = EPCGExShapeOutputMode::PerSeed;
};

struct PCGEXELEMENTSSHAPES_API FPCGExShapeProcessorContext : FPCGExPointsProcessorContext
{
	friend class FPCGExShapeProcessorElement;
	TArray<TObjectPtr<const UPCGExShapeBuilderFactoryData>> BuilderFactories;
};

class PCGEXELEMENTSSHAPES_API FPCGExShapeProcessorElement : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ShapeProcessor)

	virtual bool Boot(FPCGExContext* InContext) const override;
};

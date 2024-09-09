// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "PCGExDetails.h"
#include "PCGExBlendPath.generated.h"

class UPCGExSubPointsBlendOperation;

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Path Split Action"))
enum class EPCGExPathBlendMode : uint8
{
	Full   = 0 UMETA(DisplayName = "Start to End", ToolTip="Blend properties & attributes of all path' points from start point to last point"),
	Switch = 1 UMETA(DisplayName = "Switch", ToolTip="Switch between pruning/non-pruning based on filters"),
};


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExBlendPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BlendPath, "Path : Blend", "Blend path individual points between its start and end points.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UObject interface
public:
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings

public:
	/** Consider paths to be closed -- processing will wrap between first and last points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedPath = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault))
	TObjectPtr<UPCGExSubPointsBlendOperation> Blending;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBlendPathContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExBlendPathElement;

	virtual ~FPCGExBlendPathContext() override;

	EPCGExSubdivideMode SubdivideMethod;
	UPCGExSubPointsBlendOperation* Blending = nullptr;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBlendPathElement final : public FPCGExPathProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExBlendPathTask final : public PCGExMT::FPCGExTask
{
public:
	FPCGExBlendPathTask(PCGExData::FPointIO* InPointIO) :
		FPCGExTask(InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};

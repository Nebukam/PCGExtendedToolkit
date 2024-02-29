// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Geometry/PCGExGeo.h"
#include "PCGExOffsetPath.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExOffsetPathSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(OffsetPath, "Path : Offset", "Offset paths points.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UObject interface
public:
	virtual void PostInitProperties() override;
#if WITH_EDITOR

public:
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Consider paths to be closed -- processing will wrap between first and last points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedPath = false;

	/** Offset size.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	double Offset = 1.0;

	/** Fetch the size from a local attribute. The regular Size parameter then act as a scale.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalOffset = false;

	/** Fetch the size from a local attribute. The regular Size parameter then act as a scale.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bUseLocalOffset"))
	FPCGAttributePropertyInputSelector LocalOffset;

	/** Up vector used to calculate Offset direction.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FVector UpVector = FVector::UpVector;

	/** Fetch the Up vector from a local point attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalUpVector = false;

	/** Fetch the Up vector from a local point attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bUseLocalOffset"))
	FPCGAttributePropertyInputSelector LocalUpVector;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExOffsetPathContext : public FPCGExPathProcessorContext
{
	friend class FPCGExOffsetPathElement;

	virtual ~FPCGExOffsetPathContext() override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExOffsetPathElement : public FPCGExPathProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExOffsetPathTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExOffsetPathTask(FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};

﻿// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Splines/SubPoints/PCGExSubPointsProcessor.h"
#include "PCGExSubdivide.generated.h"

UENUM(BlueprintType)
enum class EPCGExSubdivideMode : uint8
{
	Distance UMETA(DisplayName = "Distance", ToolTip="Number of subdivisions depends on segment' length"),
	Count UMETA(DisplayName = "Count", ToolTip="Number of subdivisions is static"),
};

UENUM(BlueprintType)
enum class EPCGExSubdivideBlendMode : uint8
{
	InheritStart UMETA(DisplayName = "Inherit Start Point", ToolTip="Subdivided points inherit the segment' starting attributes"),
	InheritEnd UMETA(DisplayName = "Inherit End Point", ToolTip="Subdivided points inherit the segment' starting attributes"),
	Lerp UMETA(DisplayName = "Lerp", ToolTip="Subdivided points inherit the segment' starting attributes"),
};

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExSubdivideSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExSubdivideSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Subdivide, "Subdivide", "Subdivide paths segments.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorSpline; }
#endif

	virtual PCGExPointIO::EInit GetPointOutputInitMode() const override;
	virtual FName GetMainPointsInputLabel() const override;
	virtual FName GetMainPointsOutputLabel() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExSubdivideMode Method = EPCGExSubdivideMode::Distance;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Method==EPCGExSubdivideMode::Distance", EditConditionHides))
	double Distance = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="Method==EPCGExSubdivideMode::Count", EditConditionHides))
	int32 Count = 10;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, Instanced)
	UPCGExSubPointsProcessor* Blending;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(InlineEditConditionToggle))
	bool bFlagSubPoints = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bFlagSubPoints"))
	FName FlagName = "IsSubPoint";
};

struct PCGEXTENDEDTOOLKIT_API FPCGExSubdivideContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExSubdivideElement;

public:
	EPCGExSubdivideMode Method;
	double Distance;
	int32 Count;
	bool bFlagSubPoints;

	FName FlagName;
	FPCGMetadataAttribute<bool>* FlagAttribute = nullptr;

	UPCGExSubPointsProcessor* SubPointsProcessor;
};

class PCGEXTENDEDTOOLKIT_API FPCGExSubdivideElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};
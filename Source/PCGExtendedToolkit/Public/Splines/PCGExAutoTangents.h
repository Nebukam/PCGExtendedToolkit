// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "PCGExAutoTangents.generated.h"

UENUM(BlueprintType)
enum class EPCGExAutoTangentScaleMode : uint8
{
	Scale UMETA(DisplayName = "Auto", ToolTip="Smooth tangents"),
};

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExAutoTangentsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(AutoTangents, "Auto Tangents", "Computes & writes points tangents.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorSpline; }
#endif

	virtual PCGExIO::EInitMode GetPointOutputInitMode() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName ArriveName = "ArriveTangent";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName LeaveName = "LeaveTangent";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExAutoTangentScaleMode ScaleMode = EPCGExAutoTangentScaleMode::Scale;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	double Scale = 1;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExAutoTangentsContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExAutoTangentsElement;

public:
	FName ArriveName;
	FName LeaveName;
	FPCGMetadataAttribute<FVector>* ArriveAttribute = nullptr;
	FPCGMetadataAttribute<FVector>* LeaveAttribute = nullptr;
	EPCGExAutoTangentScaleMode ScaleMode;
	double Scale = 1;
};

class PCGEXTENDEDTOOLKIT_API FPCGExAutoTangentsElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

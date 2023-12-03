// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Tangents/PCGExTangents.h"
#include "PCGExWriteTangents.generated.h"

namespace PCGExTangents
{
	struct PCGEXTENDEDTOOLKIT_API FPair
	{
		FVector Leave;
		FVector Arrive;
	};
}

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExWriteTangentsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExWriteTangentsSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(WriteTangents, "Write Tangents", "Computes & writes points tangents.");
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEx::NodeColorSpline; }
#endif

	virtual PCGExPointIO::EInit GetPointOutputInitMode() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName ArriveName = "ArriveTangent";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName LeaveName = "LeaveTangent";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, Instanced)
	UPCGExTangents* Tangents;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExWriteTangentsContext : public FPCGExPointsProcessorContext
{
	friend class FPCGExWriteTangentsElement;

public:
	UPCGExTangents* Tangents;
};

class PCGEXTENDEDTOOLKIT_API FPCGExWriteTangentsElement : public FPCGExPointsProcessorElementBase
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Graph/PCGExGraph.h"
#include "Tangents/PCGExTangentsOperation.h"
#include "PCGExWriteTangents.generated.h"

/**
 * Calculates the distance between two points (inherently a n*n operation)
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExWriteTangentsSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExWriteTangentsSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(WriteTangents, "Write Tangents", "Computes & writes points tangents.");
#endif

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName ArriveName = "ArriveTangent";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName LeaveName = "LeaveTangent";

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable))
	UPCGExTangentsOperation* Tangents;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExWriteTangentsContext : public FPCGExPathProcessorContext
{
	friend class FPCGExWriteTangentsElement;

	virtual ~FPCGExWriteTangentsContext() override;

	PCGEx::FAttributeAccessor<FVector>* ArriveTangentsAccessor = nullptr;
	PCGEx::FAttributeAccessor<FVector>* LeaveTangentsAccessor = nullptr;

	TArray<FVector> ArriveTangents;
	TArray<FVector> LeaveTangents;

	UPCGExTangentsOperation* Tangents = nullptr;

	void WriteTangents();
};

class PCGEXTENDEDTOOLKIT_API FPCGExWriteTangentsElement : public FPCGExPathProcessorElement
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

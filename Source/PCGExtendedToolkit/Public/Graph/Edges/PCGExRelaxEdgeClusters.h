// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Relaxing/PCGExForceDirectedRelaxing.h"
#include "PCGExRelaxEdgeClusters.generated.h"

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExRelaxEdgeClustersSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExRelaxEdgeClustersSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(RelaxEdgeClusters, "Edges : Relax", "Relax point positions using edges connecting them.");
#endif

	virtual PCGExData::EInit GetMainOutputInitMode() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=1))
	int32 Iterations = 100;

	/** Draw size. What it means depends on the selected debug type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=-1, ClampMax=1))
	double Influence = 1.0;

	/** Fetch the size from a local attribute. The regular Size parameter then act as a scale.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bUseLocalInfluence = false;

	/** Fetch the size from a local attribute. The regular Size parameter then act as a scale.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bUseLocalInfluence"))
	FPCGExInputDescriptorWithSingleField LocalInfluence;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, NoResetToDefault, ShowOnlyInnerProperties))
	TObjectPtr<UPCGExEdgeRelaxingOperation> Relaxing;

private:
	friend class FPCGExRelaxEdgeClustersElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExRelaxEdgeClustersContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExRelaxEdgeClustersElement;

	virtual ~FPCGExRelaxEdgeClustersContext() override;

	int32 Iterations = 10;
	int32 CurrentIteration = 0;
	bool bUseLocalInfluence = false;
	PCGEx::FLocalSingleFieldGetter InfluenceGetter;

	TArray<FVector> PrimaryBuffer;
	TArray<FVector> SecondaryBuffer;

	UPCGExEdgeRelaxingOperation* Relaxing = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExRelaxEdgeClustersElement : public FPCGExEdgesProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

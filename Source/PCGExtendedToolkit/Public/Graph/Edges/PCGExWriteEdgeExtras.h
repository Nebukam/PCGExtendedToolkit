// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Sampling/PCGExSampling.h"
#include "PCGExWriteEdgeExtras.generated.h"

#define PCGEX_FOREACH_FIELD_EDGEEXTRAS(MACRO)\
MACRO(EdgeLength, double)

namespace PCGExDataBlending
{
	class FMetadataBlender;
	struct FPropertiesBlender;
}

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Edges")
class PCGEXTENDEDTOOLKIT_API UPCGExWriteEdgeExtrasSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExWriteEdgeExtrasSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(WriteEdgeExtras, "Edges : Write Extras", "Extract & write extra edge informations to the point representing the edge.");
#endif

	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	virtual PCGExData::EInit GetEdgeOutputInitMode() const override;

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

public:
	/** Write normal from edges on vertices. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteVtxNormal = false;

	/** Name of the 'normal' vertex attribute to write normal to.*/
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteVtxNormal"))
	FName VtxNormalAttributeName = FName("Normal");

	/** Write whether the sampling was sucessful or not to a boolean attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteEdgeLength = false;

	/** Name of the 'boolean' attribute to write sampling success to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteEdgeLength"))
	FName EdgeLengthAttributeName = FName("SuccessfullySampled");

	/** Edges will inherit point attributes -- NOT IMPLEMENTED*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bBlendAttributes = false;

	/** Defines how fused point properties and attributes are merged together. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(EditCondition="bBlendAttributes"))
	FPCGExBlendingSettings BlendingSettings = FPCGExBlendingSettings(EPCGExDataBlendingType::Average);

private:
	friend class FPCGExWriteEdgeExtrasElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExWriteEdgeExtrasContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExWriteEdgeExtrasElement;

	virtual ~FPCGExWriteEdgeExtrasContext() override;

	PCGExDataBlending::FMetadataBlender* MetadataBlender;

	PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_DECL)

	PCGEx::TFAttributeWriter<FVector>* VtxNormalWriter = nullptr;
};

class PCGEXTENDEDTOOLKIT_API FPCGExWriteEdgeExtrasElement : public FPCGExEdgesProcessorElement
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

// Define the background task class
class PCGEXTENDEDTOOLKIT_API FWriteExtrasTask : public FPCGExNonAbandonableTask
{
public:
	FWriteExtrasTask(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO)
	{
	}

	virtual bool ExecuteTask() override;
};

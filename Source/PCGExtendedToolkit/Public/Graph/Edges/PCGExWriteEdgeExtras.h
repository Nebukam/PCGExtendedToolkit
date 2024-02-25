// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Sampling/PCGExSampling.h"
#include "PCGExWriteEdgeExtras.generated.h"

#define PCGEX_FOREACH_FIELD_EDGEEXTRAS(MACRO)\
MACRO(EdgeLength, double)\
MACRO(EdgeDirection, FVector)

namespace PCGExDataBlending
{
	class FMetadataBlender;
	struct FPropertiesBlender;
}

UENUM(BlueprintType)
enum class EPCGExEdgeDirectionMethod : uint8
{
	EndpointsOrder UMETA(DisplayName = "Endpoints Order", ToolTip="Uses the edge' Start & End properties"),
	EndpointsIndices UMETA(DisplayName = "Endpoints Indices", ToolTip="Uses the edge' Start & End indices"),
	EndpointsAttribute UMETA(DisplayName = "Endpoints Attribute", ToolTip="Uses a single-component property or attribute value on Start & End points"),
	EdgeDotAttribute UMETA(DisplayName = "Edge Dot Attribute", ToolTip="Chooses the highest dot product against a vector property or attribute on the edge point"),
};

UENUM(BlueprintType)
enum class EPCGExEdgeDirectionChoice : uint8
{
	SmallestToGreatest UMETA(DisplayName = "Smallest to Greatest", ToolTip="Direction points from smallest to greatest value"),
	GreatestToSmallest UMETA(DisplayName = "Greatest to Smallest", ToolTip="Direction points from the greatest to smallest value")
};

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteVtxNormal = false;

	/** Name of the 'normal' vertex attribute to write normal to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteVtxNormal"))
	FName VtxNormalAttributeName = FName("Normal");

	/** Method to pick the edge direction amongst various possibilities.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExEdgeDirectionMethod DirectionMethod = EPCGExEdgeDirectionMethod::EndpointsOrder;

	/** Further refine the direction method. Not all methods make use of this property.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExEdgeDirectionChoice DirectionChoice = EPCGExEdgeDirectionChoice::SmallestToGreatest;

	/** Attribute picker for the selected Direction Method.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="DirectionMethod==EPCGExEdgeDirectionMethod::EndpointsAttribute", EditConditionHides))
	FPCGExInputDescriptor VtxSourceAttribute;

	/** Attribute picker for the selected Direction Method.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="DirectionMethod==EPCGExEdgeDirectionMethod::EdgeDotAttribute", EditConditionHides))
	FPCGExInputDescriptor EdgeSourceAttribute;

	/** Output Edge Length. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteEdgeLength = false;

	/** Name of the 'boolean' attribute to write sampling success to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteEdgeLength"))
	FName EdgeLengthAttributeName = FName("EdgeLength");

	/** Output Edge Direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteEdgeDirection = false;

	/** Name of the 'boolean' attribute to write sampling success to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteEdgeDirection"))
	FName EdgeDirectionAttributeName = FName("EdgeDirection");


	/** Update Edge position as a lerp between endpoints (according to the direction method selected above) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteEdgePosition = false;

	/** Name of the 'boolean' attribute to write sampling success to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bWriteEdgePosition"))
	double EdgePositionLerp = 0.5;

	/** Edges will inherit point attributes -- NOT IMPLEMENTED*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta = (PCG_Overridable))
	bool bEndpointsBlending = false;

	/** Balance between start/end point*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bEndpointsBlend", ClampMin=0, ClampMax=1))
	double EndpointsBlending = 0.5;

	/** Defines how fused point properties and attributes are merged together. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(EditCondition="bBlendAttributes"))
	FPCGExBlendingSettings BlendingSettings = FPCGExBlendingSettings(EPCGExDataBlendingType::Average);

	/** Projection settings used for normal calculations. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExGeo2DProjectionSettings ProjectionSettings;

private:
	friend class FPCGExWriteEdgeExtrasElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExWriteEdgeExtrasContext : public FPCGExEdgesProcessorContext
{
	friend class FPCGExWriteEdgeExtrasElement;

	virtual ~FPCGExWriteEdgeExtrasContext() override;

	FPCGExGeo2DProjectionSettings ProjectionSettings;
	
	PCGExDataBlending::FMetadataBlender* MetadataBlender;

	PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_DECL)
	bool bWriteEdgePosition;
	double EdgePositionLerp;

	EPCGExEdgeDirectionMethod DirectionMethod;
	EPCGExEdgeDirectionChoice DirectionChoice;

	bool bEndpointsBlending;
	double EndpointsBlending;

	PCGEx::FLocalSingleFieldGetter* VtxDirCompGetter = nullptr;
	PCGEx::FLocalVectorGetter* EdgeDirCompGetter = nullptr;

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


class PCGEXTENDEDTOOLKIT_API FPCGExWriteExtrasTask : public FPCGExNonAbandonableTask
{
public:
	FPCGExWriteExtrasTask(
		FPCGExAsyncManager* InManager, const int32 InTaskIndex, PCGExData::FPointIO* InPointIO,
		FPCGExGeo2DProjectionSettings* InProjectionSettings) :
		FPCGExNonAbandonableTask(InManager, InTaskIndex, InPointIO),
		ProjectionSettings(InProjectionSettings)
	{
	}

	virtual bool ExecuteTask() override;

	FPCGExGeo2DProjectionSettings* ProjectionSettings = nullptr;
};

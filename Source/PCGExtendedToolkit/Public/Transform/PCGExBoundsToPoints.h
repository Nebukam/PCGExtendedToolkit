// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "PCGExtendedToolkit/Public/Transform/PCGExTransform.h"

#include "PCGExBoundsToPoints.generated.h"

class FPCGExComputeIOBounds;

UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc")
class PCGEXTENDEDTOOLKIT_API UPCGExBoundsToPointsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings interface
#if WITH_EDITOR
	PCGEX_NODE_INFOS(BoundsToPoints, "Bounds To Points", "Generate a point on the surface of the bounds.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorTransform; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Generates a point collections per generated point */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bGeneratePerPointData = false;

	/** Generate points in symmetry */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExMinimalAxis SymmetryAxis = EPCGExMinimalAxis::None;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExUVW UVW;

	/** Use a Seed attribute value to tag output data. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bGenerateNewData"))
	bool bUsePointAttributeToTagNewData;

	/** Which Seed attribute to use as tag. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging & Forwarding", meta=(EditCondition="bGenerateNewData&&bUsePointAttributeToTagNewData"))
	FPCGAttributePropertyInputSelector TagAttribute;

private:
	friend class FPCGExBoundsToPointsElement;
};

struct PCGEXTENDEDTOOLKIT_API FPCGExBoundsToPointsContext final : public FPCGExPointsProcessorContext
{
	friend class FPCGExBoundsToPointsElement;

	virtual ~FPCGExBoundsToPointsContext() override;
};

class PCGEXTENDEDTOOLKIT_API FPCGExBoundsToPointsElement final : public FPCGExPointsProcessorElement
{
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExBoundsToPoints
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		int32 NumPoints = 0;
		bool bGeneratePerPointData = false;
		bool bSymmetry = false;
		EPCGExMinimalAxis Axis = EPCGExMinimalAxis::None;

		FPCGExUVW UVW;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		TArray<PCGExData::FPointIO*> NewOutputs;

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount) override;
		virtual void CompleteWork() override;
	};
}

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Data/PCGExAttributeHelpers.h"
#include "Data/Blending/PCGExMetadataBlender.h"

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
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorMiscAdd; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings interface

	//~Begin UPCGExPointsProcessorSettings interface
public:
	virtual PCGExData::EInit GetMainOutputInitMode() const override;
	//~End UPCGExPointsProcessorSettings interface

public:
	/** Overlap overlap test mode */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsSource = EPCGExPointBoundsSource::ScaledExtents;

	/** Generates a point collections per generated point */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bGenerateNewData = false;

	/** Generate points in symmetry */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bSymmetry = false;

	/** Which axis to sample.\n W = depth on that axis, U & V are the two other axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExMinimalAxis Axis = EPCGExMinimalAxis::X;

	/** U Source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExFetchType USource = EPCGExFetchType::Constant;

	/** U Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="USource==EPCGExFetchType::Constant", DisplayName="U"))
	double UConstant = 0.5;

	/** U Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="USource==EPCGExFetchType::Attribute", DisplayName="U"))
	FPCGAttributePropertyInputSelector UAttribute;

	/** V Source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExFetchType VSource = EPCGExFetchType::Constant;

	/** V Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="VSource==EPCGExFetchType::Constant", DisplayName="V"))
	double VConstant = 0.5;

	/** V Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="USource==EPCGExFetchType::Attribute", DisplayName="V"))
	FPCGAttributePropertyInputSelector VAttribute;

	/** W Source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExFetchType WSource = EPCGExFetchType::Constant;

	/** W Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="WSource==EPCGExFetchType::Constant", DisplayName="W"))
	double WConstant = 0.5;

	/** W Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="USource==EPCGExFetchType::Attribute", DisplayName="W"))
	FPCGAttributePropertyInputSelector WAttribute;


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
	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void CompleteWork() override;
	};
}

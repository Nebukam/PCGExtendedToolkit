// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"
#include "Orient/PCGExOrientOperation.h"
#include "PCGExOrient.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Transform Component Selector"))
enum class EPCGExOrientUsage : uint8
{
	ApplyToPoint UMETA(DisplayName = "Apply to point", ToolTip="Applies the orientation transform to the point"),
	OutputToAttribute UMETA(DisplayName = "Output to attribute", ToolTip="Output the orientation transform to an attribute"),
};

namespace PCGExOrient
{
	static double DotProduct(const PCGEx::FPointRef& CurrentPt, const PCGEx::FPointRef& PreviousPt, const PCGEx::FPointRef& NextPt)
	{
		const FVector Mid = CurrentPt.Point->Transform.GetLocation();
		return FVector::DotProduct((PreviousPt.Point->Transform.GetLocation() - Mid).GetSafeNormal(), (Mid - NextPt.Point->Transform.GetLocation()).GetSafeNormal());
	}
}

/**
 * 
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class PCGEXTENDEDTOOLKIT_API UPCGExOrientSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(Orient, "Path : Orient", "Orient paths points");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointProcessorSettings
public:
	virtual FName GetPointFilterLabel() const override;
	//~End UPCGExPointProcessorSettings

public:
	/** Consider paths to be closed -- processing will wrap between first and last points. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedPath = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExAxis OrientAxis = EPCGExAxis::Forward;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExAxis UpAxis = EPCGExAxis::Up;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault))
	TObjectPtr<UPCGExOrientOperation> Orientation;

	/** Default value, can be overriden per-point through filters. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bFlipDirection = false;

	/**  */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOrientUsage Output = EPCGExOrientUsage::ApplyToPoint;

	/**  */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Output==EPCGExOrientUsage::OutputToAttribute", EditConditionHides))
	FName OutputAttribute = "Orient";

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputDot = false;

	/** Whether to output the dot product between prev/next points.  */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bOutputDot"))
	FName DotAttribute = "Dot";
};

struct PCGEXTENDEDTOOLKIT_API FPCGExOrientContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExOrientElement;

public:
	UPCGExOrientOperation* Orientation;
};

class PCGEXTENDEDTOOLKIT_API FPCGExOrientElement final : public FPCGExPathProcessorElement
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

namespace PCGExOrient
{
	class FProcessor final : public PCGExPointsMT::FPointsProcessor
	{
		PCGEx::TFAttributeWriter<FTransform>* TransformWriter = nullptr;
		PCGEx::TFAttributeWriter<double>* DotWriter = nullptr;
		UPCGExOrientOperation* Orient = nullptr;
		int32 LastIndex = 0;

	public:
		explicit FProcessor(PCGExData::FPointIO* InPoints):
			FPointsProcessor(InPoints)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(PCGExMT::FTaskManager* AsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}

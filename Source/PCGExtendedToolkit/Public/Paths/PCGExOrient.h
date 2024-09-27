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
	ApplyToPoint      = 0 UMETA(DisplayName = "Apply to point", ToolTip="Applies the orientation transform to the point"),
	OutputToAttribute = 1 UMETA(DisplayName = "Output to attribute", ToolTip="Output the orientation transform to an attribute"),
};

namespace PCGExOrient
{
	static double DotProduct(const PCGExData::FPointRef& CurrentPt, const PCGExData::FPointRef& PreviousPt, const PCGExData::FPointRef& NextPt)
	{
		const FVector Mid = CurrentPt.Point->Transform.GetLocation();
		return FVector::DotProduct((PreviousPt.Point->Transform.GetLocation() - Mid).GetSafeNormal(), (Mid - NextPt.Point->Transform.GetLocation()).GetSafeNormal());
	}
}

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExOrientSettings : public UPCGExPathProcessorSettings
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
	PCGEX_NODE_POINT_FILTER(FName("Flip Orientation Conditions"), "Filters used to know whether an orientation should be flipped or not", PCGExFactories::PointFilters, false)
	//~End UPCGExPointProcessorSettings

public:
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

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExOrientContext final : public FPCGExPathProcessorContext
{
	friend class FPCGExOrientElement;

public:
	UPCGExOrientOperation* Orientation;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExOrientElement final : public FPCGExPathProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExOrient
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExOrientContext, UPCGExOrientSettings>
	{
		TSharedPtr<PCGExData::TBuffer<FTransform>> TransformWriter;
		TSharedPtr<PCGExData::TBuffer<double>> DotWriter;
		UPCGExOrientOperation* Orient = nullptr;
		int32 LastIndex = 0;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count) override;
		virtual void CompleteWork() override;
	};
}

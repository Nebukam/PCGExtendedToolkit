﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"

#include "PCGExPointsProcessor.h"


#include "Orient/PCGExOrientOperation.h"
#include "PCGExOrient.generated.h"

UENUM()
enum class EPCGExOrientUsage : uint8
{
	ApplyToPoint      = 0 UMETA(DisplayName = "Apply to point", ToolTip="Applies the orientation transform to the point"),
	OutputToAttribute = 1 UMETA(DisplayName = "Output to attribute", ToolTip="Output the orientation transform to an attribute"),
};

namespace PCGExOrient
{
	static double DotProduct(const PCGExData::FConstPoint& CurrentPt, const PCGExData::FConstPoint& PreviousPt, const PCGExData::FConstPoint& NextPt)
	{
		const FVector Mid = CurrentPt.GetTransform().GetLocation();
		return FVector::DotProduct((PreviousPt.GetTransform().GetLocation() - Mid).GetSafeNormal(), (Mid - NextPt.GetTransform().GetLocation()).GetSafeNormal());
	}
}

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/orient"))
class UPCGExOrientSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		PathOrient, "Path : Orient", "Orient paths points",
		(Orientation ? FName(Orientation.GetClass()->GetMetaData(TEXT("DisplayName"))) : FName("...")));
#endif

#if WITH_EDITORONLY_DATA
	// UObject interface
	virtual void PostInitProperties() override;
	// End of UObject interface
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointProcessorSettings
public:
	PCGEX_NODE_POINT_FILTER(FName("Flip Conditions"), "Filters used to know whether an orientation should be flipped or not", PCGExFactories::PointFilters, false)
	//~End UPCGExPointProcessorSettings

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExAxis OrientAxis = EPCGExAxis::Forward;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExAxis UpAxis = EPCGExAxis::Up;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, Instanced, meta=(PCG_Overridable, ShowOnlyInnerProperties, NoResetToDefault))
	TObjectPtr<UPCGExOrientInstancedFactory> Orientation;

	/** Default value, can be overriden per-point through filters. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bFlipDirection = false;

	/**  */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExOrientUsage Output = EPCGExOrientUsage::ApplyToPoint;

	/**  */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Output == EPCGExOrientUsage::OutputToAttribute", EditConditionHides))
	FName OutputAttribute = "Orient";

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOutputDot = false;

	/** Whether to output the dot product between prev/next points.  */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bOutputDot"))
	FName DotAttribute = "Dot";
};

struct FPCGExOrientContext final : FPCGExPathProcessorContext
{
	friend class FPCGExOrientElement;

	UPCGExOrientInstancedFactory* Orientation;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExOrientElement final : public FPCGExPathProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(Orient)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExOrient
{
	const FName SourceOverridesOrient = TEXT("Overrides : Orient");

	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExOrientContext, UPCGExOrientSettings>
	{
		TSharedPtr<PCGExPaths::FPath> Path;

		TSharedPtr<PCGExData::TBuffer<FTransform>> TransformWriter;
		TSharedPtr<PCGExData::TBuffer<double>> DotWriter;
		TSharedPtr<FPCGExOrientOperation> Orient;
		int32 LastIndex = 0;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}

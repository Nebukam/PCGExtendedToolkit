// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExData.h"

#include "PCGExFitting.generated.h"


UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Transform Component Selector"))
enum class EPCGExFitMode : uint8
{
	None UMETA(DisplayName = "None", ToolTip="No fitting"),
	Uniform UMETA(DisplayName = "Uniform", ToolTip="Uniform fit"),
	Individual UMETA(DisplayName = "Individual", ToolTip="Per-component fit"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Scale to fit"))
enum class EPCGExScaleToFit : uint8
{
	None UMETA(DisplayName = "None", ToolTip="No fitting"),
	Fill UMETA(DisplayName = "Fill", ToolTip="..."),
	Min UMETA(DisplayName = "Min", ToolTip="..."),
	Max UMETA(DisplayName = "Max", ToolTip="..."),
	Avg UMETA(DisplayName = "Average", ToolTip="..."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Justify From"))
enum class EPCGExJustifyFrom : uint8
{
	Min UMETA(DisplayName = "Min", ToolTip="..."),
	Center UMETA(DisplayName = "Center", ToolTip="..."),
	Max UMETA(DisplayName = "Max", ToolTip="..."),
	Custom UMETA(DisplayName = "Custom", ToolTip="..."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Justify To"))
enum class EPCGExJustifyTo : uint8
{
	Same UMETA(DisplayName = "Same", ToolTip="..."),
	Min UMETA(DisplayName = "Min", ToolTip="..."),
	Center UMETA(DisplayName = "Center", ToolTip="..."),
	Max UMETA(DisplayName = "Max", ToolTip="..."),
	Custom UMETA(DisplayName = "Custom", ToolTip="..."),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExScaleToFitDetails
{
	GENERATED_BODY()

	FPCGExScaleToFitDetails()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFitMode ScaleToFitMode = EPCGExFitMode::Individual;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="ScaleToFitMode==EPCGExFitMode::Uniform", EditConditionHides))
	EPCGExScaleToFit ScaleToFit = EPCGExScaleToFit::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="ScaleToFitMode==EPCGExFitMode::Individual", EditConditionHides))
	EPCGExScaleToFit ScaleToFitX = EPCGExScaleToFit::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="ScaleToFitMode==EPCGExFitMode::Individual", EditConditionHides))
	EPCGExScaleToFit ScaleToFitY = EPCGExScaleToFit::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="ScaleToFitMode==EPCGExFitMode::Individual", EditConditionHides))
	EPCGExScaleToFit ScaleToFitZ = EPCGExScaleToFit::None;

	void Process(
		const FPCGPoint& InPoint,
		const FBox& InBounds,
		FVector& OutScale,
		FBox& OutBounds) const
	{
		if (ScaleToFitMode == EPCGExFitMode::None) { return; }

		const FVector PtSize = InPoint.GetLocalBounds().GetSize();
		const FVector ScaledPtSize = InPoint.GetLocalBounds().GetSize() * InPoint.Transform.GetScale3D();
		const FVector StSize = InBounds.GetSize();

		const double XFactor = ScaledPtSize.X / StSize.X;
		const double YFactor = ScaledPtSize.Y / StSize.Y;
		const double ZFactor = ScaledPtSize.Z / StSize.Z;

		const FVector FitMinMax = FVector(
			FMath::Min3(XFactor, YFactor, ZFactor),
			FMath::Max3(XFactor, YFactor, ZFactor),
			(XFactor + YFactor + ZFactor) / 3);

		OutBounds.Min = InBounds.Min;
		OutBounds.Max = InBounds.Max;

		const FVector InScale = InPoint.Transform.GetScale3D();

		if (ScaleToFitMode == EPCGExFitMode::Uniform)
		{
			ScaleToFitAxis(ScaleToFit, 0, InScale, PtSize, StSize, FitMinMax, OutScale);
			ScaleToFitAxis(ScaleToFit, 1, InScale, PtSize, StSize, FitMinMax, OutScale);
			ScaleToFitAxis(ScaleToFit, 2, InScale, PtSize, StSize, FitMinMax, OutScale);
		}
		else
		{
			ScaleToFitAxis(ScaleToFitX, 0, InScale, PtSize, StSize, FitMinMax, OutScale);
			ScaleToFitAxis(ScaleToFitY, 1, InScale, PtSize, StSize, FitMinMax, OutScale);
			ScaleToFitAxis(ScaleToFitZ, 2, InScale, PtSize, StSize, FitMinMax, OutScale);
		}
	}

private:
	static void ScaleToFitAxis(
		const EPCGExScaleToFit Fit,
		const int32 Axis,
		const FVector& InScale,
		const FVector& InPtSize,
		const FVector& InStSize,
		const FVector& MinMaxFit,
		FVector& OutScale)
	{
		const double Scale = InScale[Axis];
		double FinalScale = Scale;

		switch (Fit)
		{
		default:
		case EPCGExScaleToFit::None:
			break;
		case EPCGExScaleToFit::Fill:
			FinalScale = ((InPtSize[Axis] * Scale) / InStSize[Axis]);
			break;
		case EPCGExScaleToFit::Min:
			FinalScale = MinMaxFit[0];
			break;
		case EPCGExScaleToFit::Max:
			FinalScale = MinMaxFit[1];
			break;
		case EPCGExScaleToFit::Avg:
			FinalScale = MinMaxFit[2];
			break;
		}

		OutScale[Axis] = FinalScale;
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExSingleJustifyDetails
{
	GENERATED_BODY()

	FPCGExSingleJustifyDetails()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExJustifyFrom From = EPCGExJustifyFrom::Center;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="From==EPCGExJustifyFrom::Custom", EditConditionHides))
	EPCGExFetchType FromType = EPCGExFetchType::Constant;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="From==EPCGExJustifyFrom::Custom && FromType==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector FromSourceAttribute;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="From==EPCGExJustifyFrom::Custom && FromType==EPCGExFetchType::Constant", EditConditionHides))
	double FromConstant = 0.5;

	PCGExData::FCache<double>* FromGetter = nullptr;


	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExJustifyTo To = EPCGExJustifyTo::Same;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="To==EPCGExJustifyTo::Custom", EditConditionHides))
	EPCGExFetchType ToType = EPCGExFetchType::Constant;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="To==EPCGExJustifyTo::Custom && ToType==EPCGExFetchType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector ToSourceAttribute;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="To==EPCGExJustifyTo::Custom && ToType==EPCGExFetchType::Constant", EditConditionHides))
	double ToConstant = 0.5;

	PCGExData::FCache<double>* ToGetter = nullptr;

	bool Init(FPCGExContext* InContext, PCGExData::FFacade* InDataFacade)
	{
		if (From == EPCGExJustifyFrom::Custom && FromType == EPCGExFetchType::Attribute)
		{
			FromGetter = InDataFacade->GetScopedBroadcaster<double>(FromSourceAttribute);
			if (!FromGetter)
			{
				// TODO : Throw
				return false;
			}
		}

		if (To == EPCGExJustifyTo::Same)
		{
			switch (From)
			{
			default:
			case EPCGExJustifyFrom::Min:
				To = EPCGExJustifyTo::Min;
				break;
			case EPCGExJustifyFrom::Center:
				To = EPCGExJustifyTo::Center;
				break;
			case EPCGExJustifyFrom::Max:
				To = EPCGExJustifyTo::Max;
				break;
			case EPCGExJustifyFrom::Custom:
				break;
			}
		}

		if (To == EPCGExJustifyTo::Custom && FromType == EPCGExFetchType::Attribute)
		{
			ToGetter = InDataFacade->GetScopedBroadcaster<double>(FromSourceAttribute);
			if (!ToGetter)
			{
				// TODO : Throw
				return false;
			}
		}

		return true;
	}

	void JustifyAxis(
		const int32 Axis,
		const int32 Index,
		const FVector& InCenter,
		const FVector& InSize,
		const FVector& OutCenter,
		const FVector& OutSize,
		FVector& OutTranslation) const
	{
		double Start = 0;
		double End = 0;

		switch (From)
		{
		default:
		case EPCGExJustifyFrom::Min:
			Start = OutCenter[Axis] - OutSize[Axis] * 0.5;
			break;
		case EPCGExJustifyFrom::Center:
			Start = OutCenter[Axis];
			break;
		case EPCGExJustifyFrom::Max:
			Start = OutCenter[Axis] + (OutSize[Axis] * 0.5);
			break;
		case EPCGExJustifyFrom::Custom:
			Start = OutCenter[Axis] - (OutSize[Axis] * 0.5) + (OutSize[Axis] * (FromGetter ? FromGetter->Values[Index] : FromConstant));
			break;
		}

		switch (To)
		{
		default:
		case EPCGExJustifyTo::Min:
			End = InCenter[Axis] - (InSize[Axis] * 0.5);
			break;
		case EPCGExJustifyTo::Center:
			End = InCenter[Axis];
			break;
		case EPCGExJustifyTo::Max:
			End = InCenter[Axis] + (InSize[Axis] * 0.5);
			break;
		case EPCGExJustifyTo::Custom:
			End = InCenter[Axis] - (InSize[Axis] * 0.5) + (InSize[Axis] * (ToGetter ? ToGetter->Values[Index] : ToConstant));
			break;
		case EPCGExJustifyTo::Same:
			// == Custom but using From values
			End = InCenter[Axis] - (InSize[Axis] * 0.5) * (InSize[Axis] * (FromGetter ? FromGetter->Values[Index] : FromConstant));
			break;
		}

		OutTranslation[Axis] = End - Start ;
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExJustificationDetails
{
	GENERATED_BODY()

	FPCGExJustificationDetails()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bJustifyX = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bJustifyX"))
	FPCGExSingleJustifyDetails JustifyX;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bJustifyY = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bJustifyY"))
	FPCGExSingleJustifyDetails JustifyY;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bJustifyZ = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bJustifyZ"))
	FPCGExSingleJustifyDetails JustifyZ;

	void Process(
		const int32 Index,
		const FBox& InBounds,
		const FBox& OutBounds,
		FVector& OutTranslation) const
	{
		const FVector InCenter = InBounds.GetCenter();
		const FVector InSize = InBounds.GetSize();
		
		const FVector OutCenter = OutBounds.GetCenter();
		const FVector OutSize = OutBounds.GetSize();

		if (bJustifyX) { JustifyX.JustifyAxis(0, Index, InCenter, InSize, OutCenter, OutSize, OutTranslation); }
		if (bJustifyY) { JustifyY.JustifyAxis(1, Index, InCenter, InSize, OutCenter, OutSize, OutTranslation); }
		if (bJustifyZ) { JustifyZ.JustifyAxis(2, Index, InCenter, InSize, OutCenter, OutSize, OutTranslation); }
	}

	bool Init(FPCGExContext* InContext, PCGExData::FFacade* InDataFacade)
	{
		if (bJustifyX) { if (!JustifyX.Init(InContext, InDataFacade)) { return false; } }
		if (bJustifyY) { if (!JustifyY.Init(InContext, InDataFacade)) { return false; } }
		if (bJustifyZ) { if (!JustifyZ.Init(InContext, InDataFacade)) { return false; } }
		return true;
	}
};

namespace PCGExGeo
{
}

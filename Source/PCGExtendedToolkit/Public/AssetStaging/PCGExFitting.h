// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/PCGExData.h"
#include "PCGExRandom.h"

#include "PCGExFitting.generated.h"


UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Transform Component Selector"))
enum class EPCGExFitMode : uint8
{
	None       = 0 UMETA(DisplayName = "None", ToolTip="No fitting"),
	Uniform    = 1 UMETA(DisplayName = "Uniform", ToolTip="Uniform fit"),
	Individual = 2 UMETA(DisplayName = "Individual", ToolTip="Per-component fit"),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Scale to fit"))
enum class EPCGExScaleToFit : uint8
{
	None = 0 UMETA(DisplayName = "None", ToolTip="No fitting"),
	Fill = 1 UMETA(DisplayName = "Fill", ToolTip="..."),
	Min  = 2 UMETA(DisplayName = "Min", ToolTip="..."),
	Max  = 3 UMETA(DisplayName = "Max", ToolTip="..."),
	Avg  = 4 UMETA(DisplayName = "Average", ToolTip="..."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Justify From"))
enum class EPCGExJustifyFrom : uint8
{
	Min    = 0 UMETA(DisplayName = "Min", ToolTip="..."),
	Center = 1 UMETA(DisplayName = "Center", ToolTip="..."),
	Max    = 2 UMETA(DisplayName = "Max", ToolTip="..."),
	Pivot  = 3 UMETA(DisplayName = "Pivot", ToolTip="..."),
	Custom = 4 UMETA(DisplayName = "Custom", ToolTip="..."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Justify To"))
enum class EPCGExJustifyTo : uint8
{
	Same   = 0 UMETA(DisplayName = "Same", ToolTip="..."),
	Min    = 1 UMETA(DisplayName = "Min", ToolTip="..."),
	Center = 2 UMETA(DisplayName = "Center", ToolTip="..."),
	Max    = 3 UMETA(DisplayName = "Max", ToolTip="..."),
	Pivot  = 4 UMETA(DisplayName = "Pivot", ToolTip="..."),
	Custom = 5 UMETA(DisplayName = "Custom", ToolTip="..."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Justify From"))
enum class EPCGExVariationMode : uint8
{
	Disabled = 0 UMETA(DisplayName = "Disabled", ToolTip="..."),
	Before   = 1 UMETA(DisplayName = "Before fitting", ToolTip="Variation are applied to the point that will be fitted"),
	After    = 2 UMETA(DisplayName = "After fitting", ToolTip="Variation are applied to the fitted bounds"),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExScaleToFitDetails
{
	GENERATED_BODY()

	FPCGExScaleToFitDetails()
	{
	}

	explicit FPCGExScaleToFitDetails(const EPCGExFitMode DefaultFit)
	{
		ScaleToFitMode = DefaultFit;
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExFitMode ScaleToFitMode = EPCGExFitMode::Uniform;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="ScaleToFitMode==EPCGExFitMode::Uniform", EditConditionHides))
	EPCGExScaleToFit ScaleToFit = EPCGExScaleToFit::Min;

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
		FromSourceAttribute.Update(TEXT("None"));
		ToSourceAttribute.Update(TEXT("None"));
	}

	/** Reference point inside the bounds getting justified */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExJustifyFrom From = EPCGExJustifyFrom::Center;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="From==EPCGExJustifyFrom::Custom", EditConditionHides))
	EPCGExInputValueType FromInput = EPCGExInputValueType::Constant;

	/**  Value is expected to be 0-1 normalized, 0 being bounds min and 1 being bounds min + size. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="From==EPCGExJustifyFrom::Custom && FromInput==EPCGExInputValueType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector FromSourceAttribute;

	/**  Value is expected to be 0-1 normalized, 0 being bounds min and 1 being bounds min + size. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="From==EPCGExJustifyFrom::Custom && FromInput==EPCGExInputValueType::Constant", EditConditionHides))
	double FromConstant = 0.5;

	TSharedPtr<PCGExData::TBuffer<double>> FromGetter;
	TSharedPtr<PCGExData::TBuffer<FVector>> SharedFromGetter;

	/** Reference point inside the container bounds*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExJustifyTo To = EPCGExJustifyTo::Same;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="To==EPCGExJustifyTo::Custom", EditConditionHides))
	EPCGExInputValueType ToInput = EPCGExInputValueType::Constant;

	/**  Value is expected to be 0-1 normalized, 0 being bounds min and 1 being bounds min + size. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="To==EPCGExJustifyTo::Custom && ToInput==EPCGExInputValueType::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector ToSourceAttribute;

	/**  Value is expected to be 0-1 normalized, 0 being bounds min and 1 being bounds min + size. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="To==EPCGExJustifyTo::Custom && ToInput==EPCGExInputValueType::Constant", EditConditionHides))
	double ToConstant = 0.5;

	TSharedPtr<PCGExData::TBuffer<double>> ToGetter;
	TSharedPtr<PCGExData::TBuffer<FVector>> SharedToGetter;

	bool Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade)
	{
		if (From == EPCGExJustifyFrom::Custom && FromInput == EPCGExInputValueType::Attribute)
		{
			FromGetter = InDataFacade->GetScopedBroadcaster<double>(FromSourceAttribute);
			if (!FromGetter)
			{
				if (SharedFromGetter)
				{
					// No complaints, expected.
				}
				else
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Invalid custom 'From' attribute used"));
					return false;
				}
			}
			else
			{
				// Get rid of shared ref
				SharedFromGetter = nullptr;
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
			case EPCGExJustifyFrom::Pivot:
				To = EPCGExJustifyTo::Pivot;
				break;
			}
		}

		if (To == EPCGExJustifyTo::Custom && FromInput == EPCGExInputValueType::Attribute)
		{
			ToGetter = InDataFacade->GetScopedBroadcaster<double>(FromSourceAttribute);
			if (!ToGetter)
			{
				if (SharedToGetter)
				{
					// No complaints, expected.
				}
				else
				{
					PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Invalid custom 'To' attribute used"));
					return false;
				}
			}
			else
			{
				// Get rid of shared ref
				SharedToGetter = nullptr;
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

		const double HalfOutSize = OutSize[Axis] * 0.5;
		const double HalfInSize = InSize[Axis] * 0.5;
		const double FromValue = SharedFromGetter ? SharedFromGetter->Read(Index)[Axis] : FromGetter ? FromGetter->Read(Index) : FromConstant;
		const double ToValue = SharedToGetter ? SharedToGetter->Read(Index)[Axis] : ToGetter ? ToGetter->Read(Index) : ToConstant;

		switch (From)
		{
		default:
		case EPCGExJustifyFrom::Min:
			Start = OutCenter[Axis] - HalfOutSize;
			break;
		case EPCGExJustifyFrom::Center:
			Start = OutCenter[Axis];
			break;
		case EPCGExJustifyFrom::Max:
			Start = OutCenter[Axis] + HalfOutSize;
			break;
		case EPCGExJustifyFrom::Custom:
			Start = OutCenter[Axis] - HalfOutSize + (OutSize[Axis] * FromValue);
			break;
		case EPCGExJustifyFrom::Pivot:
			Start = 0;
			break;
		}

		switch (To)
		{
		default:
		case EPCGExJustifyTo::Min:
			End = InCenter[Axis] - HalfInSize;
			break;
		case EPCGExJustifyTo::Center:
			End = InCenter[Axis];
			break;
		case EPCGExJustifyTo::Max:
			End = InCenter[Axis] + HalfInSize;
			break;
		case EPCGExJustifyTo::Custom:
			End = InCenter[Axis] - HalfInSize + (InSize[Axis] * ToValue);
			break;
		case EPCGExJustifyTo::Same:
			// == Custom but using From values
			End = InCenter[Axis] - HalfInSize * (InSize[Axis] * FromValue);
			break;
		case EPCGExJustifyTo::Pivot:
			End = 0;
			break;
		}

		OutTranslation[Axis] = End - Start;
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
	bool bDoJustifyX = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bDoJustifyX"))
	FPCGExSingleJustifyDetails JustifyX;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bDoJustifyY = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bDoJustifyY"))
	FPCGExSingleJustifyDetails JustifyY;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bDoJustifyZ = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bDoJustifyZ"))
	FPCGExSingleJustifyDetails JustifyZ;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bSharedCustomFromAttribute = false;

	/**  Whether to use matching component of this Vector attribute for custom 'From' justifications instead of setting local attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bSharedCustomFromAttribute"))
	FPCGAttributePropertyInputSelector CustomFromVectorAttribute;

	TSharedPtr<PCGExData::TBuffer<FVector>> SharedFromGetter;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bSharedCustomToAttribute = false;

	/**  Whether to use matching component of this Vector attribute for custom 'To' justifications instead of setting local attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bSharedCustomToAttribute"))
	FPCGAttributePropertyInputSelector CustomToVectorAttribute;

	TSharedPtr<PCGExData::TBuffer<FVector>> SharedToGetter;

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

		if (bDoJustifyX) { JustifyX.JustifyAxis(0, Index, InCenter, InSize, OutCenter, OutSize, OutTranslation); }
		if (bDoJustifyY) { JustifyY.JustifyAxis(1, Index, InCenter, InSize, OutCenter, OutSize, OutTranslation); }
		if (bDoJustifyZ) { JustifyZ.JustifyAxis(2, Index, InCenter, InSize, OutCenter, OutSize, OutTranslation); }
	}

	bool Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade)
	{
		if (bSharedCustomFromAttribute)
		{
			SharedFromGetter = InDataFacade->GetScopedBroadcaster<FVector>(CustomFromVectorAttribute);
		}

		if (bSharedCustomToAttribute)
		{
			SharedToGetter = InDataFacade->GetScopedBroadcaster<FVector>(CustomToVectorAttribute);
		}

		if (bDoJustifyX)
		{
			if (JustifyX.From == EPCGExJustifyFrom::Pivot && (JustifyX.To == EPCGExJustifyTo::Pivot || JustifyX.To == EPCGExJustifyTo::Same))
			{
				bDoJustifyX = false;
			}
			else
			{
				JustifyX.SharedFromGetter = SharedFromGetter;
				JustifyX.SharedToGetter = SharedToGetter;
				if (!JustifyX.Init(InContext, InDataFacade)) { return false; }
			}
		}

		if (bDoJustifyY)
		{
			if (JustifyY.From == EPCGExJustifyFrom::Pivot && (JustifyY.To == EPCGExJustifyTo::Pivot || JustifyY.To == EPCGExJustifyTo::Same))
			{
				bDoJustifyY = false;
			}
			else
			{
				JustifyY.SharedFromGetter = SharedFromGetter;
				JustifyY.SharedToGetter = SharedToGetter;
				if (!JustifyY.Init(InContext, InDataFacade)) { return false; }
			}
		}

		if (bDoJustifyZ)
		{
			if (JustifyZ.From == EPCGExJustifyFrom::Pivot && (JustifyZ.To == EPCGExJustifyTo::Pivot || JustifyZ.To == EPCGExJustifyTo::Same))
			{
				bDoJustifyZ = false;
			}
			else
			{
				JustifyZ.SharedFromGetter = SharedFromGetter;
				JustifyZ.SharedToGetter = SharedToGetter;
				if (!JustifyZ.Init(InContext, InDataFacade)) { return false; }
			}
		}
		return true;
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFittingVariations
{
	GENERATED_BODY()

	/** Index picking mode*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector OffsetMin = FVector::Zero();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FVector OffsetMax = FVector::Zero();

	/** Set offset in world space */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bAbsoluteOffset = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FRotator RotationMin = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FRotator RotationMax = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (AllowPreserveRatio, PCG_Overridable))
	FVector ScaleMin = FVector::One();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (AllowPreserveRatio, PCG_Overridable))
	FVector ScaleMax = FVector::One();

	/** Scale uniformly on each axis. Uses the X component of ScaleMin and ScaleMax. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bUniformScale = true;
};


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExFittingVariationsDetails
{
	GENERATED_BODY()

	FPCGExFittingVariationsDetails()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExVariationMode Offset = EPCGExVariationMode::Disabled;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExVariationMode Rotation = EPCGExVariationMode::Disabled;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExVariationMode Scale = EPCGExVariationMode::Disabled;

	bool bEnabledBefore = true;
	bool bEnabledAfter = true;
	int Seed = 0;

	void Init(const int InSeed)
	{
		Seed = InSeed;
		bEnabledBefore = (Offset == EPCGExVariationMode::Before || Rotation != EPCGExVariationMode::Before || Scale != EPCGExVariationMode::Before);
		bEnabledAfter = (Offset == EPCGExVariationMode::After || Rotation != EPCGExVariationMode::After || Scale != EPCGExVariationMode::After);
	}

	void Apply(
		FPCGPoint& InPoint,
		const FPCGExFittingVariations& Variations,
		const EPCGExVariationMode& Step) const
	{
		FRandomStream RandomSource(PCGExRandom::ComputeSeed(Seed, InPoint.Seed));

		FVector RandomScale = FVector::OneVector;
		FVector RandomOffset = FVector::ZeroVector;
		FQuat RandomRotation = FQuat::Identity;
		const FTransform SourceTransform = InPoint.Transform;

		if (Offset == Step)
		{
			const float OffsetX = RandomSource.FRandRange(Variations.OffsetMin.X, Variations.OffsetMax.X);
			const float OffsetY = RandomSource.FRandRange(Variations.OffsetMin.Y, Variations.OffsetMax.Y);
			const float OffsetZ = RandomSource.FRandRange(Variations.OffsetMin.Z, Variations.OffsetMax.Z);
			RandomOffset = FVector(OffsetX, OffsetY, OffsetZ);
		}

		if (Rotation == Step)
		{
			const float RotationX = RandomSource.FRandRange(Variations.RotationMin.Pitch, Variations.RotationMax.Pitch);
			const float RotationY = RandomSource.FRandRange(Variations.RotationMin.Yaw, Variations.RotationMax.Yaw);
			const float RotationZ = RandomSource.FRandRange(Variations.RotationMin.Roll, Variations.RotationMax.Roll);
			RandomRotation = FQuat(FRotator(RotationX, RotationY, RotationZ).Quaternion());
		}

		if (Scale == Step)
		{
			if (Variations.bUniformScale)
			{
				RandomScale = FVector(RandomSource.FRandRange(Variations.ScaleMin.X, Variations.ScaleMax.X));
			}
			else
			{
				RandomScale.X = RandomSource.FRandRange(Variations.ScaleMin.X, Variations.ScaleMax.X);
				RandomScale.Y = RandomSource.FRandRange(Variations.ScaleMin.Y, Variations.ScaleMax.Y);
				RandomScale.Z = RandomSource.FRandRange(Variations.ScaleMin.Z, Variations.ScaleMax.Z);
			}
		}

		FTransform FinalTransform = SourceTransform;

		if (Variations.bAbsoluteOffset)
		{
			FinalTransform.SetLocation(SourceTransform.GetLocation() + RandomOffset);
		}
		else
		{
			const FTransform RotatedTransform(SourceTransform.GetRotation());
			FinalTransform.SetLocation(SourceTransform.GetLocation() + RotatedTransform.TransformPosition(RandomOffset));
		}

		FinalTransform.SetRotation(SourceTransform.GetRotation() * RandomRotation);
		FinalTransform.SetScale3D(SourceTransform.GetScale3D() * RandomScale);

		InPoint.Transform = FinalTransform;
	}
};

namespace PCGExGeo
{
}

// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "PCGExFitting.h"

void FPCGExScaleToFitDetails::Process(const FPCGPoint& InPoint, const FBox& InBounds, FVector& OutScale, FBox& OutBounds) const
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

bool FPCGExSingleJustifyDetails::Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade)
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

void FPCGExSingleJustifyDetails::JustifyAxis(const int32 Axis, const int32 Index, const FVector& InCenter, const FVector& InSize, const FVector& OutCenter, const FVector& OutSize, FVector& OutTranslation) const
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

void FPCGExJustificationDetails::Process(const int32 Index, const FBox& InBounds, const FBox& OutBounds, FVector& OutTranslation) const
{
	const FVector InCenter = InBounds.GetCenter();
	const FVector InSize = InBounds.GetSize();

	const FVector OutCenter = OutBounds.GetCenter();
	const FVector OutSize = OutBounds.GetSize();

	if (bDoJustifyX) { JustifyX.JustifyAxis(0, Index, InCenter, InSize, OutCenter, OutSize, OutTranslation); }
	if (bDoJustifyY) { JustifyY.JustifyAxis(1, Index, InCenter, InSize, OutCenter, OutSize, OutTranslation); }
	if (bDoJustifyZ) { JustifyZ.JustifyAxis(2, Index, InCenter, InSize, OutCenter, OutSize, OutTranslation); }
}

bool FPCGExJustificationDetails::Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade)
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

void FPCGExFittingVariationsDetails::Init(const int InSeed)
{
	Seed = InSeed;
	bEnabledBefore = (Offset == EPCGExVariationMode::Before || Rotation != EPCGExVariationMode::Before || Scale != EPCGExVariationMode::Before);
	bEnabledAfter = (Offset == EPCGExVariationMode::After || Rotation != EPCGExVariationMode::After || Scale != EPCGExVariationMode::After);
}

void FPCGExFittingVariationsDetails::Apply(FPCGPoint& InPoint, const FPCGExFittingVariations& Variations, const EPCGExVariationMode& Step) const
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

bool FPCGExFittingDetailsHandler::Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InTargetFacade)
{
	TargetDataFacade = InTargetFacade;
	return Justification.Init(InContext, InTargetFacade);
}

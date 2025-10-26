﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExFitting.h"

#include "Data/PCGExData.h"
#include "PCGExMathBounds.h"
#include "AssetStaging/PCGExStaging.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGHelpers.h"

void FPCGExScaleToFitDetails::Process(const PCGExData::FConstPoint& InPoint, const FBox& InBounds, FVector& OutScale, FBox& OutBounds) const
{
	if (ScaleToFitMode == EPCGExFitMode::None) { return; }

	const FVector TargetSize = InPoint.GetLocalBounds().GetSize();
	const FVector TargetScale = InPoint.GetTransform().GetScale3D();
	const FVector TargetSizeScaled = TargetSize * TargetScale;
	const FVector CandidateSize = InBounds.GetSize();

	const double XFactor = TargetSizeScaled.X / CandidateSize.X;
	const double YFactor = TargetSizeScaled.Y / CandidateSize.Y;
	const double ZFactor = TargetSizeScaled.Z / CandidateSize.Z;

	const FVector FitMinMax = FVector(
		FMath::Min3(XFactor, YFactor, ZFactor),
		FMath::Max3(XFactor, YFactor, ZFactor),
		(XFactor + YFactor + ZFactor) / 3);

	OutBounds.Min = InBounds.Min;
	OutBounds.Max = InBounds.Max;


	if (ScaleToFitMode == EPCGExFitMode::Uniform)
	{
		ScaleToFitAxis(ScaleToFit, 0, TargetScale, TargetSize, CandidateSize, FitMinMax, OutScale);
		ScaleToFitAxis(ScaleToFit, 1, TargetScale, TargetSize, CandidateSize, FitMinMax, OutScale);
		ScaleToFitAxis(ScaleToFit, 2, TargetScale, TargetSize, CandidateSize, FitMinMax, OutScale);
	}
	else
	{
		ScaleToFitAxis(ScaleToFitX, 0, TargetScale, TargetSize, CandidateSize, FitMinMax, OutScale);
		ScaleToFitAxis(ScaleToFitY, 1, TargetScale, TargetSize, CandidateSize, FitMinMax, OutScale);
		ScaleToFitAxis(ScaleToFitZ, 2, TargetScale, TargetSize, CandidateSize, FitMinMax, OutScale);
	}
}

void FPCGExScaleToFitDetails::ScaleToFitAxis(
	const EPCGExScaleToFit Fit, const int32 Axis,
	const FVector& TargetScale, const FVector& TargetSize,
	const FVector& CandidateSize, const FVector& MinMaxFit,
	FVector& OutScale)
{
	const double Scale = TargetScale[Axis];
	double FinalScale = Scale;

	switch (Fit)
	{
	default:
	case EPCGExScaleToFit::None:
		break;
	case EPCGExScaleToFit::Fill:
		FinalScale = ((TargetSize[Axis] * Scale) / CandidateSize[Axis]);
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

FPCGExSingleJustifyDetails::FPCGExSingleJustifyDetails()
{
	FromSourceAttribute.Update(TEXT("None"));
	ToSourceAttribute.Update(TEXT("None"));
}

bool FPCGExSingleJustifyDetails::Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade)
{
	if (From == EPCGExJustifyFrom::Custom && FromInput == EPCGExInputValueType::Attribute)
	{
		FromGetter = InDataFacade->GetBroadcaster<double>(FromSourceAttribute, true);
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
		ToGetter = InDataFacade->GetBroadcaster<double>(FromSourceAttribute, true);
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
		SharedFromGetter = InDataFacade->GetBroadcaster<FVector>(CustomFromVectorAttribute, true);
	}

	if (bSharedCustomToAttribute)
	{
		SharedToGetter = InDataFacade->GetBroadcaster<FVector>(CustomToVectorAttribute, true);
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

	bEnabledBefore = (Offset == EPCGExVariationMode::Before || Rotation == EPCGExVariationMode::Before || Scale == EPCGExVariationMode::Before);
	bEnabledAfter = (Offset == EPCGExVariationMode::After || Rotation == EPCGExVariationMode::After || Scale == EPCGExVariationMode::After);
}

void FPCGExFittingVariationsDetails::Apply(const int32 BaseSeed, FTransform& OutTransform, const FPCGExFittingVariations& Variations, const EPCGExVariationMode& Step) const
{
	FRandomStream RandomSource(PCGHelpers::ComputeSeed(Seed, BaseSeed));

	const FTransform SourceTransform = OutTransform;

	if (Offset == Step)
	{
		const FVector RandomOffset = FVector(
			RandomSource.FRandRange(Variations.OffsetMin.X, Variations.OffsetMax.X),
			RandomSource.FRandRange(Variations.OffsetMin.Y, Variations.OffsetMax.Y),
			RandomSource.FRandRange(Variations.OffsetMin.Z, Variations.OffsetMax.Z));

		if (Variations.bAbsoluteOffset)
		{
			OutTransform.SetLocation(SourceTransform.GetLocation() + RandomOffset);
		}
		else
		{
			const FTransform RotatedTransform(SourceTransform.GetRotation());
			OutTransform.SetLocation(SourceTransform.GetLocation() + RotatedTransform.TransformPosition(RandomOffset));
		}
	}

	if (Rotation == Step)
	{
		FRotator RandRot = FRotator(
			RandomSource.FRandRange(Variations.RotationMin.Pitch, Variations.RotationMax.Pitch),
			RandomSource.FRandRange(Variations.RotationMin.Yaw, Variations.RotationMax.Yaw),
			RandomSource.FRandRange(Variations.RotationMin.Roll, Variations.RotationMax.Roll));

		FRotator OutRotation = SourceTransform.GetRotation().Rotator();

		const bool bAbsX = (Variations.AbsoluteRotation & static_cast<uint8>(EPCGExAbsoluteRotationFlags::X)) != 0;
		const bool bAbsY = (Variations.AbsoluteRotation & static_cast<uint8>(EPCGExAbsoluteRotationFlags::Y)) != 0;
		const bool bAbsZ = (Variations.AbsoluteRotation & static_cast<uint8>(EPCGExAbsoluteRotationFlags::Z)) != 0;

		OutRotation.Pitch = (bAbsY ? RandRot.Pitch : OutRotation.Pitch + RandRot.Pitch);
		OutRotation.Yaw = (bAbsZ ? RandRot.Yaw : OutRotation.Yaw + RandRot.Yaw);
		OutRotation.Roll = (bAbsX ? RandRot.Roll : OutRotation.Roll + RandRot.Roll);

		OutTransform.SetRotation(OutRotation.Quaternion());
	}

	if (Scale == Step)
	{
		if (Variations.bUniformScale)
		{
			OutTransform.SetScale3D(SourceTransform.GetScale3D() * FVector(RandomSource.FRandRange(Variations.ScaleMin.X, Variations.ScaleMax.X)));
		}
		else
		{
			OutTransform.SetScale3D(
				SourceTransform.GetScale3D() * FVector(
					RandomSource.FRandRange(Variations.ScaleMin.X, Variations.ScaleMax.X),
					RandomSource.FRandRange(Variations.ScaleMin.Y, Variations.ScaleMax.Y),
					RandomSource.FRandRange(Variations.ScaleMin.Z, Variations.ScaleMax.Z)));
		}
	}
}

bool FPCGExFittingDetailsHandler::Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InTargetFacade)
{
	TargetDataFacade = InTargetFacade;
	return Justification.Init(InContext, InTargetFacade);
}

void FPCGExFittingDetailsHandler::ComputeTransform(const int32 TargetIndex, FTransform& OutTransform, FBox& InOutBounds, const bool bWorldSpace) const
{
	//
	check(TargetDataFacade);
	const PCGExData::FConstPoint& TargetPoint = TargetDataFacade->Source->GetInPoint(TargetIndex);
	const FTransform& InTransform = TargetPoint.GetTransform();

	if (bWorldSpace) { OutTransform = InTransform; }

	FVector OutScale = InTransform.GetScale3D();
	FVector OutTranslation = FVector::ZeroVector;

	ScaleToFit.Process(TargetPoint, InOutBounds, OutScale, InOutBounds);
	Justification.Process(
		TargetIndex, PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(TargetPoint),
		FBox(InOutBounds.Min * OutScale, InOutBounds.Max * OutScale),
		OutTranslation);

	OutTransform.AddToTranslation(InTransform.GetRotation().RotateVector(OutTranslation));
	OutTransform.SetScale3D(OutScale);
}

void FPCGExFittingDetailsHandler::ComputeLocalTransform(const int32 TargetIndex, const FTransform& InLocalXForm, FTransform& OutTransform, FBox& InOutBounds) const
{
	check(TargetDataFacade);
	const PCGExData::FConstPoint& TargetPoint = TargetDataFacade->Source->GetInPoint(TargetIndex);

	FVector OutScale = OutTransform.GetScale3D();
	FVector OutTranslation = FVector::ZeroVector;

	ScaleToFit.Process(TargetPoint, InOutBounds.TransformBy(InLocalXForm), OutScale, InOutBounds);
	Justification.Process(
		TargetIndex, PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(TargetPoint),
		FBox(InOutBounds.Min * OutScale, InOutBounds.Max * OutScale),
		OutTranslation);

	//OutTransform = OutTransform * InLocalXForm.Inverse(); 
	OutTransform.SetScale3D(OutScale);
	OutTransform.AddToTranslation(OutTranslation);
	OutTransform.SetRotation(InLocalXForm.GetRotation() * OutTransform.GetRotation());
}

bool FPCGExFittingDetailsHandler::WillChangeBounds() const
{
	return ScaleToFit.ScaleToFitMode != EPCGExFitMode::None;
}

bool FPCGExFittingDetailsHandler::WillChangeTransform() const
{
	return ScaleToFit.ScaleToFitMode != EPCGExFitMode::None || Justification.bDoJustifyX || Justification.bDoJustifyY || Justification.bDoJustifyZ;
}

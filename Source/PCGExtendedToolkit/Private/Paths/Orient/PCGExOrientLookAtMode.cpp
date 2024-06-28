// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Orient/PCGExOrientLookAtMode.h"

#include "PCGExPointsProcessor.h"

void UPCGExOrientLookAt::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	const UPCGExOrientLookAt* TypedOther = Cast<UPCGExOrientLookAt>(Other);
	if (TypedOther)
	{
		LookAt = TypedOther->LookAt;
		LookAtAttribute = TypedOther->LookAtAttribute;
	}
}

void UPCGExOrientLookAt::PrepareForData(PCGExData::FPointIO* InPointIO)
{
	PCGEX_DELETE(LookAtGetter)
	Super::PrepareForData(InPointIO);

	if (LookAt == EPCGExOrientLookAtMode::Direction)
	{
		LookAtGetter = new PCGEx::FLocalVectorGetter();
		LookAtGetter->Capture(LookAtAttribute);
		if (!LookAtGetter->Grab(InPointIO))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("LookAt Attribute ({0}) is not valid."), FText::FromString(LookAtAttribute.GetName().ToString())));
		}
	}
}

FTransform UPCGExOrientLookAt::ComputeOrientation(const PCGEx::FPointRef& Point, const PCGEx::FPointRef& Previous, const PCGEx::FPointRef& Next, const double DirectionMultiplier) const
{
	switch (LookAt)
	{
	default: ;
	case EPCGExOrientLookAtMode::NextPoint:
		return LookAtWorldPos(Point.Point->Transform, Next.Point->Transform.GetLocation(), DirectionMultiplier);
	case EPCGExOrientLookAtMode::PreviousPoint:
		return LookAtWorldPos(Point.Point->Transform, Previous.Point->Transform.GetLocation(), DirectionMultiplier);
	case EPCGExOrientLookAtMode::Direction:
		return LookAtDirection(Point.Point->Transform, Point.Index, DirectionMultiplier);
	case EPCGExOrientLookAtMode::Position:
		return LookAtPosition(Point.Point->Transform, Point.Index, DirectionMultiplier);
	}
}

FTransform UPCGExOrientLookAt::LookAtWorldPos(FTransform InT, const FVector& WorldPos, const double DirectionMultiplier) const
{
	FTransform OutT = InT;
	OutT.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis,
			(InT.GetLocation() - WorldPos).GetSafeNormal() * DirectionMultiplier,
			PCGExMath::GetDirection(UpAxis)));
	return OutT;
}

FTransform UPCGExOrientLookAt::LookAtDirection(FTransform InT, const int32 Index, const double DirectionMultiplier) const
{
	FTransform OutT = InT;
	OutT.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis, LookAtGetter->Values[Index].GetSafeNormal() * DirectionMultiplier, PCGExMath::GetDirection(UpAxis)));
	return OutT;
}

FTransform UPCGExOrientLookAt::LookAtPosition(FTransform InT, const int32 Index, const double DirectionMultiplier) const
{
	FTransform OutT = InT;
	const FVector Current = OutT.GetLocation();
	const FVector Position = LookAtGetter->Values[Index];
	OutT.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis, (Position - Current).GetSafeNormal() * DirectionMultiplier, PCGExMath::GetDirection(UpAxis)));
	return OutT;
}

void UPCGExOrientLookAt::Cleanup()
{
	PCGEX_DELETE(LookAtGetter)
	Super::Cleanup();
}

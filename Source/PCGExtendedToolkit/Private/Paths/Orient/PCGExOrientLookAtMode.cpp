// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Orient/PCGExOrientLookAtMode.h"

#include "PCGExPointsProcessor.h"

void UPCGExOrientLookAt::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExOrientLookAt* TypedOther = Cast<UPCGExOrientLookAt>(Other))
	{
		LookAt = TypedOther->LookAt;
		LookAtAttribute = TypedOther->LookAtAttribute;
	}
}

bool UPCGExOrientLookAt::PrepareForData(const TSharedRef<PCGExData::FFacade>& InDataFacade)
{
	if (!Super::PrepareForData(InDataFacade)) { return false; }

	if (LookAt == EPCGExOrientLookAtMode::Direction || LookAt == EPCGExOrientLookAtMode::Position)
	{
		LookAtGetter = InDataFacade->GetScopedBroadcaster<FVector>(LookAtAttribute);
		if (!LookAtGetter)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("LookAt Attribute ({0}) is not valid."), FText::FromString(LookAtAttribute.GetName().ToString())));
			return false;
		}
	}

	return true;
}

FTransform UPCGExOrientLookAt::ComputeOrientation(const PCGExData::FPointRef& Point, const PCGExData::FPointRef& Previous, const PCGExData::FPointRef& Next, const double DirectionMultiplier) const
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

FTransform UPCGExOrientLookAt::LookAtWorldPos(const FTransform InT, const FVector& WorldPos, const double DirectionMultiplier) const
{
	FTransform OutT = InT;
	OutT.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis,
			(InT.GetLocation() - WorldPos).GetSafeNormal() * DirectionMultiplier,
			PCGExMath::GetDirection(UpAxis)));
	return OutT;
}

FTransform UPCGExOrientLookAt::LookAtDirection(const FTransform InT, const int32 Index, const double DirectionMultiplier) const
{
	FTransform OutT = InT;
	OutT.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis, LookAtGetter->Read(Index).GetSafeNormal() * DirectionMultiplier, PCGExMath::GetDirection(UpAxis)));
	return OutT;
}

FTransform UPCGExOrientLookAt::LookAtPosition(const FTransform InT, const int32 Index, const double DirectionMultiplier) const
{
	FTransform OutT = InT;
	const FVector Current = OutT.GetLocation();
	const FVector Position = LookAtGetter->Read(Index);
	OutT.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis, (Position - Current).GetSafeNormal() * DirectionMultiplier, PCGExMath::GetDirection(UpAxis)));
	return OutT;
}

void UPCGExOrientLookAt::Cleanup()
{
	LookAtGetter.Reset();
	Super::Cleanup();
}

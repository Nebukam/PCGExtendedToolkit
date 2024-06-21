// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/Orient/PCGExOrientLookAt.h"

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

	if (LookAt == EPCGExOrientLookAt::Direction)
	{
		LookAtGetter = new PCGEx::FLocalVectorGetter();
		LookAtGetter->Capture(LookAtAttribute);
		if (!LookAtGetter->Grab(InPointIO))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("LookAt Attribute ({0}) is not valid."), FText::FromString(LookAtAttribute.GetName().ToString())));
		}
	}
}

void UPCGExOrientLookAt::Orient(PCGEx::FPointRef& Point, const PCGEx::FPointRef& Previous, const PCGEx::FPointRef& Next, const double Factor) const
{
	Super::Orient(Point, Previous, Next, Factor);
	switch (LookAt)
	{
	default: ;
	case EPCGExOrientLookAt::NextPoint:
		LookAtWorldPos(Point.MutablePoint(), Next.Point->Transform.GetLocation(), Factor);
		break;
	case EPCGExOrientLookAt::PreviousPoint:
		LookAtWorldPos(Point.MutablePoint(), Previous.Point->Transform.GetLocation(), Factor);
		break;
	case EPCGExOrientLookAt::Direction:
		LookAtDirection(Point.MutablePoint(), Point.Index, Factor);
		break;
	case EPCGExOrientLookAt::Position:
		LookAtPosition(Point.MutablePoint(), Point.Index, Factor);
		break;
	}
}

void UPCGExOrientLookAt::LookAtWorldPos(FPCGPoint& Point, const FVector& WorldPos, const double Factor) const
{
	Point.Transform.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis,
			(Point.Transform.GetLocation() - WorldPos).GetSafeNormal() * Factor,
			PCGExMath::GetDirection(UpAxis)));
}

void UPCGExOrientLookAt::LookAtDirection(FPCGPoint& Point, const int32 Index, const double Factor) const
{
	Point.Transform.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis, LookAtGetter->Values[Index].GetSafeNormal() * Factor, PCGExMath::GetDirection(UpAxis)));
}

void UPCGExOrientLookAt::LookAtPosition(FPCGPoint& Point, const int32 Index, const double Factor) const
{
	const FVector Current = Point.Transform.GetLocation();
	const FVector Position = LookAtGetter->Values[Index];
	Point.Transform.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis, (Position - Current).GetSafeNormal() * Factor, PCGExMath::GetDirection(UpAxis)));
}

void UPCGExOrientLookAt::Cleanup()
{
	PCGEX_DELETE(LookAtGetter)
	Super::Cleanup();
}

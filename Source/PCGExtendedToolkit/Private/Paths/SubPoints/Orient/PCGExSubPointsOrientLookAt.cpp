// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/


#include "Paths/SubPoints/Orient/PCGExSubPointsOrientLookAt.h"

#include "PCGExPointsProcessor.h"

void UPCGExSubPointsOrientLookAt::PrepareForData(PCGExData::FPointIO& InPointIO)
{
	PCGEX_DELETE(LookAtGetter)
	Super::PrepareForData(InPointIO);

	if (LookAt == EPCGExOrientLookAt::Attribute)
	{
		LookAtGetter = new PCGEx::FLocalVectorGetter();
		LookAtGetter->Capture(LookAtSelector);
		if (!LookAtGetter->Grab(InPointIO))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("LookAt Attribute ({0}) is not valid."), FText::FromString(LookAtSelector.GetName().ToString())));
		}
	}
}

void UPCGExSubPointsOrientLookAt::ProcessSubPoints(const PCGEx::FPointRef& Start, const PCGEx::FPointRef& End, const TArrayView<FPCGPoint>& SubPoints, const PCGExMath::FPathMetricsSquared& Metrics) const
{
	const int32 NumPoints = SubPoints.Num();
	const int32 NumPointsMinusOne = NumPoints - 1;

	if (bClosedPath)
	{
		switch (LookAt)
		{
		case EPCGExOrientLookAt::NextPoint:
			for (int i = 0; i < NumPoints; i++)
			{
				LookAtNext(
					SubPoints[i],
					SubPoints[PCGExMath::Tile(i - 1, 0, NumPointsMinusOne)],
					SubPoints[PCGExMath::Tile(i + 1, 0, NumPointsMinusOne)]);
			}
			break;
		case EPCGExOrientLookAt::PreviousPoint:
			for (int i = 0; i < NumPoints; i++)
			{
				LookAtPrev(
					SubPoints[i],
					SubPoints[PCGExMath::Tile(i - 1, 0, NumPointsMinusOne)],
					SubPoints[PCGExMath::Tile(i + 1, 0, NumPointsMinusOne)]);
			}
			break;
		case EPCGExOrientLookAt::Attribute:
			if (bAttributeAsOffset) { for (int i = 0; i < NumPoints; i++) { LookAtAttribute(SubPoints[i], Start.Index + i); } }
			else { for (int i = 0; i < NumPoints; i++) { LookAtAttributeOffset(SubPoints[i], Start.Index + i); } }
			break;
		}
	}
	else
	{
		switch (LookAt)
		{
		case EPCGExOrientLookAt::NextPoint:
			LookAtNext(SubPoints[0], *Start.Point, SubPoints[1]);
			for (int i = 1; i < NumPointsMinusOne; i++) { LookAtNext(SubPoints[i], SubPoints[i - 1], SubPoints[i + 1]); }
			LookAtNext(SubPoints.Last(), SubPoints[NumPointsMinusOne - 1], *End.Point);
			break;
		case EPCGExOrientLookAt::PreviousPoint:
			LookAtPrev(SubPoints[0], *Start.Point, SubPoints[1]);
			for (int i = 1; i < NumPointsMinusOne; i++) { LookAtPrev(SubPoints[i], SubPoints[i - 1], SubPoints[i + 1]); }
			LookAtPrev(SubPoints.Last(), SubPoints[NumPointsMinusOne - 1], *End.Point);
			break;
		case EPCGExOrientLookAt::Attribute:
			if (bAttributeAsOffset) { for (int i = 0; i < NumPoints; i++) { LookAtAttribute(SubPoints[i], Start.Index + i); } }
			else { for (int i = 0; i < NumPoints; i++) { LookAtAttributeOffset(SubPoints[i], Start.Index + i); } }
			break;
		}
	}
}

void UPCGExSubPointsOrientLookAt::LookAtNext(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const
{
	Point.Transform.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis,
			(Point.Transform.GetLocation() - NextPoint.Transform.GetLocation()),
			PCGExMath::GetDirection(UpAxis)));
}

void UPCGExSubPointsOrientLookAt::LookAtPrev(FPCGPoint& Point, const FPCGPoint& PreviousPoint, const FPCGPoint& NextPoint) const
{
	Point.Transform.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis,
			(Point.Transform.GetLocation() - PreviousPoint.Transform.GetLocation()),
			PCGExMath::GetDirection(UpAxis)));
}

void UPCGExSubPointsOrientLookAt::LookAtAttribute(FPCGPoint& Point, const int32 Index) const
{
	Point.Transform.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis, (*LookAtGetter)[Index], PCGExMath::GetDirection(UpAxis)));
}

void UPCGExSubPointsOrientLookAt::LookAtAttributeOffset(FPCGPoint& Point, const int32 Index) const
{
	Point.Transform.SetRotation(
		PCGExMath::MakeDirection(
			OrientAxis, Point.Transform.GetLocation() + (*LookAtGetter)[Index], PCGExMath::GetDirection(UpAxis)));
}

void UPCGExSubPointsOrientLookAt::Cleanup()
{
	PCGEX_DELETE(LookAtGetter)
	Super::Cleanup();
}

void UPCGExSubPointsOrientLookAt::ApplyOverrides()
{
	Super::ApplyOverrides();
	PCGEX_OVERRIDE_OP_PROPERTY(bAttributeAsOffset, FName(TEXT("Orient/AttributeAsOffset")), EPCGMetadataTypes::Boolean);
}

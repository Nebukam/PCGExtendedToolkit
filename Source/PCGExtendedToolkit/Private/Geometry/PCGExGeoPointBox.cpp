// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Geometry/PCGExGeoPointBox.h"

#include "CoreMinimal.h"
#include "PCGExH.h"
#include "PCGExHelpers.h"
#include "Data/PCGExPointIO.h"

namespace PCGExGeo
{
	FCut::FCut(const FVector& InPosition, const FVector& InNormal, const int32 InBoxIndex, const int32 InIdx, const EPCGExCutType InType)
		: Position(InPosition), Normal(InNormal), BoxIndex(InBoxIndex), Idx(InIdx), Type(InType)
	{
	}

	FSample::FSample(const FVector& InDistances, const int32 InBoxIndex, const bool IsInside)
		: Distances(InDistances), BoxIndex(InBoxIndex), bIsInside(IsInside)
	{
	}

	FIntersections::FIntersections(const FVector& InStartPosition, const FVector& InEndPosition)
		: StartPosition(InStartPosition), EndPosition(InEndPosition)
	{
	}

	bool FIntersections::IsEmpty() const
	{
		return Cuts.IsEmpty();
	}

	void FIntersections::Sort()
	{
		// TODO : Cache distances 
		Cuts.Sort(
			[&](const FCut& A, const FCut& B)
			{
				const double DistToA = FVector::DistSquared(StartPosition, A.Position);
				const double DistToB = FVector::DistSquared(StartPosition, B.Position);
				if (DistToA == DistToB) { return A.Idx < B.Idx; }
				return DistToA < DistToB;
			});
	}

	void FIntersections::SortAndDedupe()
	{
		Sort();

		if (Cuts.IsEmpty() || Cuts.Num() < 2) { return; }

		FVector LastPos = Cuts[0].Position;

		for (int i = 1; i < Cuts.Num(); i++)
		{
			FVector Pos = (Cuts.GetData() + i)->Position;
			if (Pos == LastPos)
			{
				Cuts.RemoveAt(i);
				i--;
			}
			LastPos = Pos;
		}
	}

	FBoxCenterAndExtent FIntersections::GetBoxCenterAndExtent() const
	{
		FBox Box = FBox(ForceInit);
		Box += StartPosition;
		Box += EndPosition;
		return FBoxCenterAndExtent(Box);
	}

	void FIntersections::Insert(const FVector& Position, const FVector& Normal, const int32 Index, const int32 Idx, const EPCGExCutType Type)
	{
		Cuts.Emplace(Position, Normal, Index, Idx, Type);
	}

	FPointBox::FPointBox(const PCGExData::FConstPoint& InPoint, const int32 InIndex, const EPCGExPointBoundsSource BoundsSource, double Expansion):
		Matrix(InPoint.GetTransform().ToMatrixNoScale()),
		InvMatrix(Matrix.Inverse()),
		Index(InIndex)
	{
		const FBox PointBox = PCGExMath::GetLocalBounds(InPoint, BoundsSource);
		Extents = PointBox.GetExtent();
		double Size = PointBox.GetExtent().Size();
		double SanitizedExpansion = Expansion < 0 ? FMath::Max(Expansion, -Size) : Expansion;

		Box = FBox(PointBox.Min, PointBox.Max);
		BoxExpanded = Box.ExpandBy(Expansion);
		SearchableBounds = FBoxSphereBounds(InPoint.GetTransform().GetLocation() + Box.GetCenter(), FVector(Size + SanitizedExpansion * 1.5), Size + SanitizedExpansion * 1.5);
		RadiusSquared = FMath::Square(Size);
		RadiusSquaredExpanded = FMath::Square(Size + SanitizedExpansion);
	}

	void FPointBox::Sample(const FVector& Position, FSample& OutSample) const
	{
		const FVector LocalPosition = Matrix.InverseTransformPosition(Position);
		const FVector LocalCenter = Box.GetCenter();
		OutSample.bIsInside = Box.IsInside(LocalPosition);
		OutSample.Distances = LocalPosition;
		OutSample.BoxIndex = Index;
		OutSample.UVW = (LocalPosition - LocalCenter) / Extents;
		OutSample.Weight = 1 - ((
			(FMath::Clamp(FMath::Abs(OutSample.UVW.X), 0, Extents.X) / Extents.X) +
			(FMath::Clamp(FMath::Abs(OutSample.UVW.Y), 0, Extents.Y) / Extents.Y) +
			(FMath::Clamp(FMath::Abs(OutSample.UVW.Z), 0, Extents.Z) / Extents.Z)) / 3);
	}

	void FPointBox::Sample(const PCGExData::FConstPoint& Point, FSample& OutSample) const
	{
		Sample(Point.GetTransform().GetLocation(), OutSample);
	}

	bool FPointBox::ProcessIntersections(FIntersections* InIntersections, const int32 Idx) const
	{
		FVector OutIntersection1 = FVector::ZeroVector;
		FVector OutIntersection2 = FVector::ZeroVector;
		FVector OutHitNormal1 = FVector::ZeroVector;
		FVector OutHitNormal2 = FVector::ZeroVector;
		bool bIsIntersection2Valid = false;
		bool bInverseDir = false;
		if (SegmentIntersection(
			InIntersections->StartPosition, InIntersections->EndPosition,
			OutIntersection1, OutIntersection2, bIsIntersection2Valid,
			OutHitNormal1, OutHitNormal2, bInverseDir))
		{
			if (bInverseDir)
			{
				if (bIsIntersection2Valid)
				{
					InIntersections->Insert(OutIntersection1, OutHitNormal1, Index, Idx, EPCGExCutType::Exit);
					InIntersections->Insert(OutIntersection2, OutHitNormal2, Index, Idx, EPCGExCutType::Entry);
				}
				else
				{
					InIntersections->Insert(OutIntersection1, OutHitNormal1, Index, Idx, EPCGExCutType::ExitNoEntry);
				}
			}
			else
			{
				if (bIsIntersection2Valid)
				{
					InIntersections->Insert(OutIntersection1, OutHitNormal1, Index, Idx, EPCGExCutType::Entry);
					InIntersections->Insert(OutIntersection2, OutHitNormal2, Index, Idx, EPCGExCutType::Exit);
				}
				else
				{
					InIntersections->Insert(OutIntersection1, OutHitNormal1, Index, Idx, EPCGExCutType::EntryNoExit);
				}
			}

			return true;
		}
		return false;
	}

	bool FPointBox::SegmentIntersection(const FVector& Start, const FVector& End, FVector& OutIntersection1, FVector& OutIntersection2, bool& bIsI2Valid, FVector& OutHitNormal1, FVector& OutHitNormal2, bool& bInverseDir) const
	{
		const FVector LocalStart = Matrix.InverseTransformPosition(Start);
		const FVector LocalEnd = Matrix.InverseTransformPosition(End);

		const bool bIsStartInside = Box.IsInside(LocalStart);
		const bool bIsEndInside = Box.IsInside(LocalEnd);

		bIsI2Valid = false;
		bInverseDir = false;

		if (bIsStartInside && bIsEndInside) { return false; }

		FVector HitLocation;
		FVector HitNormal;
		float HitTime;

		bool bHasValidIntersection = false;

		if (bIsEndInside)
		{
			if (FMath::LineExtentBoxIntersection(Box, LocalStart, LocalEnd, FVector::ZeroVector, HitLocation, HitNormal, HitTime))
			{
				OutIntersection1 = Matrix.TransformPosition(HitLocation);
				OutHitNormal1 = Matrix.TransformVector(HitNormal);
				return OutIntersection1 != Start && OutIntersection1 != End;
			}

			return false;
		}

		if (bIsStartInside)
		{
			if (FMath::LineExtentBoxIntersection(Box, LocalEnd, LocalStart, FVector::ZeroVector, HitLocation, HitNormal, HitTime))
			{
				OutIntersection1 = Matrix.TransformPosition(HitLocation);
				OutHitNormal1 = Matrix.TransformVector(HitNormal);
				bInverseDir = true;
				return OutIntersection1 != Start && OutIntersection1 != End;
			}

			return false;
		}

		if (FMath::LineExtentBoxIntersection(Box, LocalStart, LocalEnd, FVector::ZeroVector, HitLocation, HitNormal, HitTime))
		{
			OutIntersection1 = Matrix.TransformPosition(HitLocation);
			OutHitNormal1 = Matrix.TransformVector(HitNormal);
			bHasValidIntersection = OutIntersection1 != Start && OutIntersection1 != End;
		}

		if (FMath::LineExtentBoxIntersection(Box, LocalEnd, LocalStart, FVector::ZeroVector, HitLocation, HitNormal, HitTime))
		{
			if (!bHasValidIntersection)
			{
				OutIntersection1 = Matrix.TransformPosition(HitLocation);
				OutHitNormal1 = Matrix.TransformVector(HitNormal);
				bInverseDir = true;
				bHasValidIntersection = OutIntersection1 != Start && OutIntersection1 != End;
			}
			else
			{
				OutIntersection2 = Matrix.TransformPosition(HitLocation);
				OutHitNormal2 = Matrix.TransformVector(HitNormal);
				bIsI2Valid = OutIntersection1 != OutIntersection2 && (OutIntersection2 != Start && OutIntersection2 != End);
			}

			bHasValidIntersection = bHasValidIntersection || bIsI2Valid;
		}

		return bHasValidIntersection;
	}

	FPointBoxCloud::FPointBoxCloud(const UPCGBasePointData* PointData, const EPCGExPointBoundsSource BoundsSource, const double Expansion)
	{
		CloudBounds = PointData->GetBounds();
		Octree = MakeUnique<FPointBoxOctree>(CloudBounds.GetCenter(), CloudBounds.GetExtent().Length() * 1.5);

		SearchPadding = FVector(FMath::Abs(Expansion) * 2);
		CloudBounds = FBox(ForceInit);

		Boxes.Init(nullptr, PointData->GetNumPoints());

		for (int i = 0; i < Boxes.Num(); i++)
		{
			PCGEX_MAKE_SHARED(NewPointBox, FPointBox, PCGExData::FConstPoint(PointData,i), i, BoundsSource, Expansion)
			CloudBounds += NewPointBox->Box.TransformBy(NewPointBox->Matrix);
			Boxes[i] = NewPointBox;
			Octree->AddElement(NewPointBox.Get());
		}
	}

	bool FPointBoxCloud::FindIntersections(FIntersections* InIntersections) const
	{
		const FBoxCenterAndExtent BCAE = InIntersections->GetBoxCenterAndExtent();
		Octree->FindElementsWithBoundsTest(BCAE, [&](const FPointBox* NearbyBox) { NearbyBox->ProcessIntersections(InIntersections, Idx); });
		return !InIntersections->Cuts.IsEmpty();
	}

	bool FPointBoxCloud::LooseOverlaps(const UPCGBasePointData* InPointData) const
	{
		const FBox PointBounds = InPointData->GetBounds();
		if (CloudBounds.Intersect(PointBounds)) { return true; }
		return CloudBounds.IsInside(PointBounds);
	}

	bool FPointBoxCloud::Overlaps(const UPCGBasePointData* InPointData) const
	{
		const FBox PointBounds = InPointData->GetBounds();
		if (CloudBounds.Intersect(PointBounds)) { return true; }
		return CloudBounds.IsInside(PointBounds);
	}

	bool FPointBoxCloud::Encompass(const UPCGBasePointData* InPointData) const
	{
		const FBox PointBounds = InPointData->GetBounds();

		if (!CloudBounds.Intersect(PointBounds))
		{
			if (!CloudBounds.IsInside(PointBounds))
			{
				// No intersection nor encapsulation, don't bother
				return false;
			}
		}

		return false;
	}

	bool FPointBoxCloud::Sample(const PCGExData::FConstPoint& Point, const EPCGExPointBoundsSource BoundsSource, TArray<FSample>& OutSample) const
	{
		const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(Point.GetTransform().GetLocation(), PCGExMath::GetLocalBounds(Point, BoundsSource).GetExtent());
		Octree->FindElementsWithBoundsTest(
			BCAE, [&](const FPointBox* NearbyBox)
			{
				FSample& Sample = OutSample.Emplace_GetRef();
				NearbyBox->Sample(Point, Sample);
			});

		return !OutSample.IsEmpty();
	}
}

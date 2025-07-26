// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Geometry/PCGExGeoPointBox.h"

#include "CoreMinimal.h"
#include "PCGExHelpers.h"

namespace PCGExGeo
{
	void FIntersections::Sort()
	{
		Cuts.Sort(
			[&](const FCut& A, const FCut& B)
			{
				return FVector::DistSquared(StartPosition, A.Position) < FVector::DistSquared(StartPosition, B.Position);
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

	void FIntersections::Insert(const FVector& Position, const FVector& Normal, const int32 Index)
	{
		Cuts.Emplace(Position, Normal, Index);
	}

	int32 FSegmentation::GetNumCuts() const
	{
		int32 Sum = 0;
		for (const TPair<uint64, TSharedPtr<FIntersections>>& Pair : IntersectionsMap) { Sum += Pair.Value->Cuts.Num(); }
		return Sum;
	}

	void FSegmentation::ReduceToArray()
	{
		PCGEx::InitArray(IntersectionsList, IntersectionsMap.Num());
		int32 Index = 0;
		for (const TPair<uint64, TSharedPtr<FIntersections>>& Pair : IntersectionsMap) { IntersectionsList[Index++] = Pair.Value; }
		IntersectionsMap.Empty();
	}

	TSharedPtr<FIntersections> FSegmentation::Find(const uint64 Key)
	{
		{
			FReadScopeLock ReadScopeLock(IntersectionsLock);
			if (TSharedPtr<FIntersections>* ExistingIntersection = IntersectionsMap.Find(Key)) { return *ExistingIntersection; }
		}

		return nullptr;
	}

	void FSegmentation::Insert(const TSharedPtr<FIntersections>& InIntersections)
	{
		FWriteScopeLock WriteScopeLock(IntersectionsLock);
		IntersectionsMap.Add(InIntersections->GetKey(), InIntersections);
	}

	TSharedPtr<FIntersections> FSegmentation::GetOrCreate(const int32 Start, const int32 End)
	{
		const uint64 HID = PCGEx::H64U(Start, End);
		if (TSharedPtr<FIntersections> ExistingIntersection = Find(HID)) { return ExistingIntersection; }

		PCGEX_MAKE_SHARED(NewIntersections, FIntersections, Start, End)
		Insert(NewIntersections);

		return NewIntersections;
	}

	FPointBox::FPointBox(const PCGExData::FConstPoint& InPoint, const int32 InIndex, const EPCGExPointBoundsSource BoundsSource, double Expansion):
		Matrix(InPoint.GetTransform().ToMatrixNoScale()),
		InvMatrix(Matrix.Inverse()),
		Index(InIndex)
	{
		const FBox PointBox = PCGExMath::GetLocalBounds(InPoint, BoundsSource);
		Extents = PointBox.GetExtent();
		double Size = Extents.Length();
		double SanitizedExpansion = Expansion < 0 ? FMath::Max(Expansion, -Size) : Expansion;

		Box = FBox(Extents * -1, Extents);
		BoxExpanded = Box.ExpandBy(Expansion);
		SearchableBounds = FBoxSphereBounds(InPoint.GetTransform().GetLocation(), FVector(Size + SanitizedExpansion * 1.5), Size + SanitizedExpansion * 1.5);
		RadiusSquared = FMath::Square(Size);
		RadiusSquaredExpanded = FMath::Square(Size + SanitizedExpansion);
	}

	void FPointBox::Sample(const FVector& Position, FSample& OutSample) const
	{
		const FVector LocalPosition = Matrix.InverseTransformPosition(Position);
		OutSample.bIsInside = Box.IsInside(LocalPosition);
		OutSample.Distances = LocalPosition;
		OutSample.BoxIndex = Index;
		OutSample.UVW = FVector(
			LocalPosition.X / Extents.X,
			LocalPosition.Y / Extents.Y,
			LocalPosition.Z / Extents.Z);
		OutSample.Weight = 1 - ((
			(FMath::Clamp(FMath::Abs(OutSample.UVW.X), 0, Extents.X) / Extents.X) +
			(FMath::Clamp(FMath::Abs(OutSample.UVW.Y), 0, Extents.Y) / Extents.Y) +
			(FMath::Clamp(FMath::Abs(OutSample.UVW.Z), 0, Extents.Z) / Extents.Z)) / 3);
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
		Octree->FindElementsWithBoundsTest(BCAE, [&](const FPointBox* NearbyBox) { NearbyBox->ProcessIntersections(InIntersections); });
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

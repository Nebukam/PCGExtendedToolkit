// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Math/OBB/PCGExOBBCollection.h"

#include "Data/PCGExPointIO.h"

namespace PCGExMath::OBB
{
	void FCollection::Reserve(int32 Count)
	{
		Bounds.Reserve(Count);
		Orientations.Reserve(Count);
	}

	void FCollection::Add(const FOBB& Box)
	{
		Bounds.Add(Box.Bounds);
		Orientations.Add(Box.Orientation);
	}

	void FCollection::Add(const FTransform& Transform, const FBox& LocalBox, int32 Index)
	{
		WorldBounds += LocalBox.TransformBy(Transform.ToMatrixNoScale());
		Add(Factory::FromTransform(Transform, LocalBox, Index >= 0 ? Index : Bounds.Num()));
	}

	void FCollection::BuildOctree()
	{
		if (Bounds.IsEmpty())
		{
			Octree.Reset();
			return;
		}

		const FVector Extent = WorldBounds.GetExtent();
		const float MaxExtent = FMath::Max3(Extent.X, Extent.Y, Extent.Z) * 1.5f;

		Octree = MakeUnique<PCGExOctree::FItemOctree>(WorldBounds.GetCenter(), MaxExtent);

		const int32 Count = Bounds.Num();
		for (int32 i = 0; i < Count; i++)
		{
			const FBounds& B = Bounds[i];
			Octree->AddElement(PCGExOctree::FItem(i, FBoxSphereBounds(B.Origin, FVector(B.Radius), B.Radius)));
		}
	}

	void FCollection::Reset()
	{
		Bounds.Reset();
		Orientations.Reset();
		Octree.Reset();
		WorldBounds = FBox(ForceInit);
	}

	void FCollection::BuildFrom(const TSharedPtr<PCGExData::FPointIO>& InIO, const EPCGExPointBoundsSource BoundsSource)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExMath::OBB::FCollection::BuildFrom);
		
		const int32 NumPoints = InIO->GetNum();
		Reserve(NumPoints);

		for (int32 i = 0; i < NumPoints; i++)
		{
			const PCGExData::FConstPoint Point = InIO->GetInPoint(i);
			Add(Point.GetTransform(), GetLocalBounds(Point, BoundsSource), i);
		}

		BuildOctree();
	}

	bool FCollection::IsPointInside(const FVector& Point, const EPCGExBoxCheckMode Mode, const float Expansion) const
	{
		if (!Octree) return false;

		const FBoxCenterAndExtent QueryBounds(Point, FVector4(Expansion, Expansion, Expansion, Expansion));

		bool bFound = false;
		Octree->FindFirstElementWithBoundsTest(QueryBounds, [&](const PCGExOctree::FItem& Item) -> bool
		{
			if (TestPoint(GetOBB(Item.Index), Point, Mode, Expansion))
			{
				bFound = true;
				return false;
			}
			return true;
		});

		return bFound;
	}

	bool FCollection::IsPointInside(const FVector& Point, int32& OutIndex, EPCGExBoxCheckMode Mode, float Expansion) const
	{
		if (!Octree) return false;

		const FBoxCenterAndExtent QueryBounds(Point, FVector4(Expansion, Expansion, Expansion, Expansion));

		bool bFound = false;
		Octree->FindFirstElementWithBoundsTest(QueryBounds, [&](const PCGExOctree::FItem& Item) -> bool
		{
			if (TestPoint(GetOBB(Item.Index), Point, Mode, Expansion))
			{
				OutIndex = Bounds[Item.Index].Index;
				bFound = true;
				return false;
			}
			return true;
		});

		return bFound;
	}

	void FCollection::FindContaining(const FVector& Point, TArray<int32>& OutIndices, EPCGExBoxCheckMode Mode, float Expansion) const
	{
		if (!Octree) return;

		const FBoxCenterAndExtent QueryBounds(Point, FVector4(Expansion, Expansion, Expansion, Expansion));

		Octree->FindElementsWithBoundsTest(QueryBounds, [&](const PCGExOctree::FItem& Item)
		{
			if (TestPoint(GetOBB(Item.Index), Point, Mode, Expansion))
			{
				OutIndices.Add(Bounds[Item.Index].Index);
			}
		});
	}

	bool FCollection::Overlaps(const FOBB& Query, EPCGExBoxCheckMode Mode, float Expansion) const
	{
		if (!Octree) return false;

		const float R = Query.Bounds.Radius + Expansion;
		const FBoxCenterAndExtent QueryBounds(Query.Bounds.Origin, FVector4(R, R, R, R));

		bool bFound = false;
		Octree->FindFirstElementWithBoundsTest(QueryBounds, [&](const PCGExOctree::FItem& Item) -> bool
		{
			if (TestOverlap(GetOBB(Item.Index), Query, Mode, Expansion))
			{
				bFound = true;
				return false;
			}
			return true;
		});

		return bFound;
	}

	bool FCollection::FindFirstOverlap(const FOBB& Query, int32& OutIndex, EPCGExBoxCheckMode Mode, float Expansion) const
	{
		if (!Octree) return false;

		const float R = Query.Bounds.Radius + Expansion;
		const FBoxCenterAndExtent QueryBounds(Query.Bounds.Origin, FVector4(R, R, R, R));

		bool bFound = false;
		Octree->FindFirstElementWithBoundsTest(QueryBounds, [&](const PCGExOctree::FItem& Item) -> bool
		{
			if (TestOverlap(GetOBB(Item.Index), Query, Mode, Expansion))
			{
				OutIndex = Bounds[Item.Index].Index;
				bFound = true;
				return false;
			}
			return true;
		});

		return bFound;
	}

	void FCollection::FindAllOverlaps(const FOBB& Query, TArray<int32>& OutIndices, EPCGExBoxCheckMode Mode, float Expansion) const
	{
		if (!Octree) return;

		const float R = Query.Bounds.Radius + Expansion;
		const FBoxCenterAndExtent QueryBounds(Query.Bounds.Origin, FVector4(R, R, R, R));

		Octree->FindElementsWithBoundsTest(QueryBounds, [&](const PCGExOctree::FItem& Item)
		{
			if (TestOverlap(GetOBB(Item.Index), Query, Mode, Expansion))
			{
				OutIndices.Add(Bounds[Item.Index].Index);
			}
		});
	}

	bool FCollection::FindIntersections(FIntersections& IO) const
	{
		if (!Octree) return false;

		const FBoxCenterAndExtent QueryBounds = IO.GetBounds();

		Octree->FindElementsWithBoundsTest(QueryBounds, [&](const PCGExOctree::FItem& Item)
		{
			ProcessSegment(GetOBB(Item.Index), IO, CloudIndex);
		});

		return !IO.IsEmpty();
	}

	bool FCollection::SegmentIntersectsAny(const FVector& Start, const FVector& End) const
	{
		if (!Octree) return false;

		FBox SegBox(ForceInit);
		SegBox += Start;
		SegBox += End;
		const FBoxCenterAndExtent QueryBounds(SegBox);

		bool bFound = false;
		Octree->FindFirstElementWithBoundsTest(QueryBounds, [&](const PCGExOctree::FItem& Item) -> bool
		{
			if (SegmentIntersects(GetOBB(Item.Index), Start, End))
			{
				bFound = true;
				return false;
			}
			return true;
		});

		return bFound;
	}

	void FCollection::ClassifyPoints(TArrayView<const FVector> Points, TBitArray<>& OutInside, EPCGExBoxCheckMode Mode, float Expansion) const
	{
		const int32 N = Points.Num();
		OutInside.Init(false, N);

		for (int32 i = 0; i < N; i++)
		{
			OutInside[i] = IsPointInside(Points[i], Mode, Expansion);
		}
	}

	void FCollection::FilterInside(TArrayView<const FVector> Points, TArray<int32>& OutIndices, EPCGExBoxCheckMode Mode, float Expansion) const
	{
		const int32 N = Points.Num();
		OutIndices.Reserve(N / 4);

		for (int32 i = 0; i < N; i++)
		{
			if (IsPointInside(Points[i], Mode, Expansion))
			{
				OutIndices.Add(i);
			}
		}
	}
}

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPolyLineIO.h"

#include "PCGContext.h"

#include "PCGExMacros.h"

#include "Data/PCGIntersectionData.h"
#include "Data/PCGSplineData.h"

namespace PCGExData
{
#pragma region FPolyLineIO

	FPolyLineIO::FPolyLineIO(const UPCGPolyLineData& InPolyline)
		: In(&InPolyline)
	{
		Segments.Empty();
		Bounds = FBox(ForceInit);
		BuildCache();
	}

	FPolyLineIO::~FPolyLineIO()
	{
		In = nullptr;
		Segments.Empty();
	}

	PolyLine::FSegment* FPolyLineIO::NearestSegment(const FVector& Location)
	{
		FReadScopeLock ReadLock(SegmentLock);
		PolyLine::FSegment* NearestSegment = nullptr;
		double MinDistanceSquared = TNumericLimits<double>::Max();
		for (PolyLine::FSegment& Segment : Segments)
		{
			FVector ClosestPoint = Segment.NearestLocation(Location);
			const double DistanceSquared = FVector::DistSquared(Location, ClosestPoint);
			if (DistanceSquared < MinDistanceSquared)
			{
				MinDistanceSquared = DistanceSquared;
				NearestSegment = &Segment;
			}
		}
		return NearestSegment;
	}

	PolyLine::FSegment* FPolyLineIO::NearestSegment(const FVector& Location, const double Range)
	{
		FReadScopeLock ReadLock(SegmentLock);
		PolyLine::FSegment* NearestSegment = nullptr;
		double MinDistanceSquared = TNumericLimits<double>::Max();
		for (PolyLine::FSegment& Segment : Segments)
		{
			if (!Segment.Bounds.ExpandBy(Range).IsInside(Location)) { continue; }
			FVector ClosestPoint = Segment.NearestLocation(Location);
			const double DistanceSquared = FVector::DistSquared(Location, ClosestPoint);
			if (DistanceSquared < MinDistanceSquared)
			{
				MinDistanceSquared = DistanceSquared;
				NearestSegment = &Segment;
			}
		}
		return NearestSegment;
	}

	FTransform FPolyLineIO::SampleNearestTransform(const FVector& Location, double& OutTime)
	{
		const PolyLine::FSegment* Segment = NearestSegment(Location);
		const FTransform OutTransform = Segment->NearestTransform(Location);
		OutTime = Segment->GetAccumulatedLengthAt(OutTransform.GetLocation()) / TotalLength;
		return OutTransform;
	}

	bool FPolyLineIO::SampleNearestTransform(const FVector& Location, const double Range, FTransform& OutTransform, double& OutTime)
	{
		if (!Bounds.ExpandBy(Range).IsInside(Location)) { return false; }
		const PolyLine::FSegment* Segment = NearestSegment(Location, Range);
		if (!Segment) { return false; }
		OutTransform = Segment->NearestTransform(Location);
		OutTime = Segment->GetAccumulatedLengthAt(OutTransform.GetLocation()) / TotalLength;
		return true;
	}

	void FPolyLineIO::BuildCache()
	{
		FWriteScopeLock WriteLock(SegmentLock);
		const int32 NumSegments = In->GetNumSegments();
		TotalLength = 0;
		Segments.Reset(NumSegments);
		for (int S = 0; S < NumSegments; S++)
		{
			PolyLine::FSegment& LOD = Segments.Emplace_GetRef(*In, S);
			LOD.AccumulatedLength = TotalLength;
			TotalLength += LOD.Length;
			Bounds += LOD.Bounds;
		}

		TotalClosedLength = TotalLength + FVector::Distance(Segments[0].Start, Segments.Last().End);
	}

#pragma endregion

#pragma region FPolyLineIOGroup

	FPolyLineIOGroup::FPolyLineIOGroup()
	{
	}

	FPolyLineIOGroup::FPolyLineIOGroup(const FPCGContext* Context, const FName InputLabel)
		: FPolyLineIOGroup()
	{
		TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(InputLabel);
		Initialize(Sources);
	}

	FPolyLineIOGroup::FPolyLineIOGroup(TArray<FPCGTaggedData>& Sources)
		: FPolyLineIOGroup()
	{
		Initialize(Sources);
	}

	FPolyLineIOGroup::~FPolyLineIOGroup()
	{
		PCGEX_DELETE_TARRAY(Lines)
	}

	FPolyLineIO* FPolyLineIOGroup::Emplace_GetRef(const FPolyLineIO& PointIO)
	{
		return Emplace_GetRef(PointIO.Source, PointIO.In);
	}

	FPolyLineIO* FPolyLineIOGroup::Emplace_GetRef(const FPCGTaggedData& Source, const UPCGPolyLineData* In)
	{
		FPolyLineIO* Line = Lines.Add_GetRef(new FPolyLineIO(*In));
		Line->Source = Source;
		return Line;
	}

	bool FPolyLineIOGroup::SampleNearestTransform(const FVector& Location, FTransform& OutTransform, double& OutTime)
	{
		double MinDistance = TNumericLimits<double>::Max();
		bool bFound = false;
		for (FPolyLineIO*& Line : Lines)
		{
			FTransform Transform = Line->SampleNearestTransform(Location, OutTime);
			if (const double SqrDist = FVector::DistSquared(Location, Transform.GetLocation());
				SqrDist < MinDistance)
			{
				MinDistance = SqrDist;
				OutTransform = Transform;
				bFound = true;
			}
		}
		return bFound;
	}

	bool FPolyLineIOGroup::SampleNearestTransformWithinRange(const FVector& Location, const double Range, FTransform& OutTransform, double& OutTime)
	{
		double MinDistance = TNumericLimits<double>::Max();
		bool bFound = false;
		for (FPolyLineIO* Line : Lines)
		{
			FTransform Transform;
			if (!Line->SampleNearestTransform(Location, Range, Transform, OutTime)) { continue; }
			if (const double SqrDist = FVector::DistSquared(Location, Transform.GetLocation());
				SqrDist < MinDistance)
			{
				MinDistance = SqrDist;
				OutTransform = Transform;
				bFound = true;
			}
		}
		return bFound;
	}

	UPCGPolyLineData* FPolyLineIOGroup::GetMutablePolyLineData(const UPCGSpatialData* InSpatialData)
	{
		if (!InSpatialData) { return nullptr; }

		if (const UPCGPolyLineData* LineData = Cast<const UPCGPolyLineData>(InSpatialData))
		{
			return const_cast<UPCGPolyLineData*>(LineData);
		}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION == 3
		if (const UPCGSplineProjectionData* SplineProjectionData = Cast<const UPCGSplineProjectionData>(InSpatialData))
		{
			return const_cast<UPCGSplineData*>(SplineProjectionData->GetSpline());
		}
#endif

		if (const UPCGIntersectionData* Intersection = Cast<const UPCGIntersectionData>(InSpatialData))
		{
			if (const UPCGPolyLineData* IntersectionA = GetMutablePolyLineData(Intersection->A))
			{
				return const_cast<UPCGPolyLineData*>(IntersectionA);
			}
			if (const UPCGPolyLineData* IntersectionB = GetMutablePolyLineData(Intersection->B))
			{
				return const_cast<UPCGPolyLineData*>(IntersectionB);
			}
		}

		return nullptr;
	}

	UPCGPolyLineData* FPolyLineIOGroup::GetMutablePolyLineData(const FPCGTaggedData& Source)
	{
		return GetMutablePolyLineData(Cast<UPCGSpatialData>(Source.Data));
	}

	void FPolyLineIOGroup::Initialize(TArray<FPCGTaggedData>& Sources)
	{
		Lines.Empty(Sources.Num());
		for (FPCGTaggedData& Source : Sources)
		{
			const UPCGPolyLineData* MutablePolyLineData = GetMutablePolyLineData(Source);
			if (!MutablePolyLineData || MutablePolyLineData->GetNumSegments() <= 0) { continue; }
			Emplace_GetRef(Source, MutablePolyLineData);
		}
	}

	void FPolyLineIOGroup::Initialize(
		TArray<FPCGTaggedData>& Sources,
		const TFunction<bool(UPCGPolyLineData*)>& ValidateFunc,
		const TFunction<void(FPolyLineIO*)>& PostInitFunc)
	{
		Lines.Empty(Sources.Num());
		for (FPCGTaggedData& Source : Sources)
		{
			UPCGPolyLineData* MutablePolyLineData = GetMutablePolyLineData(Source);
			if (!MutablePolyLineData || MutablePolyLineData->GetNumSegments() == 0) { continue; }
			if (!ValidateFunc(MutablePolyLineData)) { continue; }
			FPolyLineIO* NewPointIO = Emplace_GetRef(Source, MutablePolyLineData);
			PostInitFunc(NewPointIO);
		}
	}

#pragma endregion
}

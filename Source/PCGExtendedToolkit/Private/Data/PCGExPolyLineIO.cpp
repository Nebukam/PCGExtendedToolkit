// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExPolyLineIO.h"

#include "PCGContext.h"
#include "Data/PCGIntersectionData.h"
#include "Data/PCGSplineData.h"

UPCGExPolyLineIO::UPCGExPolyLineIO(): In(nullptr)
{
	Bounds = FBox(EForceInit::ForceInit);
}

PCGExPolyLine::FSegment* UPCGExPolyLineIO::NearestSegment(const FVector& Location)
{
	if (bCacheDirty) { BuildCache(); }
	FReadScopeLock ScopeLock(SegmentLock);
	PCGExPolyLine::FSegment* NearestSegment = nullptr;
	double MinDistanceSquared = DBL_MAX;
	for (PCGExPolyLine::FSegment& Segment : Segments)
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

PCGExPolyLine::FSegment* UPCGExPolyLineIO::NearestSegment(const FVector& Location, const double Range)
{
	if (bCacheDirty) { BuildCache(); }
	FReadScopeLock ScopeLock(SegmentLock);
	PCGExPolyLine::FSegment* NearestSegment = nullptr;
	double MinDistanceSquared = DBL_MAX;
	for (PCGExPolyLine::FSegment& Segment : Segments)
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

FTransform UPCGExPolyLineIO::SampleNearestTransform(const FVector& Location)
{
	if (bCacheDirty) { BuildCache(); }
	const PCGExPolyLine::FSegment* Segment = NearestSegment(Location);
	return Segment->NearestTransform(Location);
}

bool UPCGExPolyLineIO::SampleNearestTransform(const FVector& Location, const double Range, FTransform& OutTransform)
{
	if (!Bounds.ExpandBy(Range).IsInside(Location)) { return false; }
	if (bCacheDirty) { BuildCache(); }
	const PCGExPolyLine::FSegment* Segment = NearestSegment(Location, Range);
	if (!Segment) { return false; }
	OutTransform = Segment->NearestTransform(Location);
	return true;
}

void UPCGExPolyLineIO::BuildCache()
{
	if (!bCacheDirty) { return; }

	FWriteScopeLock ScopeLock(SegmentLock);
	const int32 NumSegments = In->GetNumSegments();
	Segments.Reset(NumSegments);
	for (int S = 0; S < NumSegments; S++)
	{
		const PCGExPolyLine::FSegment& LOD = Segments.Emplace_GetRef(In, S);
		Bounds += LOD.Bounds;
	}

	bCacheDirty = false;
}

UPCGExPolyLineIOGroup::UPCGExPolyLineIOGroup()
{
}

UPCGExPolyLineIOGroup::UPCGExPolyLineIOGroup(const FPCGContext* Context, FName InputLabel)
	: UPCGExPolyLineIOGroup::UPCGExPolyLineIOGroup()
{
	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(InputLabel);
	Initialize(Sources);
}

UPCGExPolyLineIOGroup::UPCGExPolyLineIOGroup(TArray<FPCGTaggedData>& Sources)
	: UPCGExPolyLineIOGroup::UPCGExPolyLineIOGroup()
{
	Initialize(Sources);
}

void UPCGExPolyLineIOGroup::Initialize(TArray<FPCGTaggedData>& Sources)
{
	PolyLines.Empty(Sources.Num());
	for (FPCGTaggedData& Source : Sources)
	{
		UPCGPolyLineData* MutablePolyLineData = GetMutablePolyLineData(Source);
		if (!MutablePolyLineData || MutablePolyLineData->GetNumSegments() == 0) { continue; }
		Emplace_GetRef(Source, MutablePolyLineData);
	}
}

void UPCGExPolyLineIOGroup::Initialize(
	TArray<FPCGTaggedData>& Sources,
	const TFunction<bool(UPCGPolyLineData*)>& ValidateFunc,
	const TFunction<void(UPCGExPolyLineIO*)>& PostInitFunc)
{
	PolyLines.Empty(Sources.Num());
	for (FPCGTaggedData& Source : Sources)
	{
		UPCGPolyLineData* MutablePolyLineData = GetMutablePolyLineData(Source);
		if (!MutablePolyLineData || MutablePolyLineData->GetNumSegments() == 0) { continue; }
		if (!ValidateFunc(MutablePolyLineData)) { continue; }
		UPCGExPolyLineIO* NewPointIO = Emplace_GetRef(Source, MutablePolyLineData);
		PostInitFunc(NewPointIO);
	}
}

UPCGExPolyLineIO* UPCGExPolyLineIOGroup::Emplace_GetRef(const UPCGExPolyLineIO& PointIO)
{
	return Emplace_GetRef(PointIO.Source, PointIO.In);
}

UPCGExPolyLineIO* UPCGExPolyLineIOGroup::Emplace_GetRef(const FPCGTaggedData& Source, UPCGPolyLineData* In)
{
	//FWriteScopeLock ScopeLock(PairsLock);

	UPCGExPolyLineIO* Line = NewObject<UPCGExPolyLineIO>();
	PolyLines.Add(Line);

	Line->Source = Source;
	Line->In = In;

	Line->BuildCache();
	return Line;
}

bool UPCGExPolyLineIOGroup::SampleNearestTransform(const FVector& Location, FTransform& OutTransform)
{
	double MinDistance = DBL_MAX;
	bool bFound = false;
	for (UPCGExPolyLineIO* Line : PolyLines)
	{
		FTransform Transform = Line->SampleNearestTransform(Location);
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

bool UPCGExPolyLineIOGroup::SampleNearestTransformWithinRange(const FVector& Location, const double Range, FTransform& OutTransform)
{
	double MinDistance = DBL_MAX;
	bool bFound = false;
	for (UPCGExPolyLineIO* Line : PolyLines)
	{
		FTransform Transform;
		if (!Line->SampleNearestTransform(Location, Range, Transform)) { continue; }
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

UPCGPolyLineData* UPCGExPolyLineIOGroup::GetMutablePolyLineData(const UPCGSpatialData* InSpatialData)
{
	if (!InSpatialData) { return nullptr; }

	if (const UPCGPolyLineData* LineData = Cast<const UPCGPolyLineData>(InSpatialData))
	{
		return const_cast<UPCGPolyLineData*>(LineData);
	}
	else if (const UPCGSplineProjectionData* SplineProjectionData = Cast<const UPCGSplineProjectionData>(InSpatialData))
	{
		return const_cast<UPCGSplineData*>(SplineProjectionData->GetSpline());
	}
	else if (const UPCGIntersectionData* Intersection = Cast<const UPCGIntersectionData>(InSpatialData))
	{
		if (const UPCGPolyLineData* IntersectionA = GetMutablePolyLineData(Intersection->A))
		{
			return const_cast<UPCGPolyLineData*>(IntersectionA);
		}
		else if (const UPCGPolyLineData* IntersectionB = GetMutablePolyLineData(Intersection->B))
		{
			return const_cast<UPCGPolyLineData*>(IntersectionB);
		}
	}

	return nullptr;
}

UPCGPolyLineData* UPCGExPolyLineIOGroup::GetMutablePolyLineData(const FPCGTaggedData& Source)
{
	return GetMutablePolyLineData(Cast<UPCGSpatialData>(Source.Data));
}

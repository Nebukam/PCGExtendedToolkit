// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExDetailsFusing.h"

#include "Data/PCGExPointElements.h"
#include "Details/PCGExDetailsDistances.h"
#include "Details/PCGExDetailsSettings.h"


FPCGExFuseDetailsBase::FPCGExFuseDetailsBase()
{
	ToleranceInput = EPCGExInputValueType::Constant;
}

FPCGExFuseDetailsBase::FPCGExFuseDetailsBase(const bool InSupportLocalTolerance)
	: bSupportLocalTolerance(InSupportLocalTolerance)
{
	if (!bSupportLocalTolerance) { ToleranceInput = EPCGExInputValueType::Constant; }
}

FPCGExFuseDetailsBase::FPCGExFuseDetailsBase(const bool InSupportLocalTolerance, const double InTolerance)
	: bSupportLocalTolerance(InSupportLocalTolerance), Tolerance(InTolerance)
{
	if (!bSupportLocalTolerance) { ToleranceInput = EPCGExInputValueType::Constant; }
}

bool FPCGExFuseDetailsBase::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade)
{
	if (!bComponentWiseTolerance) { Tolerances = FVector(Tolerance); }

	if (!InDataFacade)
	{
		ToleranceGetter = PCGExDetails::MakeSettingValue<FVector>(Tolerances);
	}
	else
	{
		ToleranceGetter = PCGExDetails::MakeSettingValue<FVector>(ToleranceInput, ToleranceAttribute, Tolerances);
	}

	if (!ToleranceGetter->Init(InDataFacade)) { return false; }

	return true;
}

bool FPCGExFuseDetailsBase::IsWithinTolerance(const double DistSquared, const int32 PointIndex) const
{
	return FMath::IsWithin<double, double>(DistSquared, 0, FMath::Square(ToleranceGetter->Read(PointIndex).X));
}

bool FPCGExFuseDetailsBase::IsWithinTolerance(const FVector& Source, const FVector& Target, const int32 PointIndex) const
{
	return FMath::IsWithin<double, double>(FVector::DistSquared(Source, Target), 0, FMath::Square(ToleranceGetter->Read(PointIndex).X));
}

bool FPCGExFuseDetailsBase::IsWithinToleranceComponentWise(const FVector& Source, const FVector& Target, const int32 PointIndex) const
{
	const FVector CWTolerance = ToleranceGetter->Read(PointIndex);
	return FMath::IsWithin<double, double>(abs(Source.X - Target.X), 0, CWTolerance.X) && FMath::IsWithin<double, double>(abs(Source.Y - Target.Y), 0, CWTolerance.Y) && FMath::IsWithin<double, double>(abs(Source.Z - Target.Z), 0, CWTolerance.Z);
}

FPCGExSourceFuseDetails::FPCGExSourceFuseDetails()
	: FPCGExFuseDetailsBase(false)
{
}

FPCGExSourceFuseDetails::FPCGExSourceFuseDetails(const bool InSupportLocalTolerance)
	: FPCGExFuseDetailsBase(InSupportLocalTolerance)
{
}

FPCGExSourceFuseDetails::FPCGExSourceFuseDetails(const bool InSupportLocalTolerance, const double InTolerance)
	: FPCGExFuseDetailsBase(InSupportLocalTolerance, InTolerance)
{
}

FPCGExSourceFuseDetails::FPCGExSourceFuseDetails(const bool InSupportLocalTolerance, const double InTolerance, const EPCGExDistance SourceMethod)
	: FPCGExFuseDetailsBase(InSupportLocalTolerance, InTolerance), SourceDistance(SourceMethod)
{
}

FPCGExFuseDetails::FPCGExFuseDetails()
	: FPCGExSourceFuseDetails(false)
{
}

FPCGExFuseDetails::FPCGExFuseDetails(const bool InSupportLocalTolerance)
	: FPCGExSourceFuseDetails(InSupportLocalTolerance)
{
}

FPCGExFuseDetails::FPCGExFuseDetails(const bool InSupportLocalTolerance, const double InTolerance)
	: FPCGExSourceFuseDetails(InSupportLocalTolerance, InTolerance)
{
}

FPCGExFuseDetails::FPCGExFuseDetails(const bool InSupportLocalTolerance, const double InTolerance, const EPCGExDistance InSourceMethod)
	: FPCGExSourceFuseDetails(InSupportLocalTolerance, InTolerance, InSourceMethod)
{
}

FPCGExFuseDetails::FPCGExFuseDetails(const bool InSupportLocalTolerance, const double InTolerance, const EPCGExDistance InSourceMethod, const EPCGExDistance InTargetMethod)
	: FPCGExSourceFuseDetails(InSupportLocalTolerance, InTolerance, InSourceMethod), TargetDistance(InTargetMethod)
{
}

bool FPCGExFuseDetails::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade)
{
	if (!FPCGExFuseDetailsBase::Init(InContext, InDataFacade)) { return false; }

	DistanceDetails = PCGExDetails::MakeDistances(SourceDistance, TargetDistance);

	return true;
}

uint64 FPCGExFuseDetails::GetGridKey(const FVector& Location, const int32 PointIndex) const
{
	const FVector Raw = ToleranceGetter->Read(PointIndex);
	return PCGEx::GH3(Location + VoxelGridOffset, FVector(1 / Raw.X, 1 / Raw.Y, 1 / Raw.Z));
}

FBox FPCGExFuseDetails::GetOctreeBox(const FVector& Location, const int32 PointIndex) const
{
	const FVector Extent = ToleranceGetter->Read(PointIndex);
	return FBox(Location - Extent, Location + Extent);
}

void FPCGExFuseDetails::GetCenters(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const
{
	const FVector TargetLocation = TargetPoint.GetTransform().GetLocation();
	OutSource = DistanceDetails->GetSourceCenter(SourcePoint, SourcePoint.GetTransform().GetLocation(), TargetLocation);
	OutTarget = DistanceDetails->GetTargetCenter(TargetPoint, TargetLocation, OutSource);
}

bool FPCGExFuseDetails::IsWithinTolerance(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const
{
	FVector A;
	FVector B;
	GetCenters(SourcePoint, TargetPoint, A, B);
	return FPCGExFuseDetailsBase::IsWithinTolerance(A, B, SourcePoint.Index);
}

bool FPCGExFuseDetails::IsWithinToleranceComponentWise(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const
{
	FVector A;
	FVector B;
	GetCenters(SourcePoint, TargetPoint, A, B);
	return FPCGExFuseDetailsBase::IsWithinToleranceComponentWise(A, B, SourcePoint.Index);
}

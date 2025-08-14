// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExDetailsData.h"

#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExPointIO.h"
#include "PCGExDataMath.h"
#include "Data/PCGExData.h"

namespace PCGExDetails
{
	template <typename T>
	bool TSettingValueBuffer<T>::Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped, const bool bCaptureMinMax)
	{
		FPCGExContext* Context = InDataFacade->GetContext();
		if (!Context) { return false; }

		PCGEX_VALIDATE_NAME_C(Context, Name)

		Buffer = InDataFacade->GetReadable<T>(Name, PCGExData::EIOSide::In, bSupportScoped);

		if (!Buffer)
		{
			if (!this->bQuietErrors) { PCGEX_LOG_INVALID_ATTR_C(Context, Attribute, Name) }
			return false;
		}

		return true;
	}

	template <typename T>
	T TSettingValueBuffer<T>::Read(const int32 Index) { return Buffer->Read(Index); }

	template <typename T>
	T TSettingValueBuffer<T>::Min() { return Buffer->Min; }

	template <typename T>
	T TSettingValueBuffer<T>::Max() { return Buffer->Max; }

	template <typename T>
	bool TSettingValueSelector<T>::Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped, const bool bCaptureMinMax)
	{
		FPCGExContext* Context = InDataFacade->GetContext();
		if (!Context) { return false; }

		Buffer = InDataFacade->GetBroadcaster<T>(Selector, bSupportScoped && !bCaptureMinMax, bCaptureMinMax);

		if (!Buffer)
		{
			if (!this->bQuietErrors) { PCGEX_LOG_INVALID_SELECTOR_C(Context, Selector, Selector) }
			return false;
		}


		return true;
	}

	template <typename T>
	T TSettingValueSelector<T>::Read(const int32 Index) { return Buffer->Read(Index); }

	template <typename T>
	T TSettingValueSelector<T>::Min() { return Buffer->Min; }

	template <typename T>
	T TSettingValueSelector<T>::Max() { return Buffer->Max; }

	template <typename T>
	bool TSettingValueConstant<T>::Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped, const bool bCaptureMinMax) { return true; }

	template <typename T>
	bool TSettingValueSelectorConstant<T>::Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped, const bool bCaptureMinMax)
	{
		FPCGExContext* Context = InDataFacade->GetContext();
		if (!Context) { return false; }

		if (!PCGExDataHelpers::TryReadDataValue(Context, InDataFacade->GetIn(), Selector, this->Constant))
		{
			if (!this->bQuietErrors) { PCGEX_LOG_INVALID_SELECTOR_C(Context, Selector, Selector) }
			return false;
		}

		return true;
	}

	template <typename T>
	bool TSettingValueBufferConstant<T>::Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped, const bool bCaptureMinMax)
	{
		FPCGExContext* Context = InDataFacade->GetContext();
		if (!Context) { return false; }

		PCGEX_VALIDATE_NAME_C(Context, Name)

		if (!PCGExDataHelpers::TryReadDataValue(Context, InDataFacade->GetIn(), Name, this->Constant))
		{
			if (!this->bQuietErrors) { PCGEX_LOG_INVALID_ATTR_C(Context, Attribute, Name) }
			return false;
		}

		return true;
	}

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const T InConstant)
	{
		TSharedPtr<TSettingValueConstant<T>> V = MakeShared<TSettingValueConstant<T>>(InConstant);
		return StaticCastSharedPtr<TSettingValue<T>>(V);
	}

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const T InConstant)
	{
		if (InInput == EPCGExInputValueType::Attribute)
		{
			if (PCGExHelpers::IsDataDomainAttribute(InSelector))
			{
				TSharedPtr<TSettingValueSelectorConstant<T>> V = MakeShared<TSettingValueSelectorConstant<T>>(InSelector);
				return StaticCastSharedPtr<TSettingValue<T>>(V);
			}
			TSharedPtr<TSettingValueSelector<T>> V = MakeShared<TSettingValueSelector<T>>(InSelector);
			return StaticCastSharedPtr<TSettingValue<T>>(V);
		}

		return MakeSettingValue<T>(InConstant);
	}

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const EPCGExInputValueType InInput, const FName InName, const T InConstant)
	{
		if (InInput == EPCGExInputValueType::Attribute)
		{
			if (PCGExHelpers::IsDataDomainAttribute(InName))
			{
				TSharedPtr<TSettingValueBufferConstant<T>> V = MakeShared<TSettingValueBufferConstant<T>>(InName);
				return StaticCastSharedPtr<TSettingValue<T>>(V);
			}
			TSharedPtr<TSettingValueBuffer<T>> V = MakeShared<TSettingValueBuffer<T>>(InName);
			return StaticCastSharedPtr<TSettingValue<T>>(V);
		}

		return MakeSettingValue<T>(InConstant);
	}

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType InInput, const FName InName, const T InConstant)
	{
		T Constant = InConstant;
		PCGExDataHelpers::TryGetSettingDataValue(InContext, InData, InInput, InName, InConstant, Constant);
		return MakeSettingValue<T>(Constant);
	}

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const T InConstant)
	{
		T Constant = InConstant;
		PCGExDataHelpers::TryGetSettingDataValue(InContext, InData, InInput, InSelector, InConstant, Constant);
		return MakeSettingValue<T>(Constant);
	}

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const TSharedPtr<PCGExData::FPointIO> InData, const EPCGExInputValueType InInput, const FName InName, const T InConstant)
	{
		return MakeSettingValue<T>(InData->GetContext(), InData->GetIn(), InInput, InName, InConstant);
	}

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const TSharedPtr<PCGExData::FPointIO> InData, const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const T InConstant)
	{
		return MakeSettingValue<T>(InData->GetContext(), InData->GetIn(), InInput, InSelector, InConstant);
	}

#pragma region externalization

#define PCGEX_TPL(_TYPE, _NAME, ...)\
template class PCGEXTENDEDTOOLKIT_API TSettingValueBuffer<_TYPE>;\
template class PCGEXTENDEDTOOLKIT_API TSettingValueSelector<_TYPE>;\
template class PCGEXTENDEDTOOLKIT_API TSettingValueConstant<_TYPE>;\
template class PCGEXTENDEDTOOLKIT_API TSettingValueSelectorConstant<_TYPE>;\
template class PCGEXTENDEDTOOLKIT_API TSettingValueBufferConstant<_TYPE>;\
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const _TYPE InConstant); \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const _TYPE InConstant); \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const EPCGExInputValueType InInput, const FName InName, const _TYPE InConstant); \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType InInput, const FName InName, const _TYPE InConstant); \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const _TYPE InConstant); \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const TSharedPtr<PCGExData::FPointIO> InData, const EPCGExInputValueType InInput, const FName InName, const _TYPE InConstant); \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const TSharedPtr<PCGExData::FPointIO> InData, const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const _TYPE InConstant);

	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)

#undef PCGEX_TPL

#pragma endregion

	template <EPCGExDistance Source, EPCGExDistance Target>
	FVector TDistances<Source, Target>::GetSourceCenter(const PCGExData::FConstPoint& FromPoint, const FVector& FromCenter, const FVector& ToCenter) const
	{
		return PCGExMath::GetSpatializedCenter<Source>(FromPoint, FromCenter, ToCenter);
	}

	template <EPCGExDistance Source, EPCGExDistance Target>
	FVector TDistances<Source, Target>::GetTargetCenter(const PCGExData::FConstPoint& FromPoint, const FVector& FromCenter, const FVector& ToCenter) const
	{
		return PCGExMath::GetSpatializedCenter<Target>(FromPoint, FromCenter, ToCenter);
	}

	template <EPCGExDistance Source, EPCGExDistance Target>
	void TDistances<Source, Target>::GetCenters(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, FVector& OutSource, FVector& OutTarget) const
	{
		const FVector TargetOrigin = TargetPoint.GetLocation();
		OutSource = PCGExMath::GetSpatializedCenter<Source>(SourcePoint, SourcePoint.GetLocation(), TargetOrigin);
		OutTarget = PCGExMath::GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource);
	}

	template <EPCGExDistance Source, EPCGExDistance Target>
	double TDistances<Source, Target>::GetDistSquared(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const
	{
		const FVector TargetOrigin = TargetPoint.GetLocation();
		const FVector OutSource = PCGExMath::GetSpatializedCenter<Source>(SourcePoint, SourcePoint.GetLocation(), TargetOrigin);
		return FVector::DistSquared(OutSource, PCGExMath::GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource));
	}

	template <EPCGExDistance Source, EPCGExDistance Target>
	double TDistances<Source, Target>::GetDistSquared(const PCGExData::FProxyPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const
	{
		const FVector TargetOrigin = TargetPoint.GetLocation();
		const FVector OutSource = PCGExMath::GetSpatializedCenter<Source>(SourcePoint, SourcePoint.GetLocation(), TargetOrigin);
		return FVector::DistSquared(OutSource, PCGExMath::GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource));
	}

	template <EPCGExDistance Source, EPCGExDistance Target>
	double TDistances<Source, Target>::GetDist(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint) const
	{
		const FVector TargetOrigin = TargetPoint.GetLocation();
		const FVector OutSource = PCGExMath::GetSpatializedCenter<Source>(SourcePoint, SourcePoint.GetLocation(), TargetOrigin);
		return FVector::Dist(OutSource, PCGExMath::GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource));
	}

	template <EPCGExDistance Source, EPCGExDistance Target>
	double TDistances<Source, Target>::GetDistSquared(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, bool& bOverlap) const
	{
		const FVector TargetOrigin = TargetPoint.GetLocation();
		const FVector SourceOrigin = SourcePoint.GetLocation();
		const FVector OutSource = PCGExMath::GetSpatializedCenter<Source>(SourcePoint, SourceOrigin, TargetOrigin);
		const FVector OutTarget = PCGExMath::GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource);

		bOverlap = FVector::DotProduct((TargetOrigin - SourceOrigin), (OutTarget - OutSource)) < 0;
		return FVector::DistSquared(OutSource, OutTarget);
	}

	template <EPCGExDistance Source, EPCGExDistance Target>
	double TDistances<Source, Target>::GetDistSquared(const PCGExData::FProxyPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, bool& bOverlap) const
	{
		const FVector TargetOrigin = TargetPoint.GetLocation();
		const FVector SourceOrigin = SourcePoint.GetLocation();
		const FVector OutSource = PCGExMath::GetSpatializedCenter<Source>(SourcePoint, SourceOrigin, TargetOrigin);
		const FVector OutTarget = PCGExMath::GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource);

		bOverlap = FVector::DotProduct((TargetOrigin - SourceOrigin), (OutTarget - OutSource)) < 0;
		return FVector::DistSquared(OutSource, OutTarget);
	}

	template <EPCGExDistance Source, EPCGExDistance Target>
	double TDistances<Source, Target>::GetDist(const PCGExData::FConstPoint& SourcePoint, const PCGExData::FConstPoint& TargetPoint, bool& bOverlap) const
	{
		const FVector TargetOrigin = TargetPoint.GetLocation();
		const FVector SourceOrigin = SourcePoint.GetLocation();
		const FVector OutSource = PCGExMath::GetSpatializedCenter<Source>(SourcePoint, SourceOrigin, TargetOrigin);
		const FVector OutTarget = PCGExMath::GetSpatializedCenter<Target>(TargetPoint, TargetOrigin, OutSource);

		bOverlap = FVector::DotProduct((TargetOrigin - SourceOrigin), (OutTarget - OutSource)) < 0;
		return FVector::Dist(OutSource, OutTarget);
	}

	template class PCGEXTENDEDTOOLKIT_API TDistances<EPCGExDistance::Center, EPCGExDistance::Center>;
	template class PCGEXTENDEDTOOLKIT_API TDistances<EPCGExDistance::Center, EPCGExDistance::SphereBounds>;
	template class PCGEXTENDEDTOOLKIT_API TDistances<EPCGExDistance::Center, EPCGExDistance::BoxBounds>;
	template class PCGEXTENDEDTOOLKIT_API TDistances<EPCGExDistance::Center, EPCGExDistance::None>;
	template class PCGEXTENDEDTOOLKIT_API TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::Center>;
	template class PCGEXTENDEDTOOLKIT_API TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::SphereBounds>;
	template class PCGEXTENDEDTOOLKIT_API TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::BoxBounds>;
	template class PCGEXTENDEDTOOLKIT_API TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::None>;
	template class PCGEXTENDEDTOOLKIT_API TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::Center>;
	template class PCGEXTENDEDTOOLKIT_API TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::SphereBounds>;
	template class PCGEXTENDEDTOOLKIT_API TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::BoxBounds>;
	template class PCGEXTENDEDTOOLKIT_API TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::None>;
	template class PCGEXTENDEDTOOLKIT_API TDistances<EPCGExDistance::None, EPCGExDistance::Center>;
	template class PCGEXTENDEDTOOLKIT_API TDistances<EPCGExDistance::None, EPCGExDistance::SphereBounds>;
	template class PCGEXTENDEDTOOLKIT_API TDistances<EPCGExDistance::None, EPCGExDistance::BoxBounds>;
	template class PCGEXTENDEDTOOLKIT_API TDistances<EPCGExDistance::None, EPCGExDistance::None>;


	TSharedPtr<FDistances> MakeDistances(const EPCGExDistance Source, const EPCGExDistance Target, const bool bOverlapIsZero)
	{
		if (Source == EPCGExDistance::None || Target == EPCGExDistance::None)
		{
			return MakeShared<TDistances<EPCGExDistance::None, EPCGExDistance::None>>();
		}
		if (Source == EPCGExDistance::Center)
		{
			if (Target == EPCGExDistance::Center) { return MakeShared<TDistances<EPCGExDistance::Center, EPCGExDistance::Center>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::SphereBounds) { return MakeShared<TDistances<EPCGExDistance::Center, EPCGExDistance::SphereBounds>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::BoxBounds) { return MakeShared<TDistances<EPCGExDistance::Center, EPCGExDistance::BoxBounds>>(bOverlapIsZero); }
		}
		else if (Source == EPCGExDistance::SphereBounds)
		{
			if (Target == EPCGExDistance::Center) { return MakeShared<TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::Center>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::SphereBounds) { return MakeShared<TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::SphereBounds>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::BoxBounds) { return MakeShared<TDistances<EPCGExDistance::SphereBounds, EPCGExDistance::BoxBounds>>(bOverlapIsZero); }
		}
		else if (Source == EPCGExDistance::BoxBounds)
		{
			if (Target == EPCGExDistance::Center) { return MakeShared<TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::Center>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::SphereBounds) { return MakeShared<TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::SphereBounds>>(bOverlapIsZero); }
			if (Target == EPCGExDistance::BoxBounds) { return MakeShared<TDistances<EPCGExDistance::BoxBounds, EPCGExDistance::BoxBounds>>(bOverlapIsZero); }
		}

		return nullptr;
	}

	TSharedPtr<FDistances> MakeNoneDistances()
	{
		return MakeShared<TDistances<EPCGExDistance::None, EPCGExDistance::None>>();
	}
}

TSharedPtr<PCGExDetails::FDistances> FPCGExDistanceDetails::MakeDistances() const
{
	return PCGExDetails::MakeDistances(Source, Target);
}

bool FPCGExInfluenceDetails::Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
{
	InfluenceBuffer = GetValueSettingInfluence();
	return InfluenceBuffer->Init(InPointDataFacade, false);
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

bool FPCGExFuseDetails::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade)
{
	if (!FPCGExFuseDetailsBase::Init(InContext, InDataFacade)) { return false; }

	DistanceDetails = PCGExDetails::MakeDistances(SourceDistance, TargetDistance);

	return true;
}

uint32 FPCGExFuseDetails::GetGridKey(const FVector& Location, const int32 PointIndex) const
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

bool FPCGExManhattanDetails::IsValid() const
{
	return bInitialized;
}

bool FPCGExManhattanDetails::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade)
{
	if (bSupportAttribute)
	{
		GridSizeBuffer = GetValueSettingGridSize();
		if (!GridSizeBuffer->Init(InDataFacade)) { return false; }

		if (SpaceAlign == EPCGExManhattanAlign::Custom) { OrientBuffer = GetValueSettingOrient(); }
		else if (SpaceAlign == EPCGExManhattanAlign::World) { OrientBuffer = PCGExDetails::MakeSettingValue(FQuat::Identity); }

		if (OrientBuffer && !OrientBuffer->Init(InDataFacade)) { return false; }
	}
	else
	{
		GridSize = PCGExMath::Abs(GridSize);
		//GridIntSize = FIntVector3(FMath::Floor(GridSize.X), FMath::Floor(GridSize.Y), FMath::Floor(GridSize.Z));
		GridSizeBuffer = PCGExDetails::MakeSettingValue(GridSize);
		if (SpaceAlign == EPCGExManhattanAlign::Custom) { OrientBuffer = PCGExDetails::MakeSettingValue(OrientConstant); }
		else if (SpaceAlign == EPCGExManhattanAlign::World) { OrientBuffer = PCGExDetails::MakeSettingValue(FQuat::Identity); }
	}

	PCGEx::GetAxisOrder(Order, Comps);

	bInitialized = true;
	return true;
}

int32 FPCGExManhattanDetails::ComputeSubdivisions(const FVector& A, const FVector& B, const int32 Index, TArray<FVector>& OutSubdivisions, double& OutDist) const
{
	FVector DirectionAndSize = B - A;
	const int32 StartIndex = OutSubdivisions.Num();

	FQuat Rotation = FQuat::Identity;

	switch (SpaceAlign)
	{
	case EPCGExManhattanAlign::World:
	case EPCGExManhattanAlign::Custom:
		Rotation = OrientBuffer->Read(Index);
		break;
	case EPCGExManhattanAlign::SegmentX:
		Rotation = FRotationMatrix::MakeFromX(DirectionAndSize).ToQuat();
		break;
	case EPCGExManhattanAlign::SegmentY:
		Rotation = FRotationMatrix::MakeFromY(DirectionAndSize).ToQuat();
		break;
	case EPCGExManhattanAlign::SegmentZ:
		Rotation = FRotationMatrix::MakeFromZ(DirectionAndSize).ToQuat();
		break;
	}

	DirectionAndSize = Rotation.RotateVector(DirectionAndSize);

	if (Method == EPCGExManhattanMethod::Simple)
	{
		OutSubdivisions.Reserve(OutSubdivisions.Num() + 3);

		FVector Sub = FVector::ZeroVector;
		for (int i = 0; i < 3; ++i)
		{
			const int32 Axis = Comps[i];
			const double Dist = DirectionAndSize[Axis];

			if (FMath::IsNearlyZero(Dist)) { continue; }

			OutDist += Dist;
			Sub[Axis] = Dist;

			if (Sub == B) { break; }

			OutSubdivisions.Emplace(Sub);
		}
	}
	else
	{
		FVector Subdivs = PCGExMath::Abs(GridSizeBuffer->Read(Index));
		FVector Maxes = PCGExMath::Abs(DirectionAndSize);
		if (Method == EPCGExManhattanMethod::GridCount)
		{
			Subdivs = FVector(
				FMath::Floor(Maxes.X / Subdivs.X),
				FMath::Floor(Maxes.Y / Subdivs.Y),
				FMath::Floor(Maxes.Z / Subdivs.Z));
		}

		const FVector StepSize = FVector::Min(Subdivs, Maxes);
		const FVector Sign = FVector(FMath::Sign(DirectionAndSize.X), FMath::Sign(DirectionAndSize.Y), FMath::Sign(DirectionAndSize.Z));

		FVector Sub = FVector::ZeroVector;

		bool bAdvance = true;
		while (bAdvance)
		{
			double DistBefore = OutDist;
			for (int i = 0; i < 3; ++i)
			{
				const int32 Axis = Comps[i];
				double Dist = StepSize[Axis];

				if (const double SubAbs = FMath::Abs(Sub[Axis]); SubAbs + Dist > Maxes[Axis]) { Dist = Maxes[Axis] - SubAbs; }
				if (FMath::IsNearlyZero(Dist)) { continue; }

				OutDist += Dist;
				Sub[Axis] += Dist * Sign[Axis];

				if (Sub == B)
				{
					bAdvance = false;
					break;
				}

				OutSubdivisions.Emplace(Sub);
			}

			if (DistBefore == OutDist) { bAdvance = false; }
		}
	}

	for (int i = StartIndex; i < OutSubdivisions.Num(); i++) { OutSubdivisions[i] = A + Rotation.UnrotateVector(OutSubdivisions[i]); }

	return OutSubdivisions.Num() - StartIndex;
}

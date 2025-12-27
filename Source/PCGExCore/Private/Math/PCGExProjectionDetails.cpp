// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Math/PCGExProjectionDetails.h"

#include "CoreMinimal.h"
#include "PCGExCoreSettingsCache.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"
#include "Async/ParallelFor.h"
#include "Helpers/PCGExArrayHelpers.h"
#include "Math/PCGExBestFitPlane.h"

FPCGExGeo2DProjectionDetails::FPCGExGeo2DProjectionDetails()
{
	WorldUp = PCGEX_CORE_SETTINGS.WorldUp;
	WorldFwd = PCGEX_CORE_SETTINGS.WorldForward;
	ProjectionNormal = WorldUp;
}

FPCGExGeo2DProjectionDetails::FPCGExGeo2DProjectionDetails(const bool InSupportLocalNormal)
	: bSupportLocalNormal(InSupportLocalNormal)
{
	WorldUp = PCGEX_CORE_SETTINGS.WorldUp;
	ProjectionNormal = WorldUp;
}

bool FPCGExGeo2DProjectionDetails::Init(const TSharedPtr<PCGExData::FFacade>& PointDataFacade)
{
	FPCGExContext* Context = PointDataFacade->GetContext();
	if (!Context) { return false; }

	ProjectionNormal = ProjectionNormal.GetSafeNormal(1E-08, WorldUp);
	ProjectionQuat = FRotationMatrix::MakeFromZX(ProjectionNormal, WorldFwd).ToQuat();

	if (!bSupportLocalNormal) { bLocalProjectionNormal = false; }
	if (bLocalProjectionNormal && PointDataFacade)
	{
		NormalGetter = PCGExDetails::MakeSettingValue<FVector>(EPCGExInputValueType::Attribute, LocalNormal, ProjectionNormal);
		if (!NormalGetter->Init(PointDataFacade, false, false))
		{
			NormalGetter = nullptr;
			return false;
		}
	}

	return true;
}

bool FPCGExGeo2DProjectionDetails::Init(const TSharedPtr<PCGExData::FPointIO>& PointIO)
{
	FPCGExContext* Context = PointIO->GetContext();
	if (!Context) { return false; }

	ProjectionNormal = ProjectionNormal.GetSafeNormal(1E-08, WorldUp);
	ProjectionQuat = FRotationMatrix::MakeFromZX(ProjectionNormal, WorldFwd).ToQuat();

	if (!bSupportLocalNormal) { bLocalProjectionNormal = false; }
	if (bLocalProjectionNormal)
	{
		if (!PCGExMetaHelpers::IsDataDomainAttribute(LocalNormal))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Only @Data domain attributes are supported for local projection."));
		}
		else
		{
			NormalGetter = PCGExDetails::MakeSettingValue<FVector>(PointIO, EPCGExInputValueType::Attribute, LocalNormal, ProjectionNormal);
		}

		if (!NormalGetter) { return false; }
	}

	return true;
}

bool FPCGExGeo2DProjectionDetails::Init(const UPCGData* InData)
{
	ProjectionNormal = ProjectionNormal.GetSafeNormal(1E-08, WorldUp);
	ProjectionQuat = FRotationMatrix::MakeFromZX(ProjectionNormal, WorldFwd).ToQuat();

	if (!bSupportLocalNormal) { bLocalProjectionNormal = false; }
	if (bLocalProjectionNormal)
	{
		if (!PCGExMetaHelpers::IsDataDomainAttribute(LocalNormal))
		{
			UE_LOG(LogTemp, Warning, TEXT("Only @Data domain attributes are supported for local projection."));
		}
		else
		{
			NormalGetter = PCGExDetails::MakeSettingValue<FVector>(nullptr, InData, EPCGExInputValueType::Attribute, LocalNormal, ProjectionNormal);
		}

		if (!NormalGetter) { return false; }
	}

	return true;
}

void FPCGExGeo2DProjectionDetails::Init(const PCGExMath::FBestFitPlane& InFitPlane)
{
	ProjectionNormal = InFitPlane.Normal();
	ProjectionQuat = FRotationMatrix::MakeFromZX(ProjectionNormal, WorldFwd).ToQuat();
}

#define PCGEX_READ_QUAT(_INDEX) FRotationMatrix::MakeFromZX(NormalGetter->Read(_INDEX).GetSafeNormal(1E-08, FVector::UpVector), WorldFwd).ToQuat()

FQuat FPCGExGeo2DProjectionDetails::GetQuat(const int32 PointIndex) const
{
	return NormalGetter ? PCGEX_READ_QUAT(PointIndex) : ProjectionQuat;
}

FTransform FPCGExGeo2DProjectionDetails::Project(const FTransform& InTransform, const int32 PointIndex) const
{
	const FQuat QInv = GetQuat(PointIndex).Inverse();

	return FTransform(
		QInv * InTransform.GetRotation(),
		QInv.RotateVector(InTransform.GetLocation()),
		InTransform.GetScale3D()
	);
}

void FPCGExGeo2DProjectionDetails::ProjectInPlace(FTransform& InTransform, const int32 PointIndex) const
{
	const FQuat QInv = GetQuat(PointIndex).Inverse();
	InTransform.SetRotation(QInv * InTransform.GetRotation());
	InTransform.SetLocation(QInv.RotateVector(InTransform.GetLocation()));
}

FVector FPCGExGeo2DProjectionDetails::Project(const FVector& InPosition, const int32 PointIndex) const
{
	return GetQuat(PointIndex).UnrotateVector(InPosition);
}

FVector FPCGExGeo2DProjectionDetails::Project(const FVector& InPosition) const
{
	return ProjectionQuat.UnrotateVector(InPosition);
}

FVector FPCGExGeo2DProjectionDetails::ProjectFlat(const FVector& InPosition) const
{
	FVector RotatedPosition = ProjectionQuat.UnrotateVector(InPosition);
	RotatedPosition.Z = 0;
	return RotatedPosition;
}

FVector FPCGExGeo2DProjectionDetails::ProjectFlat(const FVector& InPosition, const int32 PointIndex) const
{
	//
	FVector RotatedPosition = GetQuat(PointIndex).UnrotateVector(InPosition);
	RotatedPosition.Z = 0;
	return RotatedPosition;
}

FTransform FPCGExGeo2DProjectionDetails::ProjectFlat(const FTransform& InTransform) const
{
	FVector Position = ProjectionQuat.UnrotateVector(InTransform.GetLocation());
	Position.Z = 0;
	const FQuat Quat = InTransform.GetRotation();
	return FTransform(Quat * ProjectionQuat, Position);
}

FTransform FPCGExGeo2DProjectionDetails::ProjectFlat(const FTransform& InTransform, const int32 PointIndex) const
{
	const FQuat Q = GetQuat(PointIndex);
	FVector Position = Q.UnrotateVector(InTransform.GetLocation());
	Position.Z = 0;
	const FQuat Quat = InTransform.GetRotation();
	return FTransform(Quat * Q, Position);
}

template <typename T>
void FPCGExGeo2DProjectionDetails::ProjectFlat(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<T>& OutPositions) const
{
	const TConstPCGValueRange<FTransform> Transforms = InFacade->Source->GetInOut()->GetConstTransformValueRange();
	const int32 NumVectors = Transforms.Num();
	PCGExArrayHelpers::InitArray(OutPositions, NumVectors);
	PCGEX_PARALLEL_FOR(
		NumVectors,
		OutPositions[i] = T(ProjectFlat(Transforms[i].GetLocation(), i));
	)
}

template PCGEXCORE_API void FPCGExGeo2DProjectionDetails::ProjectFlat<FVector2D>(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<FVector2D>& OutPositions) const;
template PCGEXCORE_API void FPCGExGeo2DProjectionDetails::ProjectFlat<FVector>(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<FVector>& OutPositions) const;
template PCGEXCORE_API void FPCGExGeo2DProjectionDetails::ProjectFlat<FVector4>(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<FVector4>& OutPositions) const;

void FPCGExGeo2DProjectionDetails::Project(const TArray<FVector>& InPositions, TArray<FVector>& OutPositions) const
{
	const int32 NumVectors = InPositions.Num();
	PCGExArrayHelpers::InitArray(OutPositions, NumVectors);

	if (NormalGetter)
	{
		PCGEX_PARALLEL_FOR(
			NumVectors,
			OutPositions[i] = PCGEX_READ_QUAT(i).UnrotateVector(InPositions[i]);
		)
	}
	else
	{
		PCGEX_PARALLEL_FOR(
			NumVectors,
			OutPositions[i] = ProjectionQuat.UnrotateVector(InPositions[i]);
		)
	}
}

void FPCGExGeo2DProjectionDetails::Project(const TArrayView<FVector>& InPositions, TArray<FVector2D>& OutPositions) const
{
	const int32 NumVectors = InPositions.Num();
	PCGExArrayHelpers::InitArray(OutPositions, NumVectors);
	PCGEX_PARALLEL_FOR(
		NumVectors,
		OutPositions[i] = FVector2D(ProjectionQuat.UnrotateVector(InPositions[i]));
	)
}

void FPCGExGeo2DProjectionDetails::Project(const TConstPCGValueRange<FTransform>& InTransforms, TArray<FVector2D>& OutPositions) const
{
	const int32 NumVectors = InTransforms.Num();
	PCGExArrayHelpers::InitArray(OutPositions, NumVectors);
	PCGEX_PARALLEL_FOR(
		NumVectors,
		OutPositions[i] = FVector2D(ProjectionQuat.UnrotateVector(InTransforms[i].GetLocation()));
	)
}

void FPCGExGeo2DProjectionDetails::Project(const TArrayView<FVector>& InPositions, std::vector<double>& OutPositions) const
{
	PCGEX_PARALLEL_FOR(
		InPositions.Num(),
		const FVector PP = ProjectionQuat.UnrotateVector(InPositions[i]);
		const int32 ii = i * 2;
		OutPositions[ii] = PP.X;
		OutPositions[ii+1] = PP.Y;
	)
}

void FPCGExGeo2DProjectionDetails::Project(const TConstPCGValueRange<FTransform>& InTransforms, std::vector<double>& OutPositions) const
{
	PCGEX_PARALLEL_FOR(
		InTransforms.Num(),
		const FVector PP = ProjectionQuat.UnrotateVector(InTransforms[i].GetLocation());
		const int32 ii = i * 2;
		OutPositions[ii] = PP.X;
		OutPositions[ii+1] = PP.Y;
	)
}

FTransform FPCGExGeo2DProjectionDetails::Restore(const FTransform& InTransform, const int32 PointIndex) const
{
	const FQuat Q = GetQuat(PointIndex);

	return FTransform(
		Q * InTransform.GetRotation(),
		Q.RotateVector(InTransform.GetLocation()),
		InTransform.GetScale3D()
	);
}

void FPCGExGeo2DProjectionDetails::RestoreInPlace(FTransform& InTransform, const int32 PointIndex) const
{
	const FQuat Q = GetQuat(PointIndex);
	InTransform.SetRotation(Q * InTransform.GetRotation());
	InTransform.SetLocation(Q.RotateVector(InTransform.GetLocation()));
}

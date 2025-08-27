// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Geometry/PCGExGeo.h"

#include "CoreMinimal.h"
#include "MinVolumeBox3.h"
#include "PCGEx.h"
#include "PCGExDetailsData.h"
#include "PCGExMath.h"
#include "PCGExMT.h"
#include "Transform/PCGExFitting.h"
#include "Curve/CurveUtil.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "EntitySystem/MovieSceneEntityManager.h"

namespace PCGExGeo
{
	bool IsWinded(const EPCGExWinding Winding, const bool bIsInputClockwise)
	{
		if (Winding == EPCGExWinding::Clockwise) { return bIsInputClockwise; }
		return !bIsInputClockwise;
	}

	bool IsWinded(const EPCGExWindingMutation Winding, const bool bIsInputClockwise)
	{
		if (Winding == EPCGExWindingMutation::Clockwise) { return bIsInputClockwise; }
		return !bIsInputClockwise;
	}

	FPolygonInfos::FPolygonInfos(const TArray<FVector2D>& InPolygon)
	{
		Area = UE::Geometry::CurveUtil::SignedArea2<double, FVector2D>(InPolygon);
		Perimeter = UE::Geometry::CurveUtil::ArcLength<double, FVector2D>(InPolygon, true);

		if (Area < 0)
		{
			bIsClockwise = true;
			Area = FMath::Abs(Area);
		}
		else
		{
			bIsClockwise = false;
		}

		if (Perimeter == 0.0f) { Compactness = 0; }
		else { Compactness = (4.0f * PI * Area) / (Perimeter * Perimeter); }
	}

	bool FPolygonInfos::IsWinded(const EPCGExWinding Winding) const { return PCGExGeo::IsWinded(Winding, bIsClockwise); }

	bool FindSphereFrom4Points(const FVector& A, const FVector& B, const FVector& C, const FVector& D, FSphere& OutSphere)
	{
		//Shamelessly stolen from https://stackoverflow.com/questions/37449046/how-to-calculate-the-sphere-center-with-4-points

		const double U = S_U(A, B, C, D, B, C, D, A);
		const double V = S_U(C, D, A, B, D, A, B, C);
		const double W = S_U(A, C, D, B, B, D, A, C);
		const double UVW = 2 * (U + V + W);

		if (UVW == 0.0) { return false; } // Coplanar

		constexpr int C_X = 0;
		constexpr int C_Y = 1;
		constexpr int C_Z = 2;
		const double RA = S_SQ(A);
		const double RB = S_SQ(B);
		const double RC = S_SQ(C);
		const double RD = S_SQ(D);

		const FVector Center = FVector(
			S_E(C_Y, C_Z, A, B, C, D, RA, RB, RC, RD, UVW),
			S_E(C_Z, C_X, A, B, C, D, RA, RB, RC, RD, UVW),
			S_E(C_X, C_Y, A, B, C, D, RA, RB, RC, RD, UVW));

		const double radius = FMath::Sqrt(S_SQ(FVector(A - Center)));

		OutSphere = FSphere(Center, radius);
		return true;
	}

	bool FindSphereFrom4Points(const TArrayView<FVector>& Positions, const int32 (&Vtx)[4], FSphere& OutSphere)
	{
		return FindSphereFrom4Points(
			Positions[Vtx[0]],
			Positions[Vtx[1]],
			Positions[Vtx[2]],
			Positions[Vtx[3]],
			OutSphere);
	}

	void GetCircumcenter(const TArrayView<FVector>& Positions, const int32 (&Vtx)[3], FVector& OutCircumcenter)
	{
		// Calculate midpoints of two sides
		const FVector& A = Positions[Vtx[0]];
		const FVector& B = Positions[Vtx[1]];
		const FVector& C = Positions[Vtx[2]];


		const FVector AC = C - A;
		const FVector AB = B - A;
		const FVector ABxAC = AB.Cross(AC);

		// this is the vector from a TO the circumsphere center
		const FVector ToCircumsphereCenter = (ABxAC.Cross(AB) * AC.SquaredLength() + AC.Cross(ABxAC) * AB.SquaredLength()) / (2 * ABxAC.SquaredLength());
		//float Radius = ToCircumsphereCenter.Length();

		// The 3 space coords of the circumsphere center then:
		OutCircumcenter = A + ToCircumsphereCenter;
	}

	void GetCentroid(const TArrayView<FVector>& Positions, const int32 (&Vtx)[4], FVector& OutCentroid)
	{
		OutCentroid = FVector::ZeroVector;
		for (int i = 0; i < 4; i++) { OutCentroid += Positions[Vtx[i]]; }
		OutCentroid /= 4;
	}

	void GetCentroid(const TArrayView<FVector>& Positions, const int32 (&Vtx)[3], FVector& OutCentroid)
	{
		OutCentroid = FVector::ZeroVector;
		for (int i = 0; i < 3; i++) { OutCentroid += Positions[Vtx[i]]; }
		OutCentroid /= 3;
	}

	void GetLongestEdge(const TArrayView<FVector>& Positions, const int32 (&Vtx)[3], uint64& Edge)
	{
		double Dist = 0;
		for (int i = 0; i < 3; i++)
		{
			for (int j = i + 1; j < 3; j++)
			{
				const double LocalDist = FVector::DistSquared(Positions[Vtx[i]], Positions[Vtx[j]]);
				if (LocalDist > Dist)
				{
					Dist = LocalDist;
					Edge = PCGEx::H64U(Vtx[i], Vtx[j]);
				}
			}
		}
	}

	void GetLongestEdge(const TArrayView<FVector>& Positions, const int32 (&Vtx)[4], uint64& Edge)
	{
		double Dist = 0;
		for (int i = 0; i < 4; i++)
		{
			for (int j = i + 1; j < 4; j++)
			{
				const double LocalDist = FVector::DistSquared(Positions[Vtx[i]], Positions[Vtx[j]]);
				if (LocalDist > Dist)
				{
					Dist = LocalDist;
					Edge = PCGEx::H64U(Vtx[i], Vtx[j]);
				}
			}
		}
	}

	void PointsToPositions(const UPCGBasePointData* InPointData, TArray<FVector>& OutPositions)
	{
		const int32 NumPoints = InPointData->GetNumPoints();
		const TConstPCGValueRange<FTransform> Transforms = InPointData->GetConstTransformValueRange();
		PCGEx::InitArray(OutPositions, NumPoints);
		for (int i = 0; i < NumPoints; i++) { OutPositions[i] = Transforms[i].GetLocation(); }
	}

	FVector GetBarycentricCoordinates(const FVector& Point, const FVector& A, const FVector& B, const FVector& C)
	{
		const FVector AB = B - A;
		const FVector AC = C - A;
		const FVector AD = Point - A;

		const double d00 = FVector::DotProduct(AB, AB);
		const double d01 = FVector::DotProduct(AB, AC);
		const double d11 = FVector::DotProduct(AC, AC);
		const double d20 = FVector::DotProduct(AD, AB);
		const double d21 = FVector::DotProduct(AD, AC);

		const double Den = d00 * d11 - d01 * d01;
		const double V = (d11 * d20 - d01 * d21) / Den;
		const double W = (d00 * d21 - d01 * d20) / Den;
		const double U = 1.0f - V - W;

		return FVector(U, V, W);
	}

	bool IsPointInTriangle(const FVector& P, const FVector& A, const FVector& B, const FVector& C)
	{
		const FVector& D = FVector::CrossProduct(B - A, P - A);
		return (FVector::DotProduct(D, FVector::CrossProduct(C - B, P - B)) >= 0) &&
			(FVector::DotProduct(D, FVector::CrossProduct(A - C, P - C)) >= 0);
	}

	FApex::FApex(const FVector& Start, const FVector& End, const FVector& InApex)
	{
		Direction = (Start - End).GetSafeNormal();
		Anchor = FMath::ClosestPointOnSegment(InApex, Start, End);

		const double DistToStart = FVector::Dist(Start, Anchor);
		const double DistToEnd = FVector::Dist(End, Anchor);
		TowardStart = Direction * (DistToStart * -1);
		TowardEnd = Direction * DistToEnd;
		Alpha = DistToStart / (DistToStart + DistToEnd);
	}

	void FApex::Scale(const double InScale)
	{
		TowardStart *= InScale;
		TowardEnd *= InScale;
	}

	void FApex::Extend(const double InSize)
	{
		TowardStart += Direction * InSize;
		TowardEnd += Direction * -InSize;
	}

	FExCenterArc::FExCenterArc(const FVector& A, const FVector& B, const FVector& C)
	{
		const FVector Up = PCGExMath::GetNormal(A, B, C);
		bool bIntersect = true;

		Center = PCGExMath::SafeLinePlaneIntersection(
			C, C + PCGExMath::GetNormal(B, C, C + Up),
			A, (A - B).GetSafeNormal(), bIntersect);

		if (!bIntersect) { Center = FMath::Lerp(A, C, 0.5); } // Parallel lines, place center right in the middle

		Radius = FVector::Dist(C, Center);

		Hand = (A - Center).GetSafeNormal();
		OtherHand = (C - Center).GetSafeNormal();

		bIsLine = FMath::IsNearlyEqual(FMath::Abs(FVector::DotProduct(Hand, OtherHand)), 1);

		Normal = FVector::CrossProduct(Hand, OtherHand).GetSafeNormal();
		Theta = FMath::Acos(FVector::DotProduct(Hand, OtherHand));
		SinTheta = FMath::Sin(Theta);
	}

	FExCenterArc::FExCenterArc(const FVector& A1, const FVector& B1, const FVector& A2, const FVector& B2, const double MaxLength)
	{
		const FVector& N1 = PCGExMath::GetNormal(B1, A1, A1 + PCGExMath::GetNormal(B1, A1, A2));
		const FVector& N2 = PCGExMath::GetNormal(B2, A2, A2 + PCGExMath::GetNormal(B2, A2, A1));

		if (FMath::IsNearlyZero(FVector::DotProduct(N1, N2)))
		{
			Center = FMath::Lerp(B1, B2, 0.5);
		}
		else
		{
			FVector OutA = FVector::ZeroVector;
			FVector OutB = FVector::ZeroVector;
			FMath::SegmentDistToSegment(
				B1 + N1 * -MaxLength, B1 + N1 * MaxLength,
				B2 + N2 * -MaxLength, B2 + N2 * MaxLength,
				OutA, OutB);
			Center = FMath::Lerp(OutA, OutB, 0.5);
		}

		Radius = FVector::Dist(A2, Center);

		Hand = (B1 - Center).GetSafeNormal();
		OtherHand = (B2 - Center).GetSafeNormal();

		Normal = FVector::CrossProduct(Hand, OtherHand).GetSafeNormal();
		Theta = FMath::Acos(FVector::DotProduct(Hand, OtherHand));
		SinTheta = FMath::Sin(Theta);
	}

	FVector FExCenterArc::GetLocationOnArc(const double Alpha) const
	{
		const double W1 = FMath::Sin((1.0 - Alpha) * Theta) / SinTheta;
		const double W2 = FMath::Sin(Alpha * Theta) / SinTheta;

		const FVector Dir = Hand * W1 + OtherHand * W2;
		return Center + (Dir * Radius);
	}

	FBestFitPlane::FBestFitPlane(const TConstPCGValueRange<FTransform>& InTransforms)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		UE::Geometry::FOrientedBox3d OrientedBox{};
		UE::Geometry::TMinVolumeBox3<double> Box;

		Centroid = FVector::ZeroVector;

		Box.Solve(
			InTransforms.Num(), [&](int32 i)
			{
				const FVector P = InTransforms[i].GetLocation();
				Centroid += P;
				return P;
			});

		Centroid /= InTransforms.Num();

		if (Box.IsSolutionAvailable())
		{
			Box.GetResult(OrientedBox);
			ProcessBox(OrientedBox);
		}
	}

	FBestFitPlane::FBestFitPlane(const TConstPCGValueRange<FTransform>& InTransforms, const TArrayView<int32> InIndices)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		UE::Geometry::FOrientedBox3d OrientedBox{};
		UE::Geometry::TMinVolumeBox3<double> Box;

		Centroid = FVector::ZeroVector;

		Box.Solve(
			InIndices.Num(), [&](int32 i)
			{
				const FVector P = InTransforms[InIndices[i]].GetLocation();
				Centroid += P;
				return P;
			});

		Centroid /= InTransforms.Num();

		if (Box.IsSolutionAvailable())
		{
			Box.GetResult(OrientedBox);
			ProcessBox(OrientedBox);
		}
	}

	FBestFitPlane::FBestFitPlane(const TArrayView<const FVector> InPositions)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		UE::Geometry::FOrientedBox3d OrientedBox{};
		UE::Geometry::TMinVolumeBox3<double> Box;

		Centroid = FVector::ZeroVector;

		Box.Solve(
			InPositions.Num(), [&](int32 i)
			{
				const FVector P = InPositions[i];
				Centroid += P;
				return P;
			});

		Centroid /= InPositions.Num();

		if (Box.IsSolutionAvailable())
		{
			Box.GetResult(OrientedBox);
			ProcessBox(OrientedBox);
		}
	}

	FBestFitPlane::FBestFitPlane(const TArrayView<const FVector2D> InPositions)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		UE::Geometry::FOrientedBox3d OrientedBox{};
		UE::Geometry::TMinVolumeBox3<double> Box;

		Centroid = FVector::ZeroVector;

		Box.Solve(
			InPositions.Num(), [&](int32 i)
			{
				const FVector P = FVector(InPositions[i], 0);
				Centroid += P;
				return P;
			});

		Centroid /= InPositions.Num();

		if (Box.IsSolutionAvailable())
		{
			Box.GetResult(OrientedBox);
			ProcessBox(OrientedBox);
		}
	}

	FVector FBestFitPlane::Normal() const { return Axis[2]; }

	FTransform FBestFitPlane::GetTransform() const
	{
		FTransform Transform = FTransform(FMatrix(Axis[0], Axis[1], Axis[2], FVector::Zero()));
		Transform.SetLocation(Centroid);
		Transform.SetScale3D(FVector::OneVector);
		return Transform;
	}

	FTransform FBestFitPlane::GetTransform(const EPCGExAxisOrder Order) const
	{
		int32 Comps[3] = {0, 0, 0};
		PCGEx::GetAxisOrder(Order, Comps);

		FTransform Transform = FTransform(FMatrix(Axis[Comps[0]], Axis[Comps[1]], Axis[Comps[2]], FVector::Zero()));
		Transform.SetLocation(Centroid);
		Transform.SetScale3D(FVector::OneVector);
		return Transform;
	}

	void FBestFitPlane::ProcessBox(const UE::Geometry::FOrientedBox3d& Box)
	{
		Centroid = Box.Center();

		Algo::Sort(Swizzle, [&](const int32 A, const int32 B) { return Box.Extents[A] > Box.Extents[B]; });

		Extents[0] = Box.Extents[Swizzle[0]];
		Extents[1] = Box.Extents[Swizzle[1]];
		Extents[2] = Box.Extents[Swizzle[2]];

		// Pick raw axes
		FVector X = Box.Frame.GetAxis(Swizzle[0]); // Longest
		FVector Y = Box.Frame.GetAxis(Swizzle[1]); // Median
		FVector Z = Box.Frame.GetAxis(Swizzle[2]); // Smallest

		// Re-orthogonalize using cross product to avoid flip
		// Ensure right-handed system
		Z = FVector::CrossProduct(X, Y).GetSafeNormal();
		Y = FVector::CrossProduct(Z, X).GetSafeNormal();

		// Make sure Z points upward
		if (FVector::DotProduct(Z, FVector::UpVector) < 0) { Z *= -1; }

		Axis[0] = X.GetSafeNormal();
		Axis[1] = Y.GetSafeNormal();
		Axis[2] = Z.GetSafeNormal();
	}
}

bool FPCGExGeo2DProjectionDetails::Init(const TSharedPtr<PCGExData::FFacade>& PointDataFacade)
{
	FPCGExContext* Context = PointDataFacade->GetContext();
	if (!Context) { return false; }

	ProjectionNormal = ProjectionNormal.GetSafeNormal(1E-08, FVector::UpVector);
	ProjectionQuat = FRotationMatrix::MakeFromZX(ProjectionNormal, FVector::ForwardVector).ToQuat();

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

	ProjectionNormal = ProjectionNormal.GetSafeNormal(1E-08, FVector::UpVector);
	ProjectionQuat = FRotationMatrix::MakeFromZX(ProjectionNormal, FVector::ForwardVector).ToQuat();

	if (!bSupportLocalNormal) { bLocalProjectionNormal = false; }
	if (bLocalProjectionNormal)
	{
		if (!PCGExHelpers::IsDataDomainAttribute(LocalNormal))
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
	ProjectionNormal = ProjectionNormal.GetSafeNormal(1E-08, FVector::UpVector);
	ProjectionQuat = FRotationMatrix::MakeFromZX(ProjectionNormal, FVector::ForwardVector).ToQuat();

	if (!bSupportLocalNormal) { bLocalProjectionNormal = false; }
	if (bLocalProjectionNormal)
	{
		if (!PCGExHelpers::IsDataDomainAttribute(LocalNormal))
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

void FPCGExGeo2DProjectionDetails::Init(const PCGExGeo::FBestFitPlane& InFitPlane)
{
	ProjectionNormal = InFitPlane.Normal();
	ProjectionQuat = FRotationMatrix::MakeFromZX(ProjectionNormal, FVector::ForwardVector).ToQuat();
}

#define PCGEX_READ_QUAT(_INDEX) FRotationMatrix::MakeFromZX(NormalGetter->Read(_INDEX).GetSafeNormal(1E-08, FVector::UpVector), FVector::ForwardVector).ToQuat()

FQuat FPCGExGeo2DProjectionDetails::GetQuat(const int32 PointIndex) const
{
	return NormalGetter ? PCGEX_READ_QUAT(PointIndex) : ProjectionQuat;
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
	PCGEx::InitArray(OutPositions, NumVectors);
	for (int i = 0; i < NumVectors; i++) { OutPositions[i] = T(ProjectFlat(Transforms[i].GetLocation(), i)); }
}

template <typename T>
void FPCGExGeo2DProjectionDetails::ProjectFlat(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<T>& OutPositions, const PCGExMT::FScope& Scope) const
{
	const TConstPCGValueRange<FTransform> Transforms = InFacade->Source->GetInOut()->GetConstTransformValueRange();
	const int32 NumVectors = Transforms.Num();
	if (OutPositions.Num() < NumVectors) { PCGEx::InitArray(OutPositions, NumVectors); }

	PCGEX_SCOPE_LOOP(i) { OutPositions[i] = T(ProjectFlat(Transforms[i].GetLocation(), i)); }
}

template PCGEXTENDEDTOOLKIT_API void FPCGExGeo2DProjectionDetails::ProjectFlat<FVector2D>(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<FVector2D>& OutPositions) const;
template PCGEXTENDEDTOOLKIT_API void FPCGExGeo2DProjectionDetails::ProjectFlat<FVector>(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<FVector>& OutPositions) const;
template PCGEXTENDEDTOOLKIT_API void FPCGExGeo2DProjectionDetails::ProjectFlat<FVector4>(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<FVector4>& OutPositions) const;
template PCGEXTENDEDTOOLKIT_API void FPCGExGeo2DProjectionDetails::ProjectFlat<FVector2D>(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<FVector2D>& OutPositions, const PCGExMT::FScope& Scope) const;
template PCGEXTENDEDTOOLKIT_API void FPCGExGeo2DProjectionDetails::ProjectFlat<FVector>(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<FVector>& OutPositions, const PCGExMT::FScope& Scope) const;
template PCGEXTENDEDTOOLKIT_API void FPCGExGeo2DProjectionDetails::ProjectFlat<FVector4>(const TSharedPtr<PCGExData::FFacade>& InFacade, TArray<FVector4>& OutPositions, const PCGExMT::FScope& Scope) const;

void FPCGExGeo2DProjectionDetails::Project(const TArray<FVector>& InPositions, TArray<FVector>& OutPositions) const
{
	const int32 NumVectors = InPositions.Num();
	PCGEx::InitArray(OutPositions, NumVectors);

	if (NormalGetter)
	{
		for (int i = 0; i < NumVectors; i++) { OutPositions[i] = PCGEX_READ_QUAT(i).UnrotateVector(InPositions[i]); }
	}
	else
	{
		for (int i = 0; i < NumVectors; i++) { OutPositions[i] = ProjectionQuat.UnrotateVector(InPositions[i]); }
	}
}

void FPCGExGeo2DProjectionDetails::Project(const TArrayView<FVector>& InPositions, TArray<FVector2D>& OutPositions) const
{
	const int32 NumVectors = InPositions.Num();
	PCGEx::InitArray(OutPositions, NumVectors);
	for (int i = 0; i < NumVectors; i++) { OutPositions[i] = FVector2D(ProjectionQuat.UnrotateVector(InPositions[i])); }
}

void FPCGExGeo2DProjectionDetails::Project(const TArrayView<FVector>& InPositions, std::vector<double>& OutPositions) const
{
	const int32 NumVectors = InPositions.Num();
	int32 p = 0;
	for (int i = 0; i < NumVectors; i++)
	{
		const FVector PP = ProjectionQuat.UnrotateVector(InPositions[i]);
		OutPositions[p++] = PP.X;
		OutPositions[p++] = PP.Y;
	}
}

namespace PCGExGeoTasks
{
	void FTransformPointIO::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		UPCGBasePointData* OutPointData = ToBeTransformedIO->GetOut();
		TPCGValueRange<FTransform> OutTransforms = OutPointData->GetTransformValueRange();
		FTransform TargetTransform = FTransform::Identity;

		FBox PointBounds = FBox(ForceInit);

		if (!TransformDetails->bIgnoreBounds)
		{
			for (int i = 0; i < OutTransforms.Num(); i++) { PointBounds += OutPointData->GetLocalBounds(i).TransformBy(OutTransforms[i]); }
		}
		else
		{
			for (const FTransform& Pt : OutTransforms) { PointBounds += Pt.GetLocation(); }
		}

		PointBounds = PointBounds.ExpandBy(0.1); // Avoid NaN
		TransformDetails->ComputeTransform(TaskIndex, TargetTransform, PointBounds);

		if (TransformDetails->bInheritRotation && TransformDetails->bInheritScale)
		{
			for (FTransform& Transform : OutTransforms) { Transform *= TargetTransform; }
		}
		else
		{
			if (TransformDetails->bInheritRotation)
			{
				for (FTransform& Transform : OutTransforms)
				{
					FVector OriginalScale = Transform.GetScale3D();
					Transform *= TargetTransform;
					Transform.SetScale3D(OriginalScale);
				}
			}
			else if (TransformDetails->bInheritScale)
			{
				for (FTransform& Transform : OutTransforms)
				{
					FQuat OriginalRot = Transform.GetRotation();
					Transform *= TargetTransform;
					Transform.SetRotation(OriginalRot);
				}
			}
			else
			{
				for (FTransform& Transform : OutTransforms)
				{
					Transform.SetLocation(TargetTransform.TransformPosition(Transform.GetLocation()));
				}
			}
		}
	}
}

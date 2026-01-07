// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Math/PCGExBestFitPlane.h"

#include "CoreMinimal.h"
#include "MinVolumeBox3.h"
#include "Async/ParallelFor.h"
#include "Math/PCGExMathAxis.h"
#include "Utils/PCGValueRange.h"

namespace PCGExMath
{
	FBestFitPlane::FBestFitPlane(const TConstPCGValueRange<FTransform>& InTransforms)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		UE::Geometry::FOrientedBox3d OrientedBox{};
		UE::Geometry::TMinVolumeBox3<double> Box;

		Centroid = FVector::ZeroVector;

		Box.Solve(InTransforms.Num(), [&](int32 i)
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

		Box.Solve(InIndices.Num(), [&](int32 i)
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

		Box.Solve(InPositions.Num(), [&](int32 i)
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

		Box.Solve(InPositions.Num(), [&](int32 i)
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

	FBestFitPlane::FBestFitPlane(const int32 NumElements, FGetElementPositionCallback&& GetPointFunc)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		UE::Geometry::FOrientedBox3d OrientedBox{};
		UE::Geometry::TMinVolumeBox3<double> Box;

		Centroid = FVector::ZeroVector;

		Box.Solve(NumElements, [&](int32 i)
		{
			const FVector P = GetPointFunc(i);
			Centroid += P;
			return P;
		});

		Centroid /= NumElements;

		if (Box.IsSolutionAvailable())
		{
			Box.GetResult(OrientedBox);
			ProcessBox(OrientedBox);
		}
	}

	FBestFitPlane::FBestFitPlane(const int32 NumElements, FGetElementPositionCallback&& GetPointFunc, const FVector& Extra)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		UE::Geometry::FOrientedBox3d OrientedBox{};
		UE::Geometry::TMinVolumeBox3<double> Box;

		Centroid = FVector::ZeroVector;

		Box.Solve(NumElements + 1, [&](int32 i)
		{
			const FVector P = i == NumElements ? Extra : GetPointFunc(i);
			Centroid += P;
			return P;
		});

		Centroid /= NumElements;

		if (Box.IsSolutionAvailable())
		{
			Box.GetResult(OrientedBox);
			ProcessBox(OrientedBox);
		}
	}

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
		GetAxesOrder(Order, Comps);

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

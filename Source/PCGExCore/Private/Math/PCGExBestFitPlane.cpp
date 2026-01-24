// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Math/PCGExBestFitPlane.h"

#include "CoreMinimal.h"
#include "MinVolumeBox3.h"
#include "Async/ParallelFor.h"
#include "Math/PCGExMathAxis.h"
#include "Utils/PCGValueRange.h"

namespace PCGExMath
{
	namespace BestFitPlaneInternal
	{
		// Fast PCA-based best fit plane computation using covariance matrix
		template <typename PointAccessor>
		void ComputePCA(const int32 NumPoints, PointAccessor&& GetPoint, FVector& OutCentroid, FVector OutAxis[3], FVector& OutExtents, int32 OutSwizzle[3])
		{
			// Compute centroid
			OutCentroid = FVector::ZeroVector;
			for (int32 i = 0; i < NumPoints; i++)
			{
				OutCentroid += GetPoint(i);
			}
			OutCentroid /= NumPoints;

			// Build covariance matrix (symmetric 3x3)
			double Cov[6] = {0, 0, 0, 0, 0, 0}; // XX, YY, ZZ, XY, XZ, YZ
			for (int32 i = 0; i < NumPoints; i++)
			{
				const FVector P = GetPoint(i) - OutCentroid;
				Cov[0] += P.X * P.X; // XX
				Cov[1] += P.Y * P.Y; // YY
				Cov[2] += P.Z * P.Z; // ZZ
				Cov[3] += P.X * P.Y; // XY
				Cov[4] += P.X * P.Z; // XZ
				Cov[5] += P.Y * P.Z; // YZ
			}

			// Normalize
			const double Scale = 1.0 / NumPoints;
			for (int32 i = 0; i < 6; i++) { Cov[i] *= Scale; }

			// Find largest eigenvalue/eigenvector using power iteration (primary axis)
			FVector V0(1, 0, 0);
			for (int32 Iter = 0; Iter < 8; Iter++)
			{
				const FVector Temp(
					Cov[0] * V0.X + Cov[3] * V0.Y + Cov[4] * V0.Z,
					Cov[3] * V0.X + Cov[1] * V0.Y + Cov[5] * V0.Z,
					Cov[4] * V0.X + Cov[5] * V0.Y + Cov[2] * V0.Z
				);
				V0 = Temp.GetSafeNormal();
			}

			// Find second eigenvector perpendicular to first
			FVector V1 = FVector::CrossProduct(V0, FVector::UpVector).GetSafeNormal();
			if (V1.IsNearlyZero()) { V1 = FVector::CrossProduct(V0, FVector::ForwardVector).GetSafeNormal(); }

			for (int32 Iter = 0; Iter < 8; Iter++)
			{
				FVector Temp(
					Cov[0] * V1.X + Cov[3] * V1.Y + Cov[4] * V1.Z,
					Cov[3] * V1.X + Cov[1] * V1.Y + Cov[5] * V1.Z,
					Cov[4] * V1.X + Cov[5] * V1.Y + Cov[2] * V1.Z
				);
				Temp -= FVector::DotProduct(Temp, V0) * V0; // Gram-Schmidt
				V1 = Temp.GetSafeNormal();
			}

			// Third axis is cross product
			FVector V2 = FVector::CrossProduct(V0, V1).GetSafeNormal();

			// Compute variance (eigenvalue approximation) for each axis
			double Variance[3];
			for (int32 AxisIdx = 0; AxisIdx < 3; AxisIdx++)
			{
				const FVector& V = (AxisIdx == 0) ? V0 : (AxisIdx == 1) ? V1 : V2;
				const FVector Temp(
					Cov[0] * V.X + Cov[3] * V.Y + Cov[4] * V.Z,
					Cov[3] * V.X + Cov[1] * V.Y + Cov[5] * V.Z,
					Cov[4] * V.X + Cov[5] * V.Y + Cov[2] * V.Z
				);
				Variance[AxisIdx] = FVector::DotProduct(V, Temp);
			}

			// Sort axes by variance (largest to smallest)
			OutSwizzle[0] = 0; OutSwizzle[1] = 1; OutSwizzle[2] = 2;
			if (Variance[1] > Variance[0]) { Swap(OutSwizzle[0], OutSwizzle[1]); }
			if (Variance[2] > Variance[OutSwizzle[0]]) { Swap(OutSwizzle[0], OutSwizzle[2]); }
			if (Variance[OutSwizzle[2]] > Variance[OutSwizzle[1]]) { Swap(OutSwizzle[1], OutSwizzle[2]); }

			FVector TempAxes[3] = {V0, V1, V2};
			FVector X = TempAxes[OutSwizzle[0]]; // Largest variance
			FVector Y = TempAxes[OutSwizzle[1]]; // Medium variance
			FVector Z = TempAxes[OutSwizzle[2]]; // Smallest variance

			// Ensure right-handed system
			Z = FVector::CrossProduct(X, Y).GetSafeNormal();
			Y = FVector::CrossProduct(Z, X).GetSafeNormal();

			// Make Z point upward
			if (FVector::DotProduct(Z, FVector::UpVector) < 0) { Z *= -1; }

			OutAxis[0] = X.GetSafeNormal();
			OutAxis[1] = Y.GetSafeNormal();
			OutAxis[2] = Z.GetSafeNormal();

			// Compute extents by projecting all points onto axes
			FVector MinProj(MAX_dbl, MAX_dbl, MAX_dbl);
			FVector MaxProj(-MAX_dbl, -MAX_dbl, -MAX_dbl);

			for (int32 i = 0; i < NumPoints; i++)
			{
				const FVector P = GetPoint(i) - OutCentroid;
				for (int32 AxisIdx = 0; AxisIdx < 3; AxisIdx++)
				{
					const double Proj = FVector::DotProduct(P, OutAxis[AxisIdx]);
					MinProj[AxisIdx] = FMath::Min(MinProj[AxisIdx], Proj);
					MaxProj[AxisIdx] = FMath::Max(MaxProj[AxisIdx], Proj);
				}
			}

			OutExtents = (MaxProj - MinProj) * 0.5;
		}
	}

	FBestFitPlane::FBestFitPlane(const TConstPCGValueRange<FTransform>& InTransforms, const bool bUsePreciseBounds)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		if (bUsePreciseBounds)
		{
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
		else
		{
			BestFitPlaneInternal::ComputePCA(
				InTransforms.Num(),
				[&](int32 i) { return InTransforms[i].GetLocation(); },
				Centroid, Axis, Extents, Swizzle
			);
		}
	}

	FBestFitPlane::FBestFitPlane(const TConstPCGValueRange<FTransform>& InTransforms, const TArrayView<int32> InIndices, const bool bUsePreciseBounds)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		if (bUsePreciseBounds)
		{
			UE::Geometry::FOrientedBox3d OrientedBox{};
			UE::Geometry::TMinVolumeBox3<double> Box;

			Centroid = FVector::ZeroVector;

			Box.Solve(InIndices.Num(), [&](int32 i)
			{
				const FVector P = InTransforms[InIndices[i]].GetLocation();
				Centroid += P;
				return P;
			});

			Centroid /= InIndices.Num();

			if (Box.IsSolutionAvailable())
			{
				Box.GetResult(OrientedBox);
				ProcessBox(OrientedBox);
			}
		}
		else
		{
			BestFitPlaneInternal::ComputePCA(
				InIndices.Num(),
				[&](int32 i) { return InTransforms[InIndices[i]].GetLocation(); },
				Centroid, Axis, Extents, Swizzle
			);
		}
	}

	FBestFitPlane::FBestFitPlane(const TArrayView<const FVector> InPositions, const bool bUsePreciseBounds)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		if (bUsePreciseBounds)
		{
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
		else
		{
			BestFitPlaneInternal::ComputePCA(
				InPositions.Num(),
				[&](int32 i) { return InPositions[i]; },
				Centroid, Axis, Extents, Swizzle
			);
		}
	}

	FBestFitPlane::FBestFitPlane(const TArrayView<const FVector2D> InPositions, const bool bUsePreciseBounds)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		if (bUsePreciseBounds)
		{
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
		else
		{
			BestFitPlaneInternal::ComputePCA(
				InPositions.Num(),
				[&](int32 i) { return FVector(InPositions[i], 0); },
				Centroid, Axis, Extents, Swizzle
			);
		}
	}

	FBestFitPlane::FBestFitPlane(const int32 NumElements, FGetElementPositionCallback&& GetPointFunc, const bool bUsePreciseBounds)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		if (bUsePreciseBounds)
		{
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
		else
		{
			BestFitPlaneInternal::ComputePCA(
				NumElements,
				[&](int32 i) { return GetPointFunc(i); },
				Centroid, Axis, Extents, Swizzle
			);
		}
	}

	FBestFitPlane::FBestFitPlane(const int32 NumElements, FGetElementPositionCallback&& GetPointFunc, const FVector& Extra, const bool bUsePreciseBounds)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FBestFitPlane::FBestFitPlane);

		if (bUsePreciseBounds)
		{
			UE::Geometry::FOrientedBox3d OrientedBox{};
			UE::Geometry::TMinVolumeBox3<double> Box;

			Centroid = FVector::ZeroVector;

			Box.Solve(NumElements + 1, [&](int32 i)
			{
				const FVector P = i == NumElements ? Extra : GetPointFunc(i);
				Centroid += P;
				return P;
			});

			Centroid /= NumElements + 1;

			if (Box.IsSolutionAvailable())
			{
				Box.GetResult(OrientedBox);
				ProcessBox(OrientedBox);
			}
		}
		else
		{
			BestFitPlaneInternal::ComputePCA(
				NumElements + 1,
				[&](int32 i) { return i == NumElements ? Extra : GetPointFunc(i); },
				Centroid, Axis, Extents, Swizzle
			);
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

	FVector FBestFitPlane::GetExtents(const EPCGExAxisOrder Order) const
	{
		int32 Comps[3] = {0, 0, 0};
		GetAxesOrder(Order, Comps);

		// Reorder extents to match the axis order
		return FVector(Extents[Comps[0]], Extents[Comps[1]], Extents[Comps[2]]);
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

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

namespace PCGExMath::OBB
{
	// Hot data - touched during spatial queries
	struct PCGEXCORE_API FBounds
	{
		FVector Origin;
		float Radius;
		FVector HalfExtents;
		int32 Index;

		FBounds() = default;

		FBounds(const FVector& InOrigin, const FVector& InHalfExtents, int32 InIndex)
			: Origin(InOrigin), Radius(InHalfExtents.Size()), HalfExtents(InHalfExtents), Index(InIndex)
		{
		}

		FORCEINLINE float GetRadiusSq() const { return Radius * Radius; }
	};

	// Cold data - only touched when doing full OBB tests 
	struct PCGEXCORE_API FOrientation
	{
		FQuat Rotation = FQuat::Identity; // 16 bytes

		FOrientation() = default;

		explicit FOrientation(const FQuat& InRotation)
			: Rotation(InRotation)
		{
		}

		// Axes computed on demand
		FORCEINLINE FVector GetAxisX() const { return Rotation.GetAxisX() ; }
		FORCEINLINE FVector GetAxisY() const { return Rotation.GetAxisY(); }
		FORCEINLINE FVector GetAxisZ() const { return Rotation.GetAxisZ(); }

		// Transform point to local space
		FORCEINLINE FVector ToLocal(const FVector& WorldPoint, const FVector& Origin) const
		{
			return Rotation.UnrotateVector(WorldPoint - Origin);
		}

		// Transform point to world space
		FORCEINLINE FVector ToWorld(const FVector& LocalPoint, const FVector& Origin) const
		{
			return Origin + Rotation.RotateVector(LocalPoint);
		}

		// Transform direction to world space (no translation)
		FORCEINLINE FVector RotateVector(const FVector& V) const
		{
			return Rotation.RotateVector(V);
		}
	};

	// Combined OBB 
	struct PCGEXCORE_API FOBB
	{
		FBounds Bounds;
		FOrientation Orientation;

		FOBB() = default;

		FOBB(const FBounds& InBounds, const FOrientation& InOrientation)
			: Bounds(InBounds), Orientation(InOrientation)
		{
		}

		// Convenience accessors
		FORCEINLINE const FVector& GetOrigin() const { return Bounds.Origin; }
		FORCEINLINE const FVector& GetHalfExtents() const { return Bounds.HalfExtents; }
		FORCEINLINE float GetRadius() const { return Bounds.Radius; }
		FORCEINLINE int32 GetIndex() const { return Bounds.Index; }
		FORCEINLINE const FQuat& GetRotation() const { return Orientation.Rotation; }

		FORCEINLINE FVector ToLocal(const FVector& WorldPoint) const
		{
			return Orientation.ToLocal(WorldPoint, Bounds.Origin);
		}

		FORCEINLINE FVector ToWorld(const FVector& LocalPoint) const
		{
			return Orientation.ToWorld(LocalPoint, Bounds.Origin);
		}

		// Local box (always centered at origin)
		FORCEINLINE FBox GetLocalBox() const
		{
			return FBox(-Bounds.HalfExtents, Bounds.HalfExtents);
		}

		// Matrix - computed on demand for FMath::LineExtentBoxIntersection
		FMatrix GetMatrix() const
		{
			const FVector X = Orientation.GetAxisX();
			const FVector Y = Orientation.GetAxisY();
			const FVector Z = Orientation.GetAxisZ();
			return FMatrix(
				FPlane(X.X, X.Y, X.Z, 0),
				FPlane(Y.X, Y.Y, Y.Z, 0),
				FPlane(Z.X, Z.Y, Z.Z, 0),
				FPlane(Bounds.Origin.X, Bounds.Origin.Y, Bounds.Origin.Z, 1)
			);
		}
	};

	// OBB Factory - single place for OBB construction
	namespace Factory
	{
		FORCEINLINE FOBB FromTransform(const FTransform& Transform, const FBox& LocalBox, int32 Index)
		{
			const FVector LocalCenter = LocalBox.GetCenter();
			const FQuat Rotation = Transform.GetRotation();
			const FVector WorldOrigin = Transform.GetLocation() + Rotation.RotateVector(LocalCenter);
			const FVector HalfExtents = LocalBox.GetExtent();

			return FOBB(
				FBounds(WorldOrigin, HalfExtents, Index),
				FOrientation(Rotation)
			);
		}

		FORCEINLINE FOBB FromTransform(const FTransform& Transform, const FVector& HalfExtents, int32 Index)
		{
			return FOBB(
				FBounds(Transform.GetLocation(), HalfExtents, Index),
				FOrientation(Transform.GetRotation())
			);
		}

		FORCEINLINE FOBB FromAABB(const FBox& WorldBox, int32 Index)
		{
			return FOBB(
				FBounds(WorldBox.GetCenter(), WorldBox.GetExtent(), Index),
				FOrientation()
			);
		}

		FORCEINLINE FOBB Expanded(const FOBB& Source, float Expansion)
		{
			FBounds NewBounds = Source.Bounds;
			NewBounds.HalfExtents += FVector(Expansion);
			NewBounds.Radius = NewBounds.HalfExtents.Size();
			return FOBB(NewBounds, Source.Orientation);
		}
	}
}

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGEx.h"
#include "PCGExMT.h"
#include "PCGExDetails.h"
#include "Data/PCGExData.h"


#include "PCGExGeo.generated.h"

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExGeo2DProjectionDetails
{
	GENERATED_BODY()

	FPCGExGeo2DProjectionDetails()
	{
	}

	explicit FPCGExGeo2DProjectionDetails(const FPCGExGeo2DProjectionDetails& Other)
		: bSupportLocalNormal(Other.bLocalProjectionNormal),
		  ProjectionNormal(Other.ProjectionNormal),
		  bLocalProjectionNormal(Other.bLocalProjectionNormal),
		  LocalNormal(Other.LocalNormal)
	{
	}

	explicit FPCGExGeo2DProjectionDetails(const bool InSupportLocalNormal)
		: bSupportLocalNormal(InSupportLocalNormal)
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, HideInDetailPanel, Hidden, EditConditionHides, EditCondition="false"))
	bool bSupportLocalNormal = true;

	/** Normal vector of the 2D projection plane. Defaults to Up for XY projection. Used as fallback when using invalid local normal. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FVector ProjectionNormal = FVector::UpVector;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="bSupportLocalNormal", EditConditionHides))
	bool bLocalProjectionNormal = false;

	/** Local attribute to fetch projection normal from */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable, EditCondition="bSupportLocalNormal && bLocalProjectionNormal", EditConditionHides))
	FPCGAttributePropertyInputSelector LocalNormal;

	TSharedPtr<PCGExData::TBuffer<FVector>> NormalGetter;
	FQuat ProjectionQuat = FQuat::Identity;
	FQuat ProjectionInverseQuat = FQuat::Identity;

	bool Init(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade>& PointDataFacade)
	{
		ProjectionNormal = ProjectionNormal.GetSafeNormal(1E-08, FVector::UpVector);
		ProjectionQuat = FQuat::FindBetweenNormals(ProjectionNormal, FVector::UpVector);
		ProjectionInverseQuat = ProjectionInverseQuat.Inverse();

		if (!bSupportLocalNormal) { bLocalProjectionNormal = false; }
		if (bLocalProjectionNormal && PointDataFacade)
		{
			NormalGetter = PointDataFacade->GetBroadcaster<FVector>(LocalNormal);
			if (!NormalGetter)
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FTEXT("Missing normal attribute for projection."));
				return false;
			}
		}

		return true;
	}

	~FPCGExGeo2DProjectionDetails()
	{
	}

	FORCEINLINE FQuat GetQuat(const int32 PointIndex) const
	{
		return NormalGetter ? FQuat::FindBetweenNormals(NormalGetter->Read(PointIndex).GetSafeNormal(1E-08, FVector::UpVector), FVector::UpVector) :
			       ProjectionQuat;
	}

	FORCEINLINE FVector Project(const FVector& InPosition, const int32 PointIndex) const
	{
		return NormalGetter ? FQuat::FindBetweenNormals(NormalGetter->Read(PointIndex).GetSafeNormal(1E-08, FVector::UpVector), FVector::UpVector).RotateVector(InPosition) :
			       ProjectionQuat.RotateVector(InPosition);
	}

	FORCEINLINE FVector Project(const FVector& InPosition) const
	{
		return ProjectionQuat.RotateVector(InPosition);
	}

	FORCEINLINE FVector ProjectFlat(const FVector& InPosition) const
	{
		FVector RotatedPosition = ProjectionQuat.RotateVector(InPosition);
		RotatedPosition.Z = 0;
		return RotatedPosition;
	}

	FORCEINLINE FVector ProjectFlat(const FVector& InPosition, const int32 PointIndex) const
	{
		FVector RotatedPosition = GetQuat(PointIndex).RotateVector(InPosition);
		RotatedPosition.Z = 0;
		return RotatedPosition;
	}

	FORCEINLINE FTransform ProjectFlat(const FTransform& InTransform) const
	{
		FVector Position = ProjectionQuat.RotateVector(InTransform.GetLocation());
		Position.Z = 0;
		const FQuat Quat = InTransform.GetRotation();
		return FTransform(FQuat::FindBetweenNormals(Quat.GetUpVector(), FVector::UpVector) * Quat, Position);
	}

	FORCEINLINE FTransform ProjectFlat(const FTransform& InTransform, const int32 PointIndex) const
	{
		FVector Position = GetQuat(PointIndex).RotateVector(InTransform.GetLocation());
		Position.Z = 0;
		const FQuat Quat = InTransform.GetRotation();
		return FTransform(FQuat::FindBetweenNormals(Quat.GetUpVector(), FVector::UpVector) * Quat, Position);
	}

	void Project(const TArray<FVector>& InPositions, TArray<FVector>& OutPositions) const
	{
		const int32 NumVectors = InPositions.Num();
		PCGEx::InitArray(OutPositions, NumVectors);

		if (NormalGetter)
		{
			for (int i = 0; i < NumVectors; i++)
			{
				OutPositions[i] = FQuat::FindBetweenNormals(
					NormalGetter->Read(i).GetSafeNormal(1E-08, FVector::UpVector),
					FVector::UpVector).RotateVector(InPositions[i]);
			}
		}
		else
		{
			for (int i = 0; i < NumVectors; i++)
			{
				OutPositions[i] = ProjectionQuat.RotateVector(InPositions[i]);
			}
		}
	}

	void Project(const TArrayView<FVector>& InPositions, TArray<FVector>& OutPositions) const
	{
		const int32 NumVectors = InPositions.Num();
		PCGEx::InitArray(OutPositions, NumVectors);
		for (int i = 0; i < NumVectors; i++) { OutPositions[i] = ProjectionQuat.RotateVector(InPositions[i]); }
	}

	void Project(const TArray<FVector>& InPositions, TArray<FVector2D>& OutPositions) const
	{
		const int32 NumVectors = InPositions.Num();
		PCGEx::InitArray(OutPositions, NumVectors);

		if (NormalGetter)
		{
			for (int i = 0; i < NumVectors; i++)
			{
				OutPositions[i] = FVector2D(
					FQuat::FindBetweenNormals(
						NormalGetter->Read(i).GetSafeNormal(1E-08, FVector::UpVector),
						FVector::UpVector).RotateVector(InPositions[i]));
			}
		}
		else
		{
			for (int i = 0; i < NumVectors; i++)
			{
				OutPositions[i] = FVector2D(ProjectionQuat.RotateVector(InPositions[i]));
			}
		}
	}

	void Project(const TArrayView<FVector>& InPositions, TArray<FVector2D>& OutPositions) const
	{
		const int32 NumVectors = InPositions.Num();
		PCGEx::InitArray(OutPositions, NumVectors);
		for (int i = 0; i < NumVectors; i++) { OutPositions[i] = FVector2D(ProjectionQuat.RotateVector(InPositions[i])); }
	}

	void Project(const TArray<FPCGPoint>& InPoints, TArray<FVector>& OutPositions) const
	{
		const int32 NumVectors = InPoints.Num();
		PCGEx::InitArray(OutPositions, NumVectors);

		if (NormalGetter)
		{
			for (int i = 0; i < NumVectors; i++)
			{
				OutPositions[i] = FQuat::FindBetweenNormals(
					NormalGetter->Read(i).GetSafeNormal(1E-08, FVector::UpVector),
					FVector::UpVector).RotateVector(InPoints[i].Transform.GetLocation());
			}
		}
		else
		{
			for (int i = 0; i < NumVectors; i++) { OutPositions[i] = ProjectionQuat.RotateVector(InPoints[i].Transform.GetLocation()); }
		}
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExLloydSettings
{
	GENERATED_BODY()

	FPCGExLloydSettings()
	{
	}

	/** The number of Lloyd iteration to apply prior to generating the graph. Very expensive!*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bLloydRelax"))
	int32 Iterations = 3;

	/** The influence of each iteration. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bLloydRelax", ClampMin=0, ClampMax=1))
	double Influence = 1;

	FORCEINLINE bool IsValid() const { return Iterations > 0 && Influence > 0; }
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Cell Center"))
enum class EPCGExCellCenter : uint8
{
	Balanced     = 0 UMETA(DisplayName = "Balanced", ToolTip="Pick centroid if circumcenter is out of bounds, otherwise uses circumcenter."),
	Circumcenter = 1 UMETA(DisplayName = "Canon (Circumcenter)", ToolTip="Uses Delaunay cells' circumcenter."),
	Centroid     = 2 UMETA(DisplayName = "Centroid", ToolTip="Uses Delaunay cells' averaged vertice positions.")
};

namespace PCGExGeo
{
	PCGEX_ASYNC_STATE(State_ExtractingMesh)

	FORCEINLINE static double S_U(
		const FVector& A, const FVector& B, const FVector& C, const FVector& D,
		const FVector& E, const FVector& F, const FVector& G, const FVector& H)
	{
		return (A.Z - B.Z) * (C.X * D.Y - D.X * C.Y) - (E.Z - F.Z) * (G.X * H.Y - H.X * G.Y);
	};

	FORCEINLINE static double S_D(
		const int FirstComponent, const int SecondComponent,
		FVector A, FVector B, FVector C)
	{
		return
			A[FirstComponent] * (B[SecondComponent] - C[SecondComponent]) +
			B[FirstComponent] * (C[SecondComponent] - A[SecondComponent]) +
			C[FirstComponent] * (A[SecondComponent] - B[SecondComponent]);
	};

	FORCEINLINE static double S_E(
		const int FirstComponent, const int SecondComponent,
		const FVector& A, const FVector& B, const FVector& C, const FVector& D,
		const double RA, const double RB, const double RC, const double RD, const double UVW)
	{
		return (RA * S_D(FirstComponent, SecondComponent, B, C, D) - RB * S_D(FirstComponent, SecondComponent, C, D, A) +
			RC * S_D(FirstComponent, SecondComponent, D, A, B) - RD * S_D(FirstComponent, SecondComponent, A, B, C)) / UVW;
	};

	static double S_SQ(const FVector& P) { return P.X * P.X + P.Y * P.Y + P.Z * P.Z; };

	FORCEINLINE static bool FindSphereFrom4Points(const FVector& A, const FVector& B, const FVector& C, const FVector& D, FSphere& OutSphere)
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

	FORCEINLINE static bool FindSphereFrom4Points(const TArrayView<FVector>& Positions, const int32 (&Vtx)[4], FSphere& OutSphere)
	{
		return FindSphereFrom4Points(
			Positions[Vtx[0]],
			Positions[Vtx[1]],
			Positions[Vtx[2]],
			Positions[Vtx[3]],
			OutSphere);
	}

	FORCEINLINE static void GetCircumcenter(const TArrayView<FVector>& Positions, const int32 (&Vtx)[3], FVector& OutCircumcenter)
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

	/*
	static void GetCircumcenter(const TArrayView<FVector>& Positions, const int32 (&Vtx)[3], FVector& OutCircumcenter)
	{
		const FVector& A = Positions[Vtx[0]];
		const FVector& B = Positions[Vtx[1]];
		const FVector& C = Positions[Vtx[2]];

		// Step 2: Calculate midpoints
		const FVector M1 = FMath::Lerp(A, B, 0.5);
		const FVector M2 = FMath::Lerp(B, C, 0.5);

		// Step 3: Calculate perpendicular bisectors
		const double S1 = -1 / ((B.Y - A.Y) / (B.X - A.X));
		const double S2 = -1 / ((C.Y - B.Y) / (C.X - B.X));

		// Calculate y-intercepts of bisectors
		const double I1 = M1.Y - S1 * M1.X;
		const double I2 = M2.Y - S2 * M2.X;

		// Step 4: Find intersection point
		const double CX = (I2 - I1) / (S1 - S2);
		const double CY = S1 * CX + I1;

		OutCircumcenter.X = CX;
		OutCircumcenter.Y = CY;
		OutCircumcenter.Z = (A.Z + B.Z + C.Z) / 3;
	}
	*/

	FORCEINLINE static void GetCentroid(const TArrayView<FVector>& Positions, const int32 (&Vtx)[4], FVector& OutCentroid)
	{
		OutCentroid = FVector::Zero();
		for (int i = 0; i < 4; i++) { OutCentroid += Positions[Vtx[i]]; }
		OutCentroid /= 4;
	}

	FORCEINLINE static void GetCentroid(const TArrayView<FVector>& Positions, const int32 (&Vtx)[3], FVector& OutCentroid)
	{
		OutCentroid = FVector::Zero();
		for (int i = 0; i < 3; i++) { OutCentroid += Positions[Vtx[i]]; }
		OutCentroid /= 3;
	}

	FORCEINLINE static void GetLongestEdge(const TArrayView<FVector>& Positions, const int32 (&Vtx)[3], uint64& Edge)
	{
		double Dist = MIN_dbl;
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

	FORCEINLINE static void GetLongestEdge(const TArrayView<FVector>& Positions, const int32 (&Vtx)[4], uint64& Edge)
	{
		double Dist = MIN_dbl;
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

	FORCEINLINE static void PointsToPositions(const TArray<FPCGPoint>& Points, TArray<FVector>& OutPositions)
	{
		const int32 NumPoints = Points.Num();
		PCGEx::InitArray(OutPositions, NumPoints);
		for (int i = 0; i < NumPoints; i++) { OutPositions[i] = Points[i].Transform.GetLocation(); }
	}

	FORCEINLINE static FVector GetBarycentricCoordinates(const FVector& Point, const FVector& A, const FVector& B, const FVector& C)
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

	/**
		 *	 Leave <---.Apex-----> Arrive (Direction)
		 *		   . '   |    '  .  
		 *		A----Anchor---------B
		 */
	struct /*PCGEXTENDEDTOOLKIT_API*/ FApex
	{
		FApex()
		{
		}

		FApex(const FVector& Start, const FVector& End, const FVector& InApex)
		{
			Direction = (Start - End).GetSafeNormal();
			Anchor = FMath::ClosestPointOnSegment(InApex, Start, End);

			const double DistToStart = FVector::Dist(Start, Anchor);
			const double DistToEnd = FVector::Dist(End, Anchor);
			TowardStart = Direction * (DistToStart * -1);
			TowardEnd = Direction * DistToEnd;
			Alpha = DistToStart / (DistToStart + DistToEnd);
		}

		FVector Direction;
		FVector Anchor;
		FVector TowardStart;
		FVector TowardEnd;
		double Alpha = 0;

		FVector GetAnchorNormal(const FVector& Location) const { return (Anchor - Location).GetSafeNormal(); }

		void Scale(const double InScale)
		{
			TowardStart *= InScale;
			TowardEnd *= InScale;
		}

		void Extend(const double InSize)
		{
			TowardStart += Direction * InSize;
			TowardEnd += Direction * -InSize;
		}

		static FApex FromStartOnly(const FVector& Start, const FVector& InApex) { return FApex(Start, InApex, InApex); }
		static FApex FromEndOnly(const FVector& End, const FVector& InApex) { return FApex(InApex, End, InApex); }
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FExCenterArc
	{
		double Radius = 0;
		double Theta = 0;
		double SinTheta = 0;

		FVector Center = FVector::ZeroVector;
		FVector Normal = FVector::ZeroVector;
		FVector Hand = FVector::ZeroVector;
		FVector OtherHand = FVector::ZeroVector;

		bool bIsLine = false;

		FExCenterArc()
		{
		}


		/**
		 * ExCenter arc from 3 points.
		 * The arc center will be opposite to B
		 * @param A 
		 * @param B 
		 * @param C 
		 */
		FExCenterArc(const FVector& A, const FVector& B, const FVector& C)
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

		/**
		 * ExCenter arc from 2 segments.
		 * The arc center will be opposite to B
		 * @param A1 
		 * @param B1 
		 * @param A2 
		 * @param B2
		 * @param MaxLength 
		 */
		FExCenterArc(const FVector& A1, const FVector& B1, const FVector& A2, const FVector& B2, const double MaxLength = 100000)
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

		FORCEINLINE double GetLength() const { return Radius * Theta; }

		/**
		 * 
		 * @param Alpha 0-1 normalized range on the arc
		 * @return 
		 */
		FORCEINLINE FVector GetLocationOnArc(const double Alpha) const
		{
			const double W1 = FMath::Sin((1.0 - Alpha) * Theta) / SinTheta;
			const double W2 = FMath::Sin(Alpha * Theta) / SinTheta;

			const FVector Dir = Hand * W1 + OtherHand * W2;
			return Center + (Dir * Radius);
		}
	};
}

namespace PCGExGeoTasks
{
	class /*PCGEXTENDEDTOOLKIT_API*/ FTransformPointIO final : public PCGExMT::FPCGExTask
	{
	public:
		FTransformPointIO(
			const TSharedPtr<PCGExData::FPointIO>& InPointIO,
			const TSharedPtr<PCGExData::FPointIO>& InToBeTransformedIO,
			FPCGExTransformDetails* InTransformDetails) :
			FPCGExTask(InPointIO),
			ToBeTransformedIO(InToBeTransformedIO),
			TransformDetails(InTransformDetails)
		{
		}

		TSharedPtr<PCGExData::FPointIO> ToBeTransformedIO;
		FPCGExTransformDetails* TransformDetails = nullptr;

		virtual bool ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override
		{
			const FPCGPoint& TargetPoint = PointIO->GetInPoint(TaskIndex);
			TArray<FPCGPoint>& MutableTargets = ToBeTransformedIO->GetOut()->GetMutablePoints();

			for (FPCGPoint& InPoint : MutableTargets)
			{
				if (TransformDetails->bInheritRotation && TransformDetails->bInheritScale)
				{
					InPoint.Transform *= TargetPoint.Transform;
					continue;
				}

				InPoint.Transform.SetLocation(TargetPoint.Transform.TransformPosition(InPoint.Transform.GetLocation()));

				if (TransformDetails->bInheritRotation)
				{
					InPoint.Transform.SetRotation(TargetPoint.Transform.TransformRotation(InPoint.Transform.GetRotation()));
				}
				else if (TransformDetails->bInheritScale)
				{
					InPoint.Transform.SetScale3D(TargetPoint.Transform.GetScale3D() * InPoint.Transform.GetScale3D());
				}
			}

			return true;
		}
	};
}

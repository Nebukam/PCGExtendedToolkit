// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPaths.h"

#include "Graph/Probes/PCGExProbeDirection.h"

#define LOCTEXT_NAMESPACE "PCGExPaths"
#define PCGEX_NAMESPACE PCGExPaths

bool FPCGExPathOutputDetails::Validate(TArray<FPCGPoint>& InPathPoints) const
{
	if (InPathPoints.Num() < 2) { return false; }
	if (bRemoveSmallPaths && InPathPoints.Num() < MinPointCount) { return false; }
	if (bRemoveLargePaths && InPathPoints.Num() > MaxPointCount) { return false; }
	return true;
}

void FPCGExPathClosedLoopDetails::Init()
{
	Tags = PCGExHelpers::GetStringArrayFromCommaSeparatedList(CommaSeparatedTags);
}

bool FPCGExPathClosedLoopDetails::IsClosedLoop(const TSharedPtr<PCGExData::FPointIO>& InPointIO) const
{
	if (Scope == EPCGExInputScope::All) { return bClosedLoop; }
	if (Tags.IsEmpty()) { return !bClosedLoop; }
	for (const FString& Tag : Tags) { if (InPointIO->Tags->IsTagged(Tag)) { return !bClosedLoop; } }
	return bClosedLoop;
}

bool FPCGExPathClosedLoopDetails::IsClosedLoop(const FPCGTaggedData& InTaggedData) const
{
	if (Scope == EPCGExInputScope::All) { return bClosedLoop; }
	if (Tags.IsEmpty()) { return !bClosedLoop; }
	for (const FString& Tag : Tags) { if (InTaggedData.Tags.Contains(Tag)) { return !bClosedLoop; } }
	return bClosedLoop;
}

void FPCGExPathClosedLoopUpdateDetails::Init()
{
	AddTags = PCGExHelpers::GetStringArrayFromCommaSeparatedList(CommaSeparatedAddTags);
	RemoveTags = PCGExHelpers::GetStringArrayFromCommaSeparatedList(CommaSeparatedRemoveTags);
}

void FPCGExPathClosedLoopUpdateDetails::Update(const TSharedPtr<PCGExData::FPointIO>& InPointIO)
{
	for (const FString& Add : AddTags) { InPointIO->Tags->AddRaw(Add); }
	for (const FString& Rem : RemoveTags) { InPointIO->Tags->Remove(Rem); }
}

void FPCGExPathEdgeIntersectionDetails::Init()
{
	MaxDot = bUseMinAngle ? PCGExMath::DegreesToDot(MinAngle) : 1;
	MinDot = bUseMaxAngle ? PCGExMath::DegreesToDot(MaxAngle) : -1;
	ToleranceSquared = Tolerance * Tolerance;
}

void FPCGExPathFilterSettings::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
}

bool FPCGExPathFilterSettings::Init(FPCGExContext* InContext)
{
	return true;
}

FPCGExPathIntersectionDetails::FPCGExPathIntersectionDetails(const double InTolerance, const double InMinAngle, const double InMaxAngle)
{
	Tolerance = InTolerance;
	MinAngle = InMinAngle;
	MaxAngle = InMaxAngle;
	bUseMinAngle = InMinAngle > 0;
	bUseMaxAngle = InMaxAngle < 90;
}

namespace PCGExPaths
{
	FPathMetrics::FPathMetrics(const FVector& InStart)
	{
		Add(InStart);
	}

	FPathMetrics::FPathMetrics(const TArrayView<FPCGPoint>& Points)
	{
		for (const FPCGPoint& Pt : Points) { Add(Pt.Transform.GetLocation()); }
	}

	void FPathMetrics::Reset(const FVector& InStart)
	{
		Start = InStart;
		Last = InStart;
		Length = 0;
		Count = 1;
	}

	double FPathMetrics::Add(const FVector& Location)
	{
		if (Length == -1)
		{
			Reset(Location);
			return 0;
		}
		Length += DistToLast(Location);
		Last = Location;
		Count++;
		return Length;
	}

	double FPathMetrics::Add(const FVector& Location, double& OutDistToLast)
	{
		if (Length == -1)
		{
			Reset(Location);
			return 0;
		}
		OutDistToLast = DistToLast(Location);
		Length += OutDistToLast;
		Last = Location;
		Count++;
		return Length;
	}

	void FSplineMeshSegment::ComputeUpVectorFromTangents()
	{
		// Thanks Drakynfly @ https://www.reddit.com/r/unrealengine/comments/kqo6ez/usplinecomponent_twists_in_on_itself/

		const FVector A = Params.StartTangent.GetSafeNormal(0.001);
		const FVector B = Params.EndTangent.GetSafeNormal(0.001);
		if (const float Dot = A | B; Dot > 0.99 || Dot <= -0.99) { UpVector = FVector(A.Y, A.Z, A.X); }
		else { UpVector = A ^ B; }
	}

	void FSplineMeshSegment::ApplySettings(USplineMeshComponent* Component) const
	{
		check(Component)

		Component->SetStartAndEnd(Params.StartPos, Params.StartTangent, Params.EndPos, Params.EndTangent, false);

		Component->SetStartScale(Params.StartScale, false);
		if (bUseDegrees) { Component->SetStartRollDegrees(Params.StartRoll, false); }
		else { Component->SetStartRoll(Params.StartRoll, false); }

		Component->SetEndScale(Params.EndScale, false);
		if (bUseDegrees) { Component->SetEndRollDegrees(Params.EndRoll, false); }
		else { Component->SetEndRoll(Params.EndRoll, false); }

		Component->SetForwardAxis(SplineMeshAxis, false);
		Component->SetSplineUpDir(UpVector, false);

		Component->SetStartOffset(Params.StartOffset, false);
		Component->SetEndOffset(Params.EndOffset, false);

		Component->SplineParams.NaniteClusterBoundsScale = Params.NaniteClusterBoundsScale;

		Component->SplineBoundaryMin = 0;
		Component->SplineBoundaryMax = 0;

		Component->bSmoothInterpRollScale = bSmoothInterpRollScale;

		if (bSetMeshWithSettings) { ApplyMesh(Component); }
	}

	bool FSplineMeshSegment::ApplyMesh(USplineMeshComponent* Component) const
	{
		check(Component)
		UStaticMesh* StaticMesh = MeshEntry->Staging.TryGet<UStaticMesh>(); //LoadSynchronous<UStaticMesh>();

		if (!StaticMesh) { return false; }

		Component->SetStaticMesh(StaticMesh); // Will trigger a force rebuild, so put this last
		MeshEntry->ApplyMaterials(MaterialPick, Component);

		return true;
	}

	FPathEdge::FPathEdge(const int32 InStart, const int32 InEnd, const TArrayView<const FVector>& Positions, const double Expansion)
		: Start(InStart), End(InEnd), AltStart(InStart)
	{
		Update(Positions, Expansion);
	}

	void FPathEdge::Update(const TArrayView<const FVector>& Positions, const double Expansion)
	{
		FBox Box = FBox(ForceInit);
		Box += Positions[Start];
		Box += Positions[End];
		Bounds = Box.ExpandBy(Expansion);
		Dir = (Positions[End] - Positions[Start]).GetSafeNormal();
	}

	bool FPathEdge::ShareIndices(const FPathEdge& Other) const
	{
		return Start == Other.Start || Start == Other.End || End == Other.Start || End == Other.End;
	}

	bool FPathEdge::Connects(const FPathEdge& Other) const
	{
		return Start == Other.End || End == Other.Start;
	}

	bool FPathEdge::ShareIndices(const FPathEdge* Other) const
	{
		return Start == Other->Start || Start == Other->End || End == Other->Start || End == Other->End;
	}

	void FPathEdgeExtraBase::ProcessingDone(const FPath* Path)
	{
	}

	void FPath::BuildEdgeOctree()
	{
		if (EdgeOctree) { return; }
		EdgeOctree = MakeUnique<FPathEdgeOctree>(Bounds.GetCenter(), Bounds.GetExtent().Length() + 10);
		for (FPathEdge& Edge : Edges)
		{
			if (!IsEdgeValid(Edge)) { continue; } // Skip zero-length edges
			EdgeOctree->AddElement(&Edge);        // Might be a problem if edges gets reallocated
		}
	}

	void FPath::BuildPartialEdgeOctree(const TArray<int8>& Filter)
	{
		if (EdgeOctree) { return; }
		EdgeOctree = MakeUnique<FPathEdgeOctree>(Bounds.GetCenter(), Bounds.GetExtent().Length() + 10);
		for (int i = 0; i < Edges.Num(); i++)
		{
			FPathEdge& Edge = Edges[i];
			if (!Filter[i] || !IsEdgeValid(Edge)) { continue; } // Skip filtered out & zero-length edges
			EdgeOctree->AddElement(&Edge);                      // Might be a problem if edges gets reallocated
		}
	}

	void FPath::UpdateConvexity(const int32 Index)
	{
		if (!bIsConvex) { return; }

		const int32 A = SafePointIndex(Index - 1);
		const int32 B = SafePointIndex(Index + 1);
		if (A == B)
		{
			bIsConvex = false;
			return;
		}

		PCGExMath::CheckConvex(
			Positions[A].GetLocation(), Positions[Index].GetLocation(), Positions[B].GetLocation(),
			bIsConvex, ConvexitySign);
	}

	void FPath::ComputeEdgeExtra(const int32 Index)
	{
		if (NumEdges == 1)
		{
			for (const TSharedPtr<FPathEdgeExtraBase>& Extra : Extras) { Extra->ProcessSingleEdge(this, Edges[0]); }
		}
		else
		{
			if (Index == 0) { for (const TSharedPtr<FPathEdgeExtraBase>& Extra : Extras) { Extra->ProcessFirstEdge(this, Edges[0]); } }
			else if (Index == LastEdge) { for (const TSharedPtr<FPathEdgeExtraBase>& Extra : Extras) { Extra->ProcessLastEdge(this, Edges[LastEdge]); } }
			else { for (const TSharedPtr<FPathEdgeExtraBase>& Extra : Extras) { Extra->ProcessEdge(this, Edges[Index]); } }
		}
	}

	void FPath::ExtraComputingDone()
	{
		for (const TSharedPtr<FPathEdgeExtraBase>& Extra : Extras) { Extra->ProcessingDone(this); }
		Extras.Empty(); // So we don't update them anymore
	}

	void FPath::ComputeAllEdgeExtra()
	{
		if (NumEdges == 1)
		{
			for (const TSharedPtr<FPathEdgeExtraBase>& Extra : Extras) { Extra->ProcessSingleEdge(this, Edges[0]); }
		}
		else
		{
			for (const TSharedPtr<FPathEdgeExtraBase>& Extra : Extras) { Extra->ProcessFirstEdge(this, Edges[0]); }
			for (int i = 1; i < LastEdge; i++) { for (const TSharedPtr<FPathEdgeExtraBase>& Extra : Extras) { Extra->ProcessEdge(this, Edges[i]); } }
			for (const TSharedPtr<FPathEdgeExtraBase>& Extra : Extras) { Extra->ProcessLastEdge(this, Edges[LastEdge]); }
		}

		ExtraComputingDone();
	}

	void FPath::BuildPath(const double Expansion)
	{
		if (bClosedLoop) { NumEdges = NumPoints; }
		else { NumEdges = LastIndex; }
		LastEdge = NumEdges - 1;

		Edges.SetNumUninitialized(NumEdges);
		for (int i = 0; i < NumEdges; i++)
		{
			const FPathEdge& E = (Edges[i] = FPathEdge(i, (i + 1) % NumPoints, Positions, Expansion));
			Bounds += E.Bounds.GetBox();
		}
	}

#pragma region FPathEdgeLength

	void FPathEdgeLength::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		const double Dist = FVector::Dist(Path->GetPos_Unsafe(Edge.Start), Path->GetPos_Unsafe(Edge.End));
		GetMutable(Edge.Start) = Dist;
		TotalLength += Dist;
	}

	void FPathEdgeLength::ProcessingDone(const FPath* Path)
	{
		TPathEdgeExtra<double>::ProcessingDone(Path);
		CumulativeLength.SetNumUninitialized(Data.Num());
		CumulativeLength[0] = Data[0];
		for (int i = 1; i < Data.Num(); i++) { CumulativeLength[i] = CumulativeLength[i - 1] + Data[i]; }
	}

#pragma endregion

#pragma region FPathEdgeLength

	void FPathEdgeLengthSquared::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		const double Dist = FVector::DistSquared(Path->GetPos_Unsafe(Edge.Start), Path->GetPos_Unsafe(Edge.End));
		GetMutable(Edge.Start) = Dist;
	}

#pragma endregion

#pragma region FPathEdgeNormal

	void FPathEdgeNormal::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		GetMutable(Edge.Start) = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
	}

#pragma endregion

#pragma region FPathEdgeBinormal

	void FPathEdgeBinormal::ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge)
	{
		if (Path->IsClosedLoop())
		{
			ProcessEdge(Path, Edge);
			return;
		}

		const FVector N = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
		Normals[Edge.Start] = N;
		GetMutable(Edge.Start) = N;
	}

	void FPathEdgeBinormal::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		const FVector N = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
		Normals[Edge.Start] = N;

		const FVector A = Path->DirToPrevPoint(Edge.Start);
		FVector D = FQuat(FVector::CrossProduct(A, Edge.Dir).GetSafeNormal(), FMath::Acos(FVector::DotProduct(A, Edge.Dir)) * 0.5f).RotateVector(A);

		if (FVector::DotProduct(N, D) < 0.0f) { D *= -1; }

		GetMutable(Edge.Start) = D;
	}

	void FPathEdgeBinormal::ProcessLastEdge(const FPath* Path, const FPathEdge& Edge)
	{
		if (Path->IsClosedLoop())
		{
			ProcessEdge(Path, Edge);
			return;
		}

		const FVector C = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();

		Normals[Edge.Start] = C;
		GetMutable(Edge.Start) = C;
	}

#pragma endregion

#pragma region FPathEdgeAvgNormal

	void FPathEdgeAvgNormal::ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge)
	{
		if (Path->IsClosedLoop())
		{
			ProcessEdge(Path, Edge);
			return;
		}

		GetMutable(Edge.Start) = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
	}

	void FPathEdgeAvgNormal::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		const FVector A = FVector::CrossProduct(Up, Path->DirToPrevPoint(Edge.Start) * -1).GetSafeNormal();
		const FVector B = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
		GetMutable(Edge.Start) = FMath::Lerp(A, B, 0.5).GetSafeNormal();
	}

	void FPathEdgeAvgNormal::ProcessLastEdge(const FPath* Path, const FPathEdge& Edge)
	{
		if (Path->IsClosedLoop())
		{
			ProcessEdge(Path, Edge);
			return;
		}

		GetMutable(Edge.Start) = FVector::CrossProduct(Up, Edge.Dir).GetSafeNormal();
	}

#pragma endregion

#pragma region FPathEdgeHalfAngle

	void FPathEdgeHalfAngle::ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge)
	{
		if (Path->IsClosedLoop())
		{
			ProcessEdge(Path, Edge);
			return;
		}

		GetMutable(Edge.Start) = PI;
	}

	void FPathEdgeHalfAngle::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		GetMutable(Edge.Start) = FMath::Acos(FVector::DotProduct(Path->DirToPrevPoint(Edge.Start), Edge.Dir));
	}

	void FPathEdgeHalfAngle::ProcessLastEdge(const FPath* Path, const FPathEdge& Edge)
	{
		if (Path->IsClosedLoop())
		{
			ProcessEdge(Path, Edge);
			return;
		}

		GetMutable(Edge.Start) = PI;
	}

#pragma endregion

#pragma region FPathEdgeAngle

	void FPathEdgeFullAngle::ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge)
	{
		if (Path->IsClosedLoop())
		{
			ProcessEdge(Path, Edge);
			return;
		}

		GetMutable(Edge.Start) = PI;
	}

	void FPathEdgeFullAngle::ProcessEdge(const FPath* Path, const FPathEdge& Edge)
	{
		GetMutable(Edge.Start) = PCGExMath::GetAngle(Path->DirToPrevPoint(Edge.Start) * -1, Edge.Dir);
	}

	void FPathEdgeFullAngle::ProcessLastEdge(const FPath* Path, const FPathEdge& Edge)
	{
		if (Path->IsClosedLoop())
		{
			ProcessEdge(Path, Edge);
			return;
		}

		GetMutable(Edge.Start) = PI;
	}

	TSharedPtr<FPath> MakePath(const UPCGBasePointData* InPointData, const double Expansion, const bool bClosedLoop)
	{
		return MakePath(InPointData->GetConstTransformValueRange(), Expansion, bClosedLoop);
	}

	TSharedPtr<FPath> MakePath(const TConstPCGValueRange<FTransform>& InTransforms, const double Expansion, const bool bClosedLoop)
	{
		if (bClosedLoop)
		{
			PCGEX_MAKE_SHARED(P, TPath<true>, InTransforms, Expansion)
			return StaticCastSharedPtr<FPath>(P);
		}

		PCGEX_MAKE_SHARED(P, TPath<false>, InTransforms, Expansion)
		return StaticCastSharedPtr<FPath>(P);
	}

	FTransform GetClosestTransform(const FPCGSplineStruct& InSpline, const FVector& InLocation, const bool bUseScale)
	{
		return InSpline.GetTransformAtSplineInputKey(InSpline.FindInputKeyClosestToWorldLocation(InLocation), ESplineCoordinateSpace::World, bUseScale);
	}

	FTransform GetClosestTransform(const TSharedPtr<const FPCGSplineStruct>& InSpline, const FVector& InLocation, const bool bUseScale)
	{
		return InSpline->GetTransformAtSplineInputKey(InSpline->FindInputKeyClosestToWorldLocation(InLocation), ESplineCoordinateSpace::World, bUseScale);
	}

	TSharedPtr<FPCGSplineStruct> MakeSplineFromPoints(const UPCGBasePointData* InData, const EPCGExSplinePointTypeRedux InPointType, const bool bClosedLoop)
	{
		const int32 NumPoints = InData->GetNumPoints();
		if (NumPoints < 2) { return nullptr; }

		TArray<FSplinePoint> SplinePoints;
		PCGEx::InitArray(SplinePoints, NumPoints);

		ESplinePointType::Type PointType = ESplinePointType::Linear;

		bool bComputeTangents = false;
		switch (InPointType)
		{
		case EPCGExSplinePointTypeRedux::Linear:
			PointType = ESplinePointType::CurveCustomTangent;
			bComputeTangents = true;
			break;
		case EPCGExSplinePointTypeRedux::Curve:
			PointType = ESplinePointType::Curve;
			break;
		case EPCGExSplinePointTypeRedux::Constant:
			PointType = ESplinePointType::Constant;
			break;
		case EPCGExSplinePointTypeRedux::CurveClamped:
			PointType = ESplinePointType::CurveClamped;
			break;
		}

		TConstPCGValueRange<FTransform> Transforms = InData->GetConstTransformValueRange();

		if (bComputeTangents)
		{
			const int32 MaxIndex = NumPoints - 1;

			for (int i = 0; i < NumPoints; i++)
			{
				const FTransform TR = Transforms[i];
				const FVector PtLoc = TR.GetLocation();

				const FVector PrevDir = (Transforms[i == 0 ? bClosedLoop ? MaxIndex : 0 : i - 1].GetLocation() - PtLoc) * -1;
				const FVector NextDir = Transforms[i == MaxIndex ? bClosedLoop ? 0 : i : i + 1].GetLocation() - PtLoc;
				const FVector Tangent = FMath::Lerp(PrevDir, NextDir, 0.5).GetSafeNormal() * 0.01;

				SplinePoints[i] = FSplinePoint(
					static_cast<float>(i),
					TR.GetLocation(),
					Tangent,
					Tangent,
					TR.GetRotation().Rotator(),
					TR.GetScale3D(),
					PointType);
			}
		}
		else
		{
			for (int i = 0; i < NumPoints; i++)
			{
				const FTransform TR = Transforms[i];
				SplinePoints[i] = FSplinePoint(
					static_cast<float>(i),
					TR.GetLocation(),
					FVector::ZeroVector,
					FVector::ZeroVector,
					TR.GetRotation().Rotator(),
					TR.GetScale3D(),
					PointType);
			}
		}

		PCGEX_MAKE_SHARED(SplineStruct, FPCGSplineStruct)
		SplineStruct->Initialize(SplinePoints, bClosedLoop, FTransform::Identity);
		return SplineStruct;
	}

#pragma endregion
}


bool FPCGExSplineMeshMutationDetails::Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade)
{
	if (!bPushStart && !bPushEnd) { return true; }

	if (bPushStart)
	{
		StartAmount = GetValueSettingStartPush();
		if (!StartAmount->Init(InContext, InDataFacade)) { return false; }
	}

	if (bPushEnd)
	{
		EndAmount = GetValueSettingEndPush();
		if (!EndAmount->Init(InContext, InDataFacade)) { return false; }
	}

	return true;
}

void FPCGExSplineMeshMutationDetails::Mutate(const int32 PointIndex, PCGExPaths::FSplineMeshSegment& InSegment)
{
	if (!bPushStart && !bPushEnd) { return; }

	const double Size = (bPushStart || bPushEnd) && (bRelativeStart || bRelativeEnd) ? FVector::Dist(InSegment.Params.StartPos, InSegment.Params.EndPos) : 1;
	const FVector StartDir = InSegment.Params.StartTangent.GetSafeNormal();
	const FVector EndDir = InSegment.Params.EndTangent.GetSafeNormal();

	if (bPushStart)
	{
		const double Factor = StartAmount->Read(PointIndex);
		const double Dist = bRelativeStart ? Size * Factor : Factor;

		InSegment.Params.StartPos -= StartDir * Dist;
		InSegment.Params.StartTangent = StartDir * (InSegment.Params.StartTangent.Size() + Dist * 3);
	}

	if (bPushEnd)
	{
		const double Factor = EndAmount->Read(PointIndex);
		const double Dist = bRelativeEnd ? Size * Factor : Factor;

		InSegment.Params.EndPos += EndDir * Dist;
		InSegment.Params.EndTangent = EndDir * (InSegment.Params.EndTangent.Size() + Dist * 3);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathsHelpers.h"

#include "Data/PCGSplineData.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGExArrayHelpers.h"
#include "Paths/PCGExPathIntersectionDetails.h"
#include "Paths/PCGExPath.h"
#include "Paths/PCGExPathsCommon.h"

namespace PCGExPaths
{
	namespace Helpers
	{
		void SetClosedLoop(UPCGData* InData, const bool bIsClosedLoop)
		{
			PCGExData::Helpers::SetDataValue(InData, Labels::ClosedLoopIdentifier, bIsClosedLoop);
		}

		void SetClosedLoop(const TSharedPtr<PCGExData::FPointIO>& InData, const bool bIsClosedLoop)
		{
			SetClosedLoop(InData->GetOut(), bIsClosedLoop);
		}

		bool GetClosedLoop(const UPCGData* InData)
		{
			if (const UPCGSplineData* SplineData = Cast<UPCGSplineData>(InData)) { return SplineData->IsClosed(); }

			const FPCGMetadataAttribute<bool>* Attr = PCGExMetaHelpers::TryGetConstAttribute<bool>(InData, Labels::ClosedLoopIdentifier);
			return Attr ? PCGExData::Helpers::ReadDataValue(Attr) : false;
		}

		bool GetClosedLoop(const TSharedPtr<PCGExData::FPointIO>& InData)
		{
			return GetClosedLoop(InData->GetIn());
		}

		void SetIsHole(UPCGData* InData, const bool bIsHole)
		{
			FPCGMetadataAttribute<bool>* Attr = PCGExMetaHelpers::TryGetMutableAttribute<bool>(InData, Labels::HoleIdentifier);

			if (!bIsHole)
			{
				if (Attr) { InData->Metadata->DeleteAttribute(Labels::HoleIdentifier); }
				return;
			}

			if (!Attr) { Attr = InData->Metadata->CreateAttribute<bool>(Labels::HoleIdentifier, bIsHole, true, true); }
			PCGExData::Helpers::SetDataValue(Attr, bIsHole);
		}

		void SetIsHole(const TSharedPtr<PCGExData::FPointIO>& InData, const bool bIsHole)
		{
			SetIsHole(InData->GetOut(), bIsHole);
		}

		bool GetIsHole(const UPCGData* InData)
		{
			if (const UPCGSplineData* SplineData = Cast<UPCGSplineData>(InData)) { return SplineData->IsClosed(); }

			const FPCGMetadataAttribute<bool>* Attr = PCGExMetaHelpers::TryGetConstAttribute<bool>(InData, Labels::HoleIdentifier);
			return Attr ? PCGExData::Helpers::ReadDataValue(Attr) : false;
		}

		bool GetIsHole(const TSharedPtr<PCGExData::FPointIO>& InData)
		{
			return GetIsHole(InData->GetIn());
		}

		void FetchPrevNext(const TSharedPtr<PCGExData::FFacade>& InFacade, const TArray<PCGExMT::FScope>& Loops)
		{
			if (Loops.Num() <= 1) { return; }
			// Fetch necessary bits for prev/next data to be valid during parallel processing
			InFacade->Fetch(PCGExMT::FScope(0, 1));
			for (int i = 1; i < Loops.Num(); ++i) { InFacade->Fetch(PCGExMT::FScope(Loops[i - 1].End - 1, 2)); }
		}

		TSharedPtr<FPath> MakePath(const UPCGBasePointData* InPointData, const double Expansion)
		{
			return MakeShared<FPath>(InPointData->GetConstTransformValueRange(), GetClosedLoop(InPointData), Expansion);
		}

		double GetPathLength(const TSharedPtr<FPath>& InPath)
		{
			FPathMetrics Metrics(InPath->GetPos(0));
			for (int i = 0; i < InPath->NumPoints; i++) { Metrics.Add(InPath->GetPos(i)); }
			if (InPath->IsClosedLoop()) { Metrics.Add(InPath->GetPos(0)); }
			return Metrics.Length;
		}

		FTransform GetClosestTransform(const FPCGSplineStruct& InSpline, const FVector& InLocation, const bool bUseScale)
		{
			return InSpline.GetTransformAtSplineInputKey(InSpline.FindInputKeyClosestToWorldLocation(InLocation), ESplineCoordinateSpace::World, bUseScale);
		}

		FTransform GetClosestTransform(const TSharedPtr<const FPCGSplineStruct>& InSpline, const FVector& InLocation, const bool bUseScale)
		{
			return InSpline->GetTransformAtSplineInputKey(InSpline->FindInputKeyClosestToWorldLocation(InLocation), ESplineCoordinateSpace::World, bUseScale);
		}

		TSharedPtr<FPCGSplineStruct> MakeSplineFromPoints(const TConstPCGValueRange<FTransform>& InTransforms, const EPCGExSplinePointTypeRedux InPointType, const bool bClosedLoop, const bool bSmoothLinear)
		{
			const int32 NumPoints = InTransforms.Num();
			if (NumPoints < 2) { return nullptr; }

			TArray<FSplinePoint> SplinePoints;
			PCGExArrayHelpers::InitArray(SplinePoints, NumPoints);

			ESplinePointType::Type PointType = ESplinePointType::Linear;
			bool bComputeTangents = false;

			switch (InPointType)
			{
			case EPCGExSplinePointTypeRedux::Linear: if (bSmoothLinear)
				{
					PointType = ESplinePointType::CurveCustomTangent;
					bComputeTangents = true;
				}
				break;
			case EPCGExSplinePointTypeRedux::Curve: PointType = ESplinePointType::Curve;
				break;
			case EPCGExSplinePointTypeRedux::Constant: PointType = ESplinePointType::Constant;
				break;
			case EPCGExSplinePointTypeRedux::CurveClamped: PointType = ESplinePointType::CurveClamped;
				break;
			}

			if (bComputeTangents)
			{
				const int32 MaxIndex = NumPoints - 1;

				for (int i = 0; i < NumPoints; i++)
				{
					const FTransform TR = InTransforms[i];
					const FVector PtLoc = TR.GetLocation();

					const FVector PrevDir = (InTransforms[i == 0 ? bClosedLoop ? MaxIndex : 0 : i - 1].GetLocation() - PtLoc) * -1;
					const FVector NextDir = InTransforms[i == MaxIndex ? bClosedLoop ? 0 : i : i + 1].GetLocation() - PtLoc;
					const FVector Tangent = FMath::Lerp(PrevDir, NextDir, 0.5).GetSafeNormal() * 0.01;

					SplinePoints[i] = FSplinePoint(static_cast<float>(i), TR.GetLocation(), Tangent, Tangent, TR.GetRotation().Rotator(), TR.GetScale3D(), PointType);
				}
			}
			else
			{
				for (int i = 0; i < NumPoints; i++)
				{
					const FTransform TR = InTransforms[i];
					SplinePoints[i] = FSplinePoint(static_cast<float>(i), TR.GetLocation(), FVector::ZeroVector, FVector::ZeroVector, TR.GetRotation().Rotator(), TR.GetScale3D(), PointType);
				}
			}

			PCGEX_MAKE_SHARED(SplineStruct, FPCGSplineStruct)
			SplineStruct->Initialize(SplinePoints, bClosedLoop, FTransform::Identity);
			return SplineStruct;
		}

		TSharedPtr<FPCGSplineStruct> MakeSplineCopy(const FPCGSplineStruct& Original)
		{
			const int32 NumPoints = Original.GetNumberOfPoints();
			if (NumPoints < 1) { return nullptr; }

			const FInterpCurveVector& SplinePositions = Original.GetSplinePointsPosition();
			TArray<FSplinePoint> SplinePoints;

			auto GetPointType = [](const EInterpCurveMode Mode)-> ESplinePointType::Type
			{
				switch (Mode)
				{
				case CIM_Linear: return ESplinePointType::Type::Linear;
				case CIM_CurveAuto: return ESplinePointType::Type::Curve;
				case CIM_Constant: return ESplinePointType::Type::Constant;
				case CIM_CurveUser: return ESplinePointType::Type::CurveCustomTangent;
				case CIM_CurveAutoClamped: return ESplinePointType::Type::CurveClamped;
				default: case CIM_Unknown:
				case CIM_CurveBreak: return ESplinePointType::Type::Curve;
				}
			};

			PCGExArrayHelpers::InitArray(SplinePoints, NumPoints);

			for (int i = 0; i < NumPoints; i++)
			{
				const FTransform TR = Original.GetTransformAtSplineInputKey(i, ESplineCoordinateSpace::Local);
				SplinePoints[i] = FSplinePoint(static_cast<float>(i), TR.GetLocation(), SplinePositions.Points[i].ArriveTangent, SplinePositions.Points[i].LeaveTangent, TR.GetRotation().Rotator(), TR.GetScale3D(), GetPointType(SplinePositions.Points[i].InterpMode));
			}

			PCGEX_MAKE_SHARED(SplineStruct, FPCGSplineStruct)
			SplineStruct->Initialize(SplinePoints, Original.bClosedLoop, Original.GetTransform());
			return SplineStruct;
		}

		PCGExMath::FClosestPosition FindClosestIntersection(const TArray<TSharedPtr<FPath>>& Paths, const FPCGExPathIntersectionDetails& InDetails, const PCGExMath::FSegment& InSegment, int32& OutPathIndex)
		{
			OutPathIndex = -1;

			PCGExMath::FClosestPosition Intersection(InSegment.A);

			for (int i = 0; i < Paths.Num(); i++)
			{
				PCGExMath::FClosestPosition LocalIntersection = Paths[i]->FindClosestIntersection(InDetails, InSegment);
				if (!LocalIntersection) { continue; }
				if (Intersection.Update(LocalIntersection, LocalIntersection.Index)) { OutPathIndex = i; }
			}

			return Intersection;
		}

		PCGExMath::FClosestPosition FindClosestIntersection(const TArray<TSharedPtr<FPath>>& Paths, const FPCGExPathIntersectionDetails& InDetails, const PCGExMath::FSegment& InSegment, int32& OutPathIndex, PCGExMath::FClosestPosition& OutClosestPosition)
		{
			OutPathIndex = -1;

			PCGExMath::FClosestPosition Intersection(InSegment.A);

			for (int i = 0; i < Paths.Num(); i++)
			{
				PCGExMath::FClosestPosition LocalIntersection = Paths[i]->FindClosestIntersection(InDetails, InSegment, OutClosestPosition);

				if (OutClosestPosition.Index == -2) { OutClosestPosition.Index = i; }

				if (!LocalIntersection) { continue; }
				if (Intersection.Update(LocalIntersection, LocalIntersection.Index)) { OutPathIndex = i; }
			}

			return Intersection;
		}
	}

	FCrossing::FCrossing(const uint64 InHash, const FVector& InLocation, const double InAlpha, const bool InIsPoint, const FVector& InDir)
		: Hash(InHash), Location(InLocation), Alpha(InAlpha), bIsPoint(InIsPoint), Dir(InDir)
	{
	}

	bool FPathEdgeCrossings::FindSplit(const TSharedPtr<FPath>& Path, const FPathEdge& Edge, const TSharedPtr<FPathEdgeLength>& PathLength, const TSharedPtr<FPath>& OtherPath, const FPathEdge& OtherEdge, const FPCGExPathEdgeIntersectionDetails& InIntersectionDetails)
	{
		if (!OtherPath->IsEdgeValid(OtherEdge)) { return false; }

		const FVector& A1 = Path->GetPos(Edge.Start);
		const FVector& B1 = Path->GetPos(Edge.End);
		const FVector& A2 = OtherPath->GetPos(OtherEdge.Start);
		const FVector& B2 = OtherPath->GetPos(OtherEdge.End);

		if (A1 == A2 || A1 == B2 || A2 == B1 || B2 == B1) { return false; }

		const FVector CrossDir = OtherEdge.Dir;

		if (InIntersectionDetails.bUseMinAngle || InIntersectionDetails.bUseMaxAngle)
		{
			if (!InIntersectionDetails.CheckDot(FMath::Abs(FVector::DotProduct((B1 - A1).GetSafeNormal(), CrossDir)))) { return false; }
		}

		FVector A;
		FVector B;
		FMath::SegmentDistToSegment(A1, B1, A2, B2, A, B);

		if (A == A1 || A == B1) { return false; } // On local point

		const double Dist = FVector::DistSquared(A, B);
		const bool bColloc = (B == A2 || B == B2); // On crossing point

		if (Dist >= InIntersectionDetails.ToleranceSquared) { return false; }

		Crossings.Emplace(PCGEx::H64(OtherEdge.Start, OtherPath->IOIndex), FMath::Lerp(A, B, 0.5), FVector::Dist(A1, A) / PathLength->Get(Edge), bColloc, CrossDir);

		return true;
	}

	bool FPathEdgeCrossings::RemoveCrossing(const int32 EdgeStartIndex, const int32 IOIndex)
	{
		const uint64 H = PCGEx::H64(EdgeStartIndex, IOIndex);
		for (int i = 0; i < Crossings.Num(); i++)
		{
			if (Crossings[i].Hash == H)
			{
				Crossings.RemoveAt(i);
				return true;
			}
		}
		return false;
	}

	bool FPathEdgeCrossings::RemoveCrossing(const TSharedPtr<FPath>& Path, const int32 EdgeStartIndex)
	{
		return RemoveCrossing(EdgeStartIndex, Path->IOIndex);
	}

	bool FPathEdgeCrossings::RemoveCrossing(const TSharedPtr<FPath>& Path, const FPathEdge& Edge)
	{
		return RemoveCrossing(Edge.Start, Path->IOIndex);
	}

	void FPathEdgeCrossings::SortByAlpha()
	{
		if (Crossings.Num() <= 1) { return; }
		Crossings.Sort([&](const FCrossing& A, const FCrossing& B) { return A.Alpha < B.Alpha; });
	}

	void FPathEdgeCrossings::SortByHash()
	{
		if (Crossings.Num() <= 1) { return; }
		Crossings.Sort([&](const FCrossing& A, const FCrossing& B) { return PCGEx::H64A(A.Hash) < PCGEx::H64A(B.Hash); });
	}

	void FPathInclusionHelper::AddPath(const TSharedPtr<FPath>& InPath, const double Tolerance)
	{
		bool bAlreadyInSet = false;
		PathsSet.Add(InPath->Idx, &bAlreadyInSet);

		if (bAlreadyInSet) { return; }

		FInclusionInfos& NewInfos = IdxMap.Add(InPath->Idx, FInclusionInfos());

		for (const TSharedPtr<FPath>& OtherPath : Paths)
		{
			FInclusionInfos& OtherInfos = IdxMap[OtherPath->Idx];

			if (OtherPath->Contains(InPath->GetPositions(), Tolerance))
			{
				NewInfos.Depth++;
				NewInfos.bOdd = NewInfos.Depth % 2 != 0;
				OtherInfos.Children++;
			}
			else if (InPath->Contains(OtherPath->GetPositions(), Tolerance))
			{
				OtherInfos.Depth++;
				OtherInfos.bOdd = OtherInfos.Depth % 2 != 0;
				NewInfos.Children++;
			}
		}

		Paths.Add(InPath);
	}

	void FPathInclusionHelper::AddPaths(const TArrayView<TSharedPtr<FPath>> InPaths, const double Tolerance)
	{
		const int32 Reserve = IdxMap.Num() + InPaths.Num();
		PathsSet.Reserve(Reserve);
		Paths.Reserve(Reserve);
		IdxMap.Reserve(Reserve);

		for (const TSharedPtr<FPath>& Path : InPaths) { AddPath(Path, Tolerance); }
	}

	bool FPathInclusionHelper::Find(const int32 Idx, FInclusionInfos& OutInfos) const
	{
		const FInclusionInfos* Infos = IdxMap.Find(Idx);
		if (!Infos) { return false; }

		OutInfos = *Infos;
		return true;
	}
}

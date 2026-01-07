// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Containers/PCGExScopedContainers.h"
#include "Data/Utils/PCGExDataFilterDetails.h"
#include "Clusters/PCGExEdge.h"
#include "Math/PCGExProjectionDetails.h"
#include "Math/PCGExWinding.h"

class UPCGBasePointData;
enum class EPCGExPointPropertyOutput : uint8;
struct FPCGExCellConstraintsDetails;

namespace PCGExData
{
	struct FMutablePoint;
}

struct FPCGExNodeSelectionDetails;

namespace PCGExMath
{
	struct FTriangle;
}

namespace PCGExClusters
{
	class FCluster;
}

namespace PCGExClusters
{
	namespace Labels
	{
		const FName SourceEdgeConstrainsFiltersLabel = FName("ConstrainedEdgeFilters");
		const FName SourceHolesLabel = FName("Holes");
	}

	PCGEXCORE_API
	void SetPointProperty(PCGExData::FMutablePoint& InPoint, const double InValue, const EPCGExPointPropertyOutput InProperty);

#pragma region Cell

	enum class ECellResult : uint8
	{
		Unknown = 0,
		Success,
		Duplicate,
		Leaf,
		Hole,
		WrongAspect,
		OutsidePointsLimit,
		OutsideBoundsLimit,
		OutsideAreaLimit,
		OutsidePerimeterLimit,
		OutsideCompactnessLimit,
		OutsideSegmentsLimit,
		OpenCell,
		WrapperCell,
		MalformedCluster,
	};

	class FCell;

	class PCGEXCORE_API FHoles : public TSharedFromThis<FHoles>
	{
		// TODO : Need to use per-processor hole instance to match best fit projection
	protected:
		mutable FRWLock ProjectionLock;

		TSharedRef<PCGExData::FFacade> PointDataFacade;
		FPCGExGeo2DProjectionDetails ProjectionDetails;
		TArray<FVector2D> ProjectedPoints;

	public:
		explicit FHoles(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const FPCGExGeo2DProjectionDetails& InProjectionDetails)
			: PointDataFacade(InPointDataFacade), ProjectionDetails(InProjectionDetails)
		{
			if (ProjectionDetails.Method == EPCGExProjectionMethod::Normal) { ProjectionDetails.Init(PointDataFacade); }
		}

		bool Overlaps(const TArray<FVector2D>& Polygon);
	};

	class PCGEXCORE_API FCellConstraints : public TSharedFromThis<FCellConstraints>
	{
	protected:
		PCGExMT::TH64SetShards<> UniquePathsHashSet;
		PCGExMT::TH64SetShards<> UniqueStartHalfEdgesHash;

	public:
		EPCGExWinding Winding = EPCGExWinding::CounterClockwise;

		bool bConcaveOnly = false;
		bool bConvexOnly = false;
		bool bKeepCellsWithLeaves = true;
		bool bDuplicateLeafPoints = false;

		int32 MaxPointCount = MAX_int32;
		int32 MinPointCount = MIN_int32;

		double MaxBoundsSize = MAX_dbl;
		double MinBoundsSize = MIN_dbl_neg;

		double MaxArea = MAX_dbl;
		double MinArea = MIN_dbl_neg;

		double MaxPerimeter = MAX_dbl;
		double MinPerimeter = MIN_dbl_neg;

		double MaxSegmentLength = MAX_dbl;
		double MinSegmentLength = MIN_dbl_neg;

		double MaxCompactness = MAX_dbl;
		double MinCompactness = MIN_dbl_neg;

		double WrapperClassificationTolerance = 0;
		bool bBuildWrapper = true;

		TSharedPtr<FCell> WrapperCell;
		TSharedPtr<FHoles> Holes;

		FCellConstraints()
		{
		}

		explicit FCellConstraints(const FPCGExCellConstraintsDetails& InDetails);

		void Reserve(const int32 InCellHashReserve);
		bool ContainsSignedEdgeHash(const uint64 Hash);
		bool IsUniqueStartHalfEdge(const uint64 Hash);
		bool IsUniqueCellHash(const TSharedPtr<FCell>& InCell);
		void BuildWrapperCell(const TSharedRef<FCluster>& InCluster, const TArray<FVector2D>& ProjectedPositions, const TSharedPtr<FCellConstraints>& InConstraints = nullptr);

		void Cleanup();
	};

	struct PCGEXCORE_API FCellData
	{
		int8 bIsValid = 0;
		uint32 CellHash = 0;
		FBox Bounds = FBox(ForceInit);
		FVector Centroid = FVector::ZeroVector;
		double Area = 0;
		double Perimeter = 0;
		double Compactness = 0;
		bool bIsConvex = true;
		bool bIsClockwise = false;
		bool bIsClosedLoop = false;

		FCellData() = default;
	};

	class PCGEXCORE_API FCell : public TSharedFromThis<FCell>
	{
	protected:
		int32 Sign = 0;
		uint64 CellHash = 0;

	public:
		TArray<int32> Nodes;
		TSharedRef<FCellConstraints> Constraints;

		FCellData Data = FCellData();

		PCGExGraphs::FLink Seed = PCGExGraphs::FLink(-1, -1);

		bool bBuiltSuccessfully = false;

		TArray<FVector2D> Polygon; // TODO : Make this a TArray<FVector2D> instead 

		int32 CustomIndex = -1;

		explicit FCell(const TSharedRef<FCellConstraints>& InConstraints)
			: Constraints(InConstraints)
		{
			Data.bIsValid = true;
		}

		~FCell() = default;

		uint64 GetCellHash();

		ECellResult BuildFromCluster(const PCGExGraphs::FLink InSeedLink, TSharedRef<FCluster> InCluster, const TArray<FVector2D>& ProjectedPositions);

		ECellResult BuildFromCluster(const FVector& SeedPosition, const TSharedRef<FCluster>& InCluster, const TArray<FVector2D>& ProjectedPositions, const FVector& UpVector = FVector::UpVector, const FPCGExNodeSelectionDetails* Picking = nullptr);

		ECellResult BuildFromPath(const TArray<FVector2D>& ProjectedPositions);

		void PostProcessPoints(UPCGBasePointData* InMutablePoints);
	};

#pragma endregion
}

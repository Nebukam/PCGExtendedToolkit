// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Geometry/PCGExGeoPrimtives.h"
#include "Graph/PCGExCluster.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "Paths/PCGExPaths.h"
#include "GeometryScript/PolygonFunctions.h"

#include "PCGExTopology.generated.h"

UENUM()
enum class EPCGExTopologyOutputType : uint8
{
	PerItem = 1 UMETA(DisplayName = "Per-item Geometry", Tooltip="Output a geometry object per-item"),
	Merged  = 0 UMETA(DisplayName = "Merged Geometry", Tooltip="Output a single geometry that merges all generated topologies"),
};

UENUM()
enum class EPCGExCellShapeTypeOutput : uint8
{
	Both        = 0 UMETA(DisplayName = "Convex & Concave", ToolTip="Output both convex and concave cells"),
	ConvexOnly  = 1 UMETA(DisplayName = "Convex Only", ToolTip="Output only convex cells"),
	ConcaveOnly = 2 UMETA(DisplayName = "Concave Only", ToolTip="Output only concave cells")
};

UENUM()
enum class EPCGExCellSeedLocation : uint8
{
	Original         = 0 UMETA(DisplayName = "Original", ToolTip="Seed position is unchanged"),
	Centroid         = 1 UMETA(DisplayName = "Centroid", ToolTip="Place the seed at the centroid of the path"),
	PathBoundsCenter = 2 UMETA(DisplayName = "Path bounds center", ToolTip="Place the seed at the center of the path' bounds"),
	FirstNode        = 3 UMETA(DisplayName = "First Node", ToolTip="Place the seed on the position of the node that started the cell."),
	LastNode         = 4 UMETA(DisplayName = "Last Node", ToolTip="Place the seed on the position of the node that ends the cell.")
};

UENUM()
enum class EPCGExCellSeedBounds : uint8
{
	Original           = 0 UMETA(DisplayName = "Original", ToolTip="Seed bounds is unchanged"),
	MatchCell          = 1 UMETA(DisplayName = "Match Cell", ToolTip="Seed bounds match cell bounds"),
	MatchPathResetQuat = 2 UMETA(DisplayName = "Match Cell (with rotation reset)", ToolTip="Seed bounds match cell bounds, and rotation is reset"),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCellConstraintsDetails
{
	GENERATED_BODY()

	FPCGExCellConstraintsDetails()
	{
	}

	explicit FPCGExCellConstraintsDetails(bool InUsedForPaths)
		: bUsedForPaths(InUsedForPaths)
	{
	}

	UPROPERTY()
	bool bUsedForPaths = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExCellShapeTypeOutput AspectFilter = EPCGExCellShapeTypeOutput::Both;

	/** Ensure there's no duplicate cells. This can happen when using seed-based search where multiple seed yield the same final cell. Just leave that on, honestly.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bUsedForPaths", EditConditionHides, HideEditConditionToggle), AdvancedDisplay)
	bool bDedupeCells = true;

	/** Keep only cells that closed gracefully; i.e connect to their start node */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bClosedCellsOnly = true;

	/** Whether to keep cells that include dead ends wrapping */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bKeepCellsWithLeaves = true;

	/** Whether to duplicate dead end points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bKeepCellsWithLeaves && bUsedForPaths", EditConditionHides, HideEditConditionToggle))
	bool bDuplicateLeafPoints = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditConditionHides))
	bool bOmitWrappingBounds = true;

	/** Omit cells with bounds that closely match the ones from the input set */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOmitWrappingBounds", ClampMin=0))
	double WrappingBoundsSizeTolerance = 100;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitBelowBoundsSize = false;

	/** Omit cells whose bounds size.length is smaller than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOmitBelowBoundsSize", ClampMin=0))
	double MinBoundsSize = 3;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitAboveBoundsSize = false;

	/** Omit cells whose bounds size.length is larger than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOmitAboveBoundsSize", ClampMin=0))
	double MaxBoundsSize = 500;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitBelowPointCount = false;

	/** Omit cells whose point count is smaller than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOmitBelowPointCount", ClampMin=0))
	int32 MinPointCount = 3;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitAbovePointCount = false;

	/** Omit cells whose point count is larger than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOmitAbovePointCount", ClampMin=0))
	int32 MaxPointCount = 500;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitBelowArea = false;

	/** Omit cells whose area is smaller than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOmitBelowArea", ClampMin=0))
	double MinArea = 3;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitAboveArea = false;

	/** Omit cells whose area is larger than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOmitAboveArea", ClampMin=0))
	double MaxArea = 500;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitBelowPerimeter = false;

	/** Omit cells whose perimeter is smaller than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOmitBelowPerimeter", ClampMin=0))
	double MinPerimeter = 3;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitAbovePerimeter = false;

	/** Omit cells whose perimeter is larger than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOmitAbovePerimeter", ClampMin=0))
	double MaxPerimeter = 500;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitBelowSegmentLength = false;

	/** Omit cells that contains any segment which length is smaller than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOmitBelowSegmentLength", ClampMin=0))
	double MinSegmentLength = 3;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitAboveSegmentLength = false;

	/** Omit cells that contains any segment which length is larger than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOmitAboveSegmentLength", ClampMin=0))
	double MaxSegmentLength = 500;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitBelowCompactness = false;

	/** Omit cells that contains any segment which length is smaller than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOmitBelowCompactness", ClampMin=0, ClampMax=1))
	double MinCompactness = 0;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitAboveCompactness = false;

	/** Omit cells that contains any segment which length is larger than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bOmitAboveCompactness", ClampMin=0, ClampMax=1))
	double MaxCompactness = 1;
};

namespace PCGExTopology
{
	class FCell;
}

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCellSeedMutationDetails
{
	GENERATED_BODY()

	FPCGExCellSeedMutationDetails()
	{
	}

	explicit FPCGExCellSeedMutationDetails(bool InUsedForPaths)
		: bUsedForPaths(InUsedForPaths)
	{
	}

	UPROPERTY()
	bool bUsedForPaths = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExCellShapeTypeOutput AspectFilter = EPCGExCellShapeTypeOutput::Both;

	/** Change the good seed position */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExCellSeedLocation Location = EPCGExCellSeedLocation::Centroid;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bMatchCellBounds = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bResetScale = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bResetRotation = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExPointPropertyOutput AreaTo = EPCGExPointPropertyOutput::None;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExPointPropertyOutput PerimeterTo = EPCGExPointPropertyOutput::None;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExPointPropertyOutput CompactnessTo = EPCGExPointPropertyOutput::None;

	void ApplyToPoint(const PCGExTopology::FCell* InCell, FPCGPoint& OutPoint, const TArray<FPCGPoint>& CellPoints) const;
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExTopologyDetails
{
	GENERATED_BODY()

	FPCGExTopologyDetails()
	{
	}

	UPROPERTY(EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TSoftObjectPtr<UMaterialInterface> Material;

	UPROPERTY(EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FLinearColor DefaultVertexColor = FLinearColor::White;
		
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	FGeometryScriptPrimitiveOptions PrimitiveOptions;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	FGeometryScriptPolygonsTriangulationOptions TriangulationOptions;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bQuietTriangulationError = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	FPCGExDynamicMeshDescriptor TemplateDescriptor;
	
};

namespace PCGExTopology
{
	enum class ETriangulationResult : uint8
	{
		Unknown = 0,
		Success,
		InvalidCell,
		TooFewPoints,
		UnsupportedAspect,
		InvalidCluster,
	};

	const FName SourceEdgeConstrainsFiltersLabel = FName("ConstrainedEdgeFilters");

	static FORCEINLINE void MarkTriangle(
		const TSharedPtr<PCGExCluster::FCluster>& InCluster,
		const PCGExGeo::FTriangle& InTriangle)
	{
		FPlatformAtomics::InterlockedExchange(&InCluster->GetNode(InTriangle.Vtx[0])->bValid, 1);
		FPlatformAtomics::InterlockedExchange(&InCluster->GetNode(InTriangle.Vtx[1])->bValid, 1);
		FPlatformAtomics::InterlockedExchange(&InCluster->GetNode(InTriangle.Vtx[2])->bValid, 1);
	}

	static double ComputeArea(const FGeometryScriptSimplePolygon& Polygon)
	{
		int32 NumVtx = Polygon.Vertices->Num();
		if (NumVtx < 3) { return 0; }

		double Area = 0;
		const TArray<FVector2D>& Vertices = *Polygon.Vertices;

		// Shoelace formula
		for (int i = 0; i < NumVtx; ++i)
		{
			const FVector2D& Current = Vertices[i];
			const FVector2D& Next = Vertices[(i + 1) % NumVtx];
			Area += (Current.X * Next.Y) - (Next.X * Current.Y);
		}

		return FMath::Abs(Area) * 0.5;
	}

#pragma region Cell

	enum class ECellResult : uint8
	{
		Unknown = 0,
		Success,
		Duplicate,
		Leaf,
		WrongAspect,
		AbovePointsLimit,
		BelowPointsLimit,
		AboveBoundsLimit,
		BelowBoundsLimit,
		AboveAreaLimit,
		BelowAreaLimit,
		OpenCell,
	};

	class FCell;

	class FCellConstraints : public TSharedFromThis<FCellConstraints>
	{
	public:
		mutable FRWLock UniquePathsBoxHashLock;
		mutable FRWLock UniquePathsStartHashLock;
		TSet<uint32> UniquePathsBoxHash;
		TSet<uint64> UniquePathsStartHash;

		bool bConcaveOnly = false;
		bool bConvexOnly = false;
		bool bKeepCellsWithLeaves = true;
		bool bDuplicateLeafPoints = false;
		bool bClosedLoopOnly = false;

		int32 MaxPointCount = MAX_int32;
		int32 MinPointCount = MIN_int32;

		double MaxBoundsSize = MAX_dbl;
		double MinBoundsSize = MIN_dbl;

		double MaxArea = MAX_dbl;
		double MinArea = MIN_dbl;

		double MaxPerimeter = MAX_dbl;
		double MinPerimeter = MIN_dbl;

		double MaxSegmentLength = MAX_dbl;
		double MinSegmentLength = MIN_dbl;

		double MaxCompactness = MAX_dbl;
		double MinCompactness = MIN_dbl;

		FBox DataBounds = FBox(ForceInit);
		bool bDoWrapperCheck = false;
		double WrapperCheckTolerance = MAX_dbl;

		bool bDedupe = true;

		FCellConstraints()
		{
		}

		explicit FCellConstraints(const FPCGExCellConstraintsDetails& InDetails)
		{
			bDedupe = InDetails.bDedupeCells;
			bConcaveOnly = InDetails.AspectFilter == EPCGExCellShapeTypeOutput::ConcaveOnly;
			bConvexOnly = InDetails.AspectFilter == EPCGExCellShapeTypeOutput::ConvexOnly;
			bClosedLoopOnly = InDetails.bClosedCellsOnly;
			bKeepCellsWithLeaves = InDetails.bKeepCellsWithLeaves;
			bDuplicateLeafPoints = InDetails.bDuplicateLeafPoints;

			if (InDetails.bOmitBelowPointCount) { MinPointCount = InDetails.MinPointCount; }
			if (InDetails.bOmitAbovePointCount) { MaxPointCount = InDetails.MaxPointCount; }

			if (InDetails.bOmitBelowBoundsSize) { MinBoundsSize = InDetails.MinBoundsSize; }
			if (InDetails.bOmitAboveBoundsSize) { MaxBoundsSize = InDetails.MaxBoundsSize; }

			if (InDetails.bOmitBelowArea) { MinArea = InDetails.MinArea; }
			if (InDetails.bOmitAboveArea) { MaxArea = InDetails.MaxArea; }

			if (InDetails.bOmitBelowPerimeter) { MinPerimeter = InDetails.MinPerimeter; }
			if (InDetails.bOmitAbovePerimeter) { MaxPerimeter = InDetails.MaxPerimeter; }

			if (InDetails.bOmitBelowSegmentLength) { MinSegmentLength = InDetails.MinSegmentLength; }
			if (InDetails.bOmitAboveSegmentLength) { MaxSegmentLength = InDetails.MaxSegmentLength; }

			if (InDetails.bOmitBelowCompactness) { MinCompactness = InDetails.MinCompactness; }
			if (InDetails.bOmitAboveCompactness) { MaxCompactness = InDetails.MaxCompactness; }

			bDoWrapperCheck = InDetails.bOmitWrappingBounds;
			WrapperCheckTolerance = InDetails.WrappingBoundsSizeTolerance;
		}

		bool ContainsSignedEdgeHash(const uint64 Hash) const;
		bool IsUniqueStartHash(const uint64 Hash);
		bool IsUniqueCellHash(const FCell* InCell);
	};

	class FCell : public TSharedFromThis<FCell>
	{
	protected:
		int32 Sign = 0;

	public:
		TArray<int32> Nodes;
		FBox Bounds = FBox(ForceInit);
		TSharedRef<FCellConstraints> Constraints;
		FVector Centroid = FVector::ZeroVector;

		double Area = 0;
		double Perimeter = 0;
		double Compactness = 0;
		bool bIsConvex = true;
		bool bCompiledSuccessfully = false;
		bool bIsClosedLoop = false;

		FGeometryScriptSimplePolygon Polygon;

		explicit FCell(const TSharedRef<FCellConstraints>& InConstraints)
			: Constraints(InConstraints)
		{
		}

		~FCell() = default;

		ECellResult BuildFromCluster(
			const int32 SeedNodeIndex,
			const int32 SeedEdgeIndex,
			const FVector& Guide,
			TSharedRef<PCGExCluster::FCluster> InCluster,
			const TArray<FVector>& ProjectedPositions);

		ECellResult BuildFromPath(
			const TArray<FVector>& ProjectedPositions);

		void PostProcessPoints(TArray<FPCGPoint>& InMutablePoints);

	};

#pragma endregion
}

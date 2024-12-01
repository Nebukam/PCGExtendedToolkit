// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "GeomTools.h"
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
enum class EPCGExCellOutputOrientation : uint8
{
	CCW = 0 UMETA(DisplayName = "Clockwise", Tooltip="..."),
	CW  = 1 UMETA(DisplayName = "Counter Clockwise", Tooltip="..."),
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bUsedForPaths", EditConditionHides, HideEditConditionToggle))
	EPCGExWinding OutputWinding = EPCGExWinding::CounterClockwise;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExCellShapeTypeOutput AspectFilter = EPCGExCellShapeTypeOutput::Both;

	/** Whether to keep cells that include dead ends wrapping */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bKeepCellsWithLeaves = true;

	/** Whether to duplicate dead end points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Duplicate Leaf points", EditCondition="bKeepCellsWithLeaves && bUsedForPaths", EditConditionHides, HideEditConditionToggle))
	bool bDuplicateLeafPoints = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bOmitWrappingBounds = true;

	/** Omit cells with areas that closely match the computed wrapper. 0 to disable. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Classification Tolerance", EditCondition="bOmitWrappingBounds", ClampMin=0, HideInDetailPanel))
	double WrapperClassificationTolerance = 0;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Keep if Sole", EditCondition="bOmitWrappingBounds", EditConditionHides))
	bool bKeepWrapperIfSolePath = true;

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

	/** Combines all topologies generated into a single dynamic mesh component */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bCombinesAllTopologies = false;

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
	const FName SourceHolesLabel = FName("Holes");


	template <typename T>
	static bool IsPointInPolygon(const T& Point, const FGeometryScriptSimplePolygon& Polygon)
	{
		return FGeomTools2D::IsPointInPolygon(FVector2D(Point[0], Point[1]), *Polygon.Vertices);
	}

	static bool IsAnyPointInPolygon(const TArray<FVector2D>& Points, const FGeometryScriptSimplePolygon& Polygon)
	{
		if (Points.IsEmpty()) { return false; }
		const TArray<FVector2D>& Vertices = *Polygon.Vertices;
		for (const FVector2D& P : Points) { if (FGeomTools2D::IsPointInPolygon(P, Vertices)) { return true; } }
		return false;
	}

	static bool IsPolygonInPolygon(const FGeometryScriptSimplePolygon& ContainerPolygon, const FGeometryScriptSimplePolygon& Polygon)
	{
		const TArray<FVector2D>& ContainerPoints = *ContainerPolygon.Vertices;
		const TArray<FVector2D>& PolyPoints = *Polygon.Vertices;
		for (const FVector2D& Point : PolyPoints)
		{
			if (!FGeomTools2D::IsPointInPolygon(Point, ContainerPoints)) { return false; }
		}
		return true;
	}

	static FORCEINLINE void MarkTriangle(
		const TSharedPtr<PCGExCluster::FCluster>& InCluster,
		const PCGExGeo::FTriangle& InTriangle)
	{
		FPlatformAtomics::InterlockedExchange(&InCluster->GetNode(InTriangle.Vtx[0])->bValid, 1);
		FPlatformAtomics::InterlockedExchange(&InCluster->GetNode(InTriangle.Vtx[1])->bValid, 1);
		FPlatformAtomics::InterlockedExchange(&InCluster->GetNode(InTriangle.Vtx[2])->bValid, 1);
	}

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

	class FHoles : public TSharedFromThis<FHoles>
	{
	protected:
		mutable FRWLock ProjectionLock;

		TSharedRef<PCGExData::FFacade> PointDataFacade;
		FPCGExGeo2DProjectionDetails ProjectionDetails;
		TArray<FVector2D> ProjectedPoints;

	public:
		explicit FHoles(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InPointDataFacade, const FPCGExGeo2DProjectionDetails& InProjectionDetails)
			: PointDataFacade(InPointDataFacade), ProjectionDetails(InProjectionDetails)
		{
			ProjectionDetails.Init(InContext, PointDataFacade);
		}

		bool Overlaps(const FGeometryScriptSimplePolygon& Polygon);
	};

	class FCellConstraints : public TSharedFromThis<FCellConstraints>
	{
	protected:
		mutable FRWLock UniquePathsHashSetLock;
		TSet<uint32> UniquePathsHashSet;

		mutable FRWLock UniqueStartHalfEdgesHashLock;
		TSet<uint64> UniqueStartHalfEdgesHash;

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

		explicit FCellConstraints(const FPCGExCellConstraintsDetails& InDetails)
		{
			Winding = InDetails.OutputWinding;
			bConcaveOnly = InDetails.AspectFilter == EPCGExCellShapeTypeOutput::ConcaveOnly;
			bConvexOnly = InDetails.AspectFilter == EPCGExCellShapeTypeOutput::ConvexOnly;
			bKeepCellsWithLeaves = InDetails.bKeepCellsWithLeaves;
			bDuplicateLeafPoints = InDetails.bDuplicateLeafPoints;

			WrapperClassificationTolerance = InDetails.WrapperClassificationTolerance;
			bBuildWrapper = InDetails.bOmitWrappingBounds;

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
		}

		bool ContainsSignedEdgeHash(const uint64 Hash) const;
		bool IsUniqueStartHalfEdge(const uint64 Hash);
		bool IsUniqueCellHash(const TSharedPtr<FCell>& InCell);
		void BuildWrapperCell(TSharedRef<PCGExCluster::FCluster> InCluster, const TArray<FVector>& ProjectedPositions);

		void Cleanup();
	};

	USTRUCT(BlueprintType)
	struct /*PCGEXTENDEDTOOLKIT_API*/ FCellData
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

	class FCell : public TSharedFromThis<FCell>
	{
	protected:
		int32 Sign = 0;
		uint32 CellHash = 0;

	public:
		TArray<int32> Nodes;
		TSharedRef<FCellConstraints> Constraints;

		FCellData Data = FCellData();

		PCGExGraph::FLink Seed = PCGExGraph::FLink(-1, -1);

		bool bBuiltSuccessfully = false;

		FGeometryScriptSimplePolygon Polygon;

		explicit FCell(const TSharedRef<FCellConstraints>& InConstraints)
			: Constraints(InConstraints)
		{
			Data.bIsValid = true;
		}

		~FCell() = default;

		uint32 GetCellHash();

		ECellResult BuildFromCluster(
			const PCGExGraph::FLink InSeedLink,
			TSharedRef<PCGExCluster::FCluster> InCluster,
			const TArray<FVector>& ProjectedPositions);

		ECellResult BuildFromCluster(
			const FVector& SeedPosition,
			const TSharedRef<PCGExCluster::FCluster>& InCluster,
			const TArray<FVector>& ProjectedPositions,
			const FPCGExNodeSelectionDetails* Picking = nullptr);

		ECellResult BuildFromPath(
			const TArray<FVector>& ProjectedPositions);

		void PostProcessPoints(TArray<FPCGPoint>& InMutablePoints);
	};

#pragma endregion
}

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExCellArtifactsDetails
{
	GENERATED_BODY()

	FPCGExCellArtifactsDetails()
	{
	}

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bFlagTerminalPoint = false;

	/** Flag terminal points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName = "Flag Terminal", EditCondition="bFlagTerminalPoint"))
	FName TerminalFlagAttributeName = FName("IsTerminal");

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteNumRepeat = false;

	/** Number of time a point is repeated in the cell */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName = "Repeat", EditCondition="bWriteNumRepeat"))
	FName NumRepeatAttributeName = FName("Repeat");


	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagConcave = false;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagConcave"))
	FString ConcaveTag = TEXT("Concave");

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(InlineEditConditionToggle))
	bool bTagConvex = false;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(EditCondition="bTagConvex"))
	FString ConvexTag = TEXT("Convex");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bTagIfClosedLoop = true;

	/** ... */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable, EditCondition="bTagIfClosedLoop"))
	FString IsClosedLoopTag = TEXT("ClosedLoop");

	/** Tags to be forwarded from clusters */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Tagging", meta=(PCG_Overridable))
	FPCGExNameFiltersDetails TagForwarding;

	bool WriteAny() const;
	bool Init(FPCGExContext* InContext);

	void Process(
		const TSharedPtr<PCGExCluster::FCluster>& InCluster,
		const TSharedPtr<PCGExData::FFacade>& InDataFacade,
		const TSharedPtr<PCGExTopology::FCell>& InCell) const;
};

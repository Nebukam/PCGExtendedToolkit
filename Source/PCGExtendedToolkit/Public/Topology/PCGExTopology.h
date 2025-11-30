// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "GeomTools.h"
#include "PCGExHelpers.h"
#include "PCGExScopedContainers.h"
#include "Collections/PCGExComponentDescriptors.h"
#include "Data/PCGExDataFilter.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshRepairFunctions.h"
#include "Paths/PCGExPaths.h"

#include "PCGExTopology.generated.h"

namespace PCGExData
{
	struct FMutablePoint;
}

struct FPCGExNodeSelectionDetails;

namespace PCGExGeo
{
	struct FTriangle;
}

namespace PCGExCluster
{
	class FCluster;
}

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
struct PCGEXTENDEDTOOLKIT_API FPCGExCellConstraintsDetails
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" ├─ Classification Tolerance", EditCondition="bOmitWrappingBounds", ClampMin=0, HideInDetailPanel))
	double WrapperClassificationTolerance = 0.1;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Keep if Sole", EditCondition="bOmitWrappingBounds", EditConditionHides))
	bool bKeepWrapperIfSolePath = true;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitBelowBoundsSize = false;

	/** Omit cells whose bounds size.length is smaller than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" ┌─ Min Bounds Size", EditCondition="bOmitBelowBoundsSize", ClampMin=0))
	double MinBoundsSize = 3;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitAboveBoundsSize = false;

	/** Omit cells whose bounds size.length is larger than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Max Bounds Size", EditCondition="bOmitAboveBoundsSize", ClampMin=0))
	double MaxBoundsSize = 500;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitBelowPointCount = false;

	/** Omit cells whose point count is smaller than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" ┌─ Min Point Count", EditCondition="bOmitBelowPointCount", ClampMin=0))
	int32 MinPointCount = 3;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitAbovePointCount = false;

	/** Omit cells whose point count is larger than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Max Point Count", EditCondition="bOmitAbovePointCount", ClampMin=0))
	int32 MaxPointCount = 500;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitBelowArea = false;

	/** Omit cells whose area is smaller than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" ┌─ Min Area", EditCondition="bOmitBelowArea", ClampMin=0))
	double MinArea = 3;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitAboveArea = false;

	/** Omit cells whose area is larger than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Max Area", EditCondition="bOmitAboveArea", ClampMin=0))
	double MaxArea = 500;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitBelowPerimeter = false;

	/** Omit cells whose perimeter is smaller than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" ┌─ Min Perimeter", EditCondition="bOmitBelowPerimeter", ClampMin=0))
	double MinPerimeter = 3;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitAbovePerimeter = false;

	/** Omit cells whose perimeter is larger than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Max Perimeter", EditCondition="bOmitAbovePerimeter", ClampMin=0))
	double MaxPerimeter = 500;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitBelowSegmentLength = false;

	/** Omit cells that contains any segment which length is smaller than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" ┌─ Min Segment Length", EditCondition="bOmitBelowSegmentLength", ClampMin=0))
	double MinSegmentLength = 3;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitAboveSegmentLength = false;

	/** Omit cells that contains any segment which length is larger than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Max Segment Length", EditCondition="bOmitAboveSegmentLength", ClampMin=0))
	double MaxSegmentLength = 500;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitBelowCompactness = false;

	/** Omit cells that contains any segment which length is smaller than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" ┌─ Min Compactness", EditCondition="bOmitBelowCompactness", ClampMin=0, ClampMax=1))
	double MinCompactness = 0;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bOmitAboveCompactness = false;

	/** Omit cells that contains any segment which length is larger than the specified amount */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Max Compactness", EditCondition="bOmitAboveCompactness", ClampMin=0, ClampMax=1))
	double MaxCompactness = 1;
};

namespace PCGExTopology
{
	class FCell;
	const FName MeshOutputLabel = TEXT("Mesh");
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExCellSeedMutationDetails
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

	void ApplyToPoint(const PCGExTopology::FCell* InCell, PCGExData::FMutablePoint& OutSeedPoint, const UPCGBasePointData* CellPoints) const;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExUVInputDetails
{
	GENERATED_BODY()

	FPCGExUVInputDetails() = default;

	/** Whether this input is enabled or not */
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bEnabled = true;

	/** Name of the attribute containing the UVs (expects FVector2) */
	UPROPERTY(EditAnywhere, Category = Settings)
	FName AttributeName = NAME_None;

	/** Index of the UV channel on the final model */
	UPROPERTY(EditAnywhere, Category = Settings, meta=(ClampMin="0", ClampMax="7", UIMin="0", UIMax="7"))
	int32 Channel = 0;
};
	
USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExTopologyUVDetails
{
	GENERATED_BODY()

	FPCGExTopologyUVDetails() = default;

	/** List of UV channels */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TArray<FPCGExUVInputDetails> UVs;

	int32 NumChannels = 0;
	TArray<int32> ChannelIndices;
	TArray<TSharedPtr<PCGExData::TBuffer<FVector2D>>> UVBuffers;

	void Prepare(const TSharedPtr<PCGExData::FFacade>& InDataFacade);
	void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const;
	void Write(FDynamicMesh3& Mesh) const;
	void Write(const TMap<uint64, int32>& HashMapRef, const FVector2D CWTolerance, FDynamicMesh3& Mesh) const;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExTopologyDetails
{
	GENERATED_BODY()

	FPCGExTopologyDetails()
	{
	}

	/** Default material assigned to the mesh */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	TSoftObjectPtr<UMaterialInterface> Material;

	/** Default vertex color used for the points. Will use point color when available.*/
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FLinearColor DefaultVertexColor = FLinearColor::White;

	/** UV input settings */
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	FPCGExTopologyUVDetails TexCoordinates;

	/** Default primitive options
	 * Note that those are applied when triangulation is appended to the dynamic mesh. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	FGeometryScriptPrimitiveOptions PrimitiveOptions;

	/** Triangulation options
	 * Note that those are applied when triangulation is appended to the dynamic mesh. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	FGeometryScriptPolygonsTriangulationOptions TriangulationOptions;

	/** If enabled, will not throw an error in case Geometry Script complain about bad triangulation.
	 * If it shows, something went wrong but it's impossible to know exactly why. Look for structural anomalies, overlapping points, ...*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bQuietTriangulationError = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Geometry Script", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWeldEdges = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Geometry Script", meta = (PCG_Overridable, EditCondition="bWeldEdges"))
	FGeometryScriptWeldEdgesOptions WeldEdgesOptions;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Geometry Script", meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bComputeNormals = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Geometry Script", meta = (PCG_Overridable, EditCondition="bComputeNormals"))
	FGeometryScriptCalculateNormalsOptions NormalsOptions;

	/** Dynamic mesh component data. Only used by legacy output mode that spawned the component, prior to vanilla PCG interop.
	 * This will be completely ignored in most usecases. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable), AdvancedDisplay)
	FPCGExDynamicMeshDescriptor TemplateDescriptor;

	void PostProcessMesh(const TObjectPtr<UDynamicMesh>& InDynamicMesh) const;
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
	const FName SourceMeshLabel = FName("Mesh");
	const FName OutputMeshLabel = FName("Mesh");

	PCGEXTENDEDTOOLKIT_API
	void SetPointProperty(PCGExData::FMutablePoint& InPoint, const double InValue, const EPCGExPointPropertyOutput InProperty);

	template <typename T>
	static bool IsPointInPolygon(const T& Point, const FGeometryScriptSimplePolygon& Polygon)
	{
		return FGeomTools2D::IsPointInPolygon(FVector2D(Point[0], Point[1]), *Polygon.Vertices);
	}

	bool IsAnyPointInPolygon(const TArray<FVector2D>& Points, const FGeometryScriptSimplePolygon& Polygon);

	bool IsPolygonInPolygon(const FGeometryScriptSimplePolygon& ContainerPolygon, const FGeometryScriptSimplePolygon& Polygon);

	PCGEXTENDEDTOOLKIT_API
	void MarkTriangle(
		const TSharedPtr<PCGExCluster::FCluster>& InCluster,
		const PCGExGeo::FTriangle& InTriangle);


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

		bool Overlaps(const FGeometryScriptSimplePolygon& Polygon);
	};

	class FCellConstraints : public TSharedFromThis<FCellConstraints>
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

		void Reserve(const int32 InCellHashReserve);
		bool ContainsSignedEdgeHash(const uint64 Hash);
		bool IsUniqueStartHalfEdge(const uint64 Hash);
		bool IsUniqueCellHash(const TSharedPtr<FCell>& InCell);
		void BuildWrapperCell(const TSharedRef<PCGExCluster::FCluster>& InCluster, const TArray<FVector2D>& ProjectedPositions, const TSharedPtr<FCellConstraints>& InConstraints = nullptr);

		void Cleanup();
	};

	struct PCGEXTENDEDTOOLKIT_API FCellData
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

	class PCGEXTENDEDTOOLKIT_API FCell : public TSharedFromThis<FCell>
	{
	protected:
		int32 Sign = 0;
		uint64 CellHash = 0;

	public:
		TArray<int32> Nodes;
		TSharedRef<FCellConstraints> Constraints;

		FCellData Data = FCellData();

		PCGExGraph::FLink Seed = PCGExGraph::FLink(-1, -1);

		bool bBuiltSuccessfully = false;

		FGeometryScriptSimplePolygon Polygon;

		int32 CustomIndex = -1;

		explicit FCell(const TSharedRef<FCellConstraints>& InConstraints)
			: Constraints(InConstraints)
		{
			Data.bIsValid = true;
		}

		~FCell() = default;

		uint64 GetCellHash();

		ECellResult BuildFromCluster(
			const PCGExGraph::FLink InSeedLink,
			TSharedRef<PCGExCluster::FCluster> InCluster,
			const TArray<FVector2D>& ProjectedPositions);

		ECellResult BuildFromCluster(
			const FVector& SeedPosition,
			const TSharedRef<PCGExCluster::FCluster>& InCluster,
			const TArray<FVector2D>& ProjectedPositions,
			const FVector& UpVector = FVector::UpVector,
			const FPCGExNodeSelectionDetails* Picking = nullptr);

		ECellResult BuildFromPath(
			const TArray<FVector2D>& ProjectedPositions);

		void PostProcessPoints(UPCGBasePointData* InMutablePoints);
	};

#pragma endregion
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExCellArtifactsDetails
{
	GENERATED_BODY()

	FPCGExCellArtifactsDetails()
	{
	}

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteCellHash = false;

	/** Write cell unique hash as a @Data attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName = "Cell Hash", EditCondition="bWriteCellHash"))
	FName CellHashAttributeName = FName("@Data.CellHash");

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteArea = false;

	/** Write cell area as a @Data attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName = "Cell Area", EditCondition="bWriteArea"))
	FName AreaAttributeName = FName("@Data.Area");

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteCompactness = false;

	/** Write cell compactness as a @Data attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName = "Cell Compactness", EditCondition="bWriteCompactness"))
	FName CompactnessAttributeName = FName("@Data.Compactness");

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bWriteVtxId = false;

	/** Name of the attribute to write the vtx index of its point to. This is useful if you want to find contours, mutate the cluster it comes from and remap the updated cluster positions onto the original cell. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName = "Vtx ID", EditCondition="bWriteVtxId"))
	FName VtxIdAttributeName = FName("VtxId");

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

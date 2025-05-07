// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Collections/PCGExMeshCollection.h"
#include "Components/SplineMeshComponent.h"
#include "Data/PCGSplineStruct.h"
#include "Graph/PCGExEdge.h"

#include "PCGExPaths.generated.h"

UENUM()
enum class EPCGExSplinePointTypeRedux : uint8
{
	Linear       = 0 UMETA(DisplayName = "Linear (0)", Tooltip="Linear (0)."),
	Curve        = 1 UMETA(DisplayName = "Curve (1)", Tooltip="Curve (1)."),
	Constant     = 2 UMETA(DisplayName = "Constant (2)", Tooltip="Constant (2)."),
	CurveClamped = 3 UMETA(DisplayName = "CurveClamped (3)", Tooltip="CurveClamped (3).")
};

UENUM()
enum class EPCGExInlinePathProcessingOrder : uint8
{
	FromStart       = 0 UMETA(DisplayName = "From Start", ToolTip="Start at the index 0 of the path. If inverted, start at the last index."),
	EndpointCompare = 2 UMETA(DisplayName = "Endpoint Comparison", ToolTip="Compare an attribute on start and end point to determine which endpoint to start with. If the comparison returns true, start with first point."),
	TaggedAny       = 3 UMETA(DisplayName = "Tagged (Any)", ToolTip="Check for a tag match on the input data. If the tag is found, start with first point."),
	TaggedAll       = 4 UMETA(DisplayName = "Tagged (All)", ToolTip="Check for all tag matches on the input data. If all tags are found, start with first point."),
};

UENUM()
enum class EPCGExInputScope : uint8
{
	All          = 0 UMETA(DisplayName = "All", Tooltip="All paths are considered to have the same open or closed status."),
	AllButTagged = 2 UMETA(DisplayName = "All but tagged", Tooltip="All paths are considered open or closed by default, except the ones with the specified tags which will use the opposite value."),
};

UENUM()
enum class EPCGExPathNormalDirection : uint8
{
	Normal        = 0 UMETA(DisplayName = "Normal", ToolTip="..."),
	Binormal      = 1 UMETA(DisplayName = "Binormal", ToolTip="..."),
	AverageNormal = 2 UMETA(DisplayName = "Average Normal", ToolTip="..."),
};

UENUM()
enum class EPCGExSplineMeshUpMode : uint8
{
	Constant  = 0 UMETA(DisplayName = "Constant", Tooltip="Constant up vector"),
	Attribute = 1 UMETA(DisplayName = "Attribute", Tooltip="Per-point attribute value"),
	Tangents  = 2 UMETA(DisplayName = "From Tangents (Gimbal fix)", Tooltip="Automatically computed up vector from tangents to enforce gimbal fix")
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPathOutputDetails
{
	GENERATED_BODY()

	FPCGExPathOutputDetails() = default;

	/** Don't output paths if they have less points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveSmallPaths = false;

	/** Minimum points threshold */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable, EditCondition="bRemoveSmallPaths", ClampMin=2))
	int32 MinPointCount = 3;

	/** Don't output paths if they have more points than a specified amount. */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable, InlineEditConditionToggle))
	bool bRemoveLargePaths = false;

	/** Maximum points threshold */
	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta = (PCG_Overridable, EditCondition="bRemoveLargePaths", ClampMin=2))
	int32 MaxPointCount = 500;

	bool Validate(TArray<FPCGPoint>& InPathPoints) const;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPathClosedLoopDetails
{
	GENERATED_BODY()

	/** Define whether the input dataset should be considered open or closed. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputScope Scope = EPCGExInputScope::AllButTagged;

	/** Whether the paths are closed loops. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bClosedLoop = false;

	/** Comma separated tags */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Scope!=EPCGExInputScope::All", EditConditionHides))
	FString CommaSeparatedTags = TEXT("ClosedLoop");

	TArray<FString> Tags;

	void Init();

	bool IsClosedLoop(const TSharedPtr<PCGExData::FPointIO>& InPointIO) const;
	bool IsClosedLoop(const FPCGTaggedData& InTaggedData) const;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPathClosedLoopUpdateDetails
{
	GENERATED_BODY()

	/** Tags to be added to closed paths that are broken into open paths */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Add Open Tags"))
	FString CommaSeparatedAddTags = TEXT("OpenPath");

	/** Tags to be removed from closed paths that are broken into open paths */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Remove Open Tags"))
	FString CommaSeparatedRemoveTags = TEXT("ClosedLoop");

	TArray<FString> AddTags;
	TArray<FString> RemoveTags;
	TArray<FString> Tags;

	void Init();

	void Update(const TSharedPtr<PCGExData::FPointIO>& InPointIO);
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPathEdgeIntersectionDetails
{
	GENERATED_BODY()

	explicit FPCGExPathEdgeIntersectionDetails(bool bInSupportSelfIntersection = true)
		: bSupportSelfIntersection(bInSupportSelfIntersection)
	{
	}

	UPROPERTY()
	bool bSupportSelfIntersection = true;

	/** If disabled, edges will only be checked against other datasets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0, EditCondition="bSupportSelfIntersection", EditConditionHides, HideEditConditionToggle))
	bool bEnableSelfIntersection = true;

	/** Distance at which two edges are considered intersecting. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	double Tolerance = DBL_INTERSECTION_TOLERANCE;
	double ToleranceSquared = DBL_INTERSECTION_TOLERANCE;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseMinAngle = true;

	/** Min angle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseMinAngle", Units="Degrees", ClampMin=0, ClampMax=180))
	double MinAngle = 0;
	double MaxDot = -1;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseMaxAngle = true;

	/** Maximum angle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseMaxAngle", Units="Degrees", ClampMin=0, ClampMax=180))
	double MaxAngle = 90;
	double MinDot = 1;

	//

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteCrossing = false;

	/** Name of the attribute to flag point as crossing (result of an Edge/Edge intersection) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, EditCondition="bWriteCrossing"))
	FName CrossingAttributeName = "bIsCrossing";

	void Init();

	FORCEINLINE bool CheckDot(const double InDot) const { return InDot <= MaxDot && InDot >= MinDot; }
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPathFilterSettings
{
	GENERATED_BODY()

	/** Method to pick the edge direction amongst various possibilities.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExEdgeDirectionMethod DirectionMethod = EPCGExEdgeDirectionMethod::EndpointsOrder;

	/** Further refine the direction method. Not all methods make use of this property.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExEdgeDirectionChoice DirectionChoice = EPCGExEdgeDirectionChoice::SmallestToGreatest;

	/** Attribute picker for the selected Direction Method.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="DirectionMethod==EPCGExEdgeDirectionMethod::EndpointsAttribute || DirectionMethod==EPCGExEdgeDirectionMethod::EdgeDotAttribute", EditConditionHides))
	FPCGAttributePropertyInputSelector DirSourceAttribute;

	bool bAscendingDesired = false;
	TSharedPtr<PCGExData::TBuffer<double>> EndpointsReader;
	TSharedPtr<PCGExData::TBuffer<FVector>> EdgeDirReader;

	void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const;

	bool Init(FPCGExContext* InContext);
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPathIntersectionDetails
{
	GENERATED_BODY()

	FPCGExPathIntersectionDetails() = default;
	explicit FPCGExPathIntersectionDetails(const double InTolerance, const double InMinAngle, const double InMaxAngle = 90);

	/** Distance at which two edges are considered intersecting. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	double Tolerance = DBL_INTERSECTION_TOLERANCE;
	double ToleranceSquared = DBL_INTERSECTION_TOLERANCE * DBL_INTERSECTION_TOLERANCE;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseMinAngle = false;

	/** Min angle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseMinAngle", Units="Degrees", ClampMin=0, ClampMax=90))
	double MinAngle = 0;
	double MinDot = -1;

	/** . */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bUseMaxAngle = false;

	/** Maximum angle. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseMaxAngle", Units="Degrees", ClampMin=0, ClampMax=90))
	double MaxAngle = 90;
	double MaxDot = 1;

	bool bWantsDotCheck = false;

	void Init()
	{
		MaxDot = bUseMinAngle ? PCGExMath::DegreesToDot(MinAngle) : 1;
		MinDot = bUseMaxAngle ? PCGExMath::DegreesToDot(MaxAngle) : -1;
		ToleranceSquared = Tolerance * Tolerance;
		bWantsDotCheck = bUseMinAngle || bUseMaxAngle;
	}

	FORCEINLINE bool CheckDot(const double InDot) const { return InDot <= MaxDot && InDot >= MinDot; }
};

namespace PCGExPaths
{
	PCGEX_CTX_STATE(State_BuildingPaths)

	const FName SourcePathsLabel = TEXT("Paths");
	const FName OutputPathsLabel = TEXT("Paths");

	const FName SourceCanCutFilters = TEXT("Can Cut Conditions");
	const FName SourceCanBeCutFilters = TEXT("Can Be Cut Conditions");
	const FName SourceTriggerFilters = TEXT("Trigger Conditions");
	const FName SourceShiftFilters = TEXT("Shift Conditions");

	struct PCGEXTENDEDTOOLKIT_API FPathMetrics
	{
		FPathMetrics() = default;
		explicit FPathMetrics(const FVector& InStart);
		explicit FPathMetrics(const TArrayView<FPCGPoint>& Points);

		FVector Start = FVector::ZeroVector;
		FVector Last = FVector::ZeroVector;
		double Length = -1;
		int32 Count = 0;

		void Reset(const FVector& InStart);

		double Add(const FVector& Location);
		double Add(const FVector& Location, double& OutDistToLast);

		bool IsValid() const { return Length > 0; }
		double GetTime(const double Distance) const { return (!Distance || !Length) ? 0 : Distance / Length; }
		double DistToLast(const FVector& Location) const { return FVector::Dist(Last, Location); }
		bool IsLastWithinRange(const FVector& Location, const double Range) const { return DistToLast(Location) < Range; }
	};

	struct PCGEXTENDEDTOOLKIT_API FMetadata
	{
		double Position = 0;
		double TotalLength = 0;

		double GetAlpha() const { return Position / TotalLength; }
		double GetInvertedAlpha() const { return 1 - (Position / TotalLength); }
	};

	constexpr FMetadata InvalidMetadata = FMetadata();

	struct PCGEXTENDEDTOOLKIT_API FSplineMeshSegment
	{
		FSplineMeshSegment()
		{
		}

		bool bSetMeshWithSettings = false;
		bool bSmoothInterpRollScale = true;
		bool bUseDegrees = true;
		FVector UpVector = FVector::UpVector;
		TSet<FName> Tags;

		ESplineMeshAxis::Type SplineMeshAxis = ESplineMeshAxis::Type::X;

		const FPCGExMeshCollectionEntry* MeshEntry = nullptr;
		int32 MaterialPick = -1;
		FSplineMeshParams Params;

		void ComputeUpVectorFromTangents();

		void ApplySettings(USplineMeshComponent* Component) const;

		bool ApplyMesh(USplineMeshComponent* Component) const;
	};

	struct FPathEdge
	{
		int32 Start = -1;
		int32 End = -1;
		FVector Dir = FVector::ZeroVector;
		FBoxSphereBounds Bounds = FBoxSphereBounds{};

		int32 AltStart = -1;

		FPathEdge(const int32 InStart, const int32 InEnd, const TArrayView<const FVector>& Positions, const double Expansion = 0);

		void Update(const TArrayView<const FVector>& Positions, const double Expansion = 0);

		bool ShareIndices(const FPathEdge& Other) const;
		bool Connects(const FPathEdge& Other) const;
		bool ShareIndices(const FPathEdge* Other) const;
	};

	class FPath;

	class FPathEdgeExtraBase : public TSharedFromThis<FPathEdgeExtraBase>
	{
	protected:
		bool bClosedLoop = false;

	public:
		explicit FPathEdgeExtraBase(const int32 InNumSegments, bool InClosedLoop)
			: bClosedLoop(InClosedLoop)
		{
		}

		virtual ~FPathEdgeExtraBase() = default;

		virtual void ProcessSingleEdge(const FPath* Path, const FPathEdge& Edge) { ProcessFirstEdge(Path, Edge); }
		virtual void ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge) { ProcessEdge(Path, Edge); };
		virtual void ProcessEdge(const FPath* Path, const FPathEdge& Edge) = 0;
		virtual void ProcessLastEdge(const FPath* Path, const FPathEdge& Edge) { ProcessEdge(Path, Edge); }

		virtual void ProcessingDone(const FPath* Path);
	};

	template <typename T>
	class TPathEdgeExtra : public FPathEdgeExtraBase
	{
	protected:
		TArray<T> Data;

	public:
		TArray<T> Values;

		explicit TPathEdgeExtra(const int32 InNumSegments, bool InClosedLoop)
			: FPathEdgeExtraBase(InNumSegments, InClosedLoop)
		{
			PCGEx::InitArray(Data, InNumSegments);
		}

		FORCEINLINE T& operator[](const int32 At) { return Data[At]; }
		FORCEINLINE T operator[](const int32 At) const { return Data[At]; }
		FORCEINLINE void Set(const int32 At, const T Value) { Data[At] = Value; }
		FORCEINLINE T Get(const int32 At) { return Data[At]; }
		FORCEINLINE T& GetMutable(const int32 At) { return Data[At]; }
		FORCEINLINE T Get(const FPathEdge& At) { return Data[At.Start]; }
	};

	PCGEX_OCTREE_SEMANTICS(FPathEdge, { return Element->Bounds;}, { return A == B; })

	class FPath : public TSharedFromThis<FPath>
	{
	protected:
		bool bClosedLoop = false;
		TArray<FVector> Positions;
		TUniquePtr<FPathEdgeOctree> EdgeOctree;
		TArray<TSharedPtr<FPathEdgeExtraBase>> Extras;

	public:
		virtual ~FPath() = default;
		FBox Bounds = FBox(ForceInit);
		TArray<FPathEdge> Edges;
		int32 NumPoints = 0;
		int32 NumEdges = 0;
		int32 LastIndex = 0;
		int32 LastEdge = 0;
		int32 Id = -1;

		int32 ConvexitySign = 0;
		bool bIsConvex = true;

		int32 IOIndex = -1;

		FORCEINLINE const TArray<FVector>& GetPositions() const { return Positions; }

		FORCEINLINE int32 LoopPointIndex(const int32 Index) const { return PCGExMath::Tile(Index, 0, LastIndex); };
		virtual int32 SafePointIndex(const int32 Index) const = 0;

		FORCEINLINE virtual const FVector& GetPos(const int32 Index) const { return Positions[SafePointIndex(Index)]; }
		FORCEINLINE virtual const FVector& GetPos_Unsafe(const int32 Index) const { return Positions[Index]; }
		FORCEINLINE bool IsValidEdgeIndex(const int32 Index) const { return Index >= 0 && Index < NumEdges; }

		virtual FVector DirToNextPoint(const int32 Index) const = 0;
		FVector DirToPrevPoint(const int32 Index) const { return DirToNextPoint(SafePointIndex(Index - 1)) * -1; }

		virtual int32 NextPointIndex(const int32 Index) const { return SafePointIndex(Index + 1); }
		virtual int32 PrevPointIndex(const int32 Index) const { return SafePointIndex(Index - 1); }

		FVector GetEdgePositionAtAlpha(const FPathEdge& Edge, const double Alpha) const { return FMath::Lerp(Positions[Edge.End], Positions[Edge.Start], Alpha); }

		FVector GetEdgePositionAtAlpha(const int32 Index, const double Alpha) const
		{
			const FPathEdge& Edge = Edges[Index];
			return FMath::Lerp(Positions[Edge.Start], Positions[Edge.End], Alpha);
		}

		virtual bool IsEdgeValid(const FPathEdge& Edge) const { return FVector::DistSquared(GetPos_Unsafe(Edge.Start), GetPos_Unsafe(Edge.End)) > 0; }
		virtual bool IsEdgeValid(const int32 Index) const { return IsEdgeValid(Edges[Index]); }

		template <PCGExMath::EIntersectionTestMode Mode = PCGExMath::EIntersectionTestMode::Strict>
		PCGExMath::FClosestPosition FindClosestIntersection(
			const FPCGExPathIntersectionDetails& InDetails, const PCGExMath::FSegment& Segment) const
		{
			PCGExMath::FClosestPosition Closest(Segment.A);

			if (!Bounds.Intersect(Segment.Bounds)) { return Closest; }

			GetEdgeOctree()->FindElementsWithBoundsTest(
				Segment.Bounds, [&](const FPathEdge* PathEdge)
				{
					if (InDetails.bWantsDotCheck)
					{
						if (!InDetails.CheckDot(FMath::Abs(Segment.Dot(PathEdge->Dir)))) { return; }
					}

					FVector OnSegment = FVector::ZeroVector;
					FVector OnPath = FVector::ZeroVector;

					if (!Segment.FindIntersection<Mode>(
						GetPos_Unsafe(PathEdge->Start),
						GetPos_Unsafe(PathEdge->End),
						InDetails.ToleranceSquared,
						OnSegment,
						OnPath))
					{
						return;
					}

					Closest.Update(OnPath, PathEdge->Start);
				});

			return Closest;
		}

		template <PCGExMath::EIntersectionTestMode Mode = PCGExMath::EIntersectionTestMode::Strict>
		PCGExMath::FClosestPosition FindClosestIntersection(
			const FPCGExPathIntersectionDetails& InDetails, const FVector& A1, const FVector& B1) const
		{
			return FindClosestIntersection<Mode>(InDetails, A1, B1);
		}

		template <PCGExMath::EIntersectionTestMode Mode = PCGExMath::EIntersectionTestMode::Strict>
		PCGExMath::FClosestPosition FindClosestIntersection(
			const FPCGExPathIntersectionDetails& InDetails, const PCGExMath::FSegment& Segment,
			PCGExMath::FClosestPosition& OutClosestPosition) const
		{
			PCGExMath::FClosestPosition Closest(Segment.A);

			if (!Bounds.Intersect(Segment.Bounds)) { return Closest; }

			GetEdgeOctree()->FindElementsWithBoundsTest(
				Segment.Bounds, [&](const FPathEdge* PathEdge)
				{
					if (InDetails.bWantsDotCheck)
					{
						if (!InDetails.CheckDot(FMath::Abs(Segment.Dot(PathEdge->Dir)))) { return; }
					}

					FVector OnSegment = FVector::ZeroVector;
					FVector OnPath = FVector::ZeroVector;

					if (!Segment.FindIntersection<Mode>(
						GetPos_Unsafe(PathEdge->Start),
						GetPos_Unsafe(PathEdge->End),
						InDetails.ToleranceSquared,
						OnSegment,
						OnPath))
					{
						OutClosestPosition.Update(OnPath, -2);
						return;
					}

					OutClosestPosition.Update(OnPath, -2);
					Closest.Update(OnPath, PathEdge->Start);
				});

			return Closest;
		}

		void BuildEdgeOctree();
		void BuildPartialEdgeOctree(const TArray<int8>& Filter);

		const FPathEdgeOctree* GetEdgeOctree() const { return EdgeOctree.Get(); }
		FORCEINLINE bool IsClosedLoop() const { return bClosedLoop; }

		void UpdateConvexity(const int32 Index);

		template <typename T, typename... Args>
		TSharedPtr<T> AddExtra(const bool bImmediateCompute = false, Args&&... InArgs)
		{
			PCGEX_MAKE_SHARED(Extra, T, NumEdges, bClosedLoop, std::forward<Args>(InArgs)...)

			if (bImmediateCompute)
			{
				if (NumEdges == 1)
				{
					Extra->ProcessSingleEdge(this, Edges[0]);
				}
				else
				{
					if (bClosedLoop)
					{
						for (int i = 0; i < NumEdges; ++i) { Extra->ProcessEdge(this, Edges[i]); }
					}
					else
					{
						Extra->ProcessFirstEdge(this, Edges[0]);
						for (int i = 1; i < LastEdge; ++i) { Extra->ProcessEdge(this, Edges[i]); }
						Extra->ProcessLastEdge(this, Edges[LastEdge]);
					}
				}

				Extra->ProcessingDone(this);
			}
			else
			{
				Extras.Add(Extra);
			}

			return Extra;
		}

		virtual void ComputeEdgeExtra(const int32 Index);
		virtual void ExtraComputingDone();
		virtual void ComputeAllEdgeExtra();

		virtual void UpdateEdges(const TArray<FPCGPoint>& InPoints, const double Expansion);
		virtual void UpdateEdges(const TArrayView<const FVector> InPositions, const double Expansion);

	protected:
		void BuildPath(const double Expansion);
	};

	template <bool ClosedLoop = false>
	class TPath final : public FPath
	{
	public:
		explicit TPath(const TArrayView<const FPCGPoint>& InPoints, const double Expansion = 0)
		{
			bClosedLoop = ClosedLoop;

			NumPoints = InPoints.Num();
			LastIndex = NumPoints - 1;

			Positions.SetNumUninitialized(NumPoints);
			for (int i = 0; i < NumPoints; i++) { *(Positions.GetData() + i) = InPoints[i].Transform.GetLocation(); }

			BuildPath(Expansion);
		}

		explicit TPath(const TArrayView<const FVector> InPositions, const double Expansion = 0)
		{
			bClosedLoop = ClosedLoop;

			NumPoints = InPositions.Num();
			LastIndex = NumPoints - 1;

			Positions.Reset(NumPoints);
			Positions.Append(InPositions);

			BuildPath(Expansion);
		}

		virtual int32 SafePointIndex(const int32 Index) const override
		{
			if constexpr (ClosedLoop) { return PCGExMath::Tile(Index, 0, LastIndex); }
			else { return Index < 0 ? 0 : Index > LastIndex ? LastIndex : Index; }
		}

		virtual FVector DirToNextPoint(const int32 Index) const override
		{
			if constexpr (ClosedLoop) { return Edges[Index].Dir; }
			else { return Index == LastIndex ? Edges[Index - 1].Dir : Edges[Index].Dir; }
		}
	};

	template <typename T>
	class FPathEdgeCustomData : public TPathEdgeExtra<T>
	{
	public:
		using ProcessEdgeFunc = std::function<T(const FPath*, const FPathEdge&)>;
		ProcessEdgeFunc ProcessEdgeCallback;

		explicit FPathEdgeCustomData(const int32 InNumSegments, const bool InClosedLoop, ProcessEdgeFunc&& Func)
			: TPathEdgeExtra<T>(InNumSegments, InClosedLoop), ProcessEdgeCallback(Func)
		{
		}

		virtual void ProcessEdge(const FPath* Path, const FPathEdge& Edge) override
		{
			this->GetMutable(Edge.Start) = ProcessEdgeCallback(Path, Edge);
		}
	};

	class FPathEdgeLength : public TPathEdgeExtra<double>
	{
	public:
		double TotalLength = 0;
		TArray<double> CumulativeLength;

		explicit FPathEdgeLength(const int32 InNumSegments, const bool InClosedLoop)
			: TPathEdgeExtra(InNumSegments, InClosedLoop)
		{
		}

		virtual void ProcessEdge(const FPath* Path, const FPathEdge& Edge) override;
		virtual void ProcessingDone(const FPath* Path) override;
	};

	class FPathEdgeLengthSquared : public TPathEdgeExtra<double>
	{
	public:
		explicit FPathEdgeLengthSquared(const int32 InNumSegments, const bool InClosedLoop)
			: TPathEdgeExtra(InNumSegments, InClosedLoop)
		{
		}

		virtual void ProcessEdge(const FPath* Path, const FPathEdge& Edge) override;
	};

	class FPathEdgeNormal : public TPathEdgeExtra<FVector>
	{
		FVector Up = FVector::UpVector;

	public:
		explicit FPathEdgeNormal(const int32 InNumSegments, const bool InClosedLoop, const FVector& InUp)
			: TPathEdgeExtra(InNumSegments, InClosedLoop), Up(InUp)
		{
		}

		virtual void ProcessEdge(const FPath* Path, const FPathEdge& Edge) override;
	};

	class FPathEdgeBinormal : public TPathEdgeExtra<FVector>
	{
		FVector Up = FVector::UpVector;

	public:
		TArray<FVector> Normals;

		explicit FPathEdgeBinormal(const int32 InNumSegments, const bool InClosedLoop, const FVector& InUp = FVector::UpVector)
			: TPathEdgeExtra(InNumSegments, InClosedLoop), Up(InUp)
		{
			Normals.SetNumUninitialized(InNumSegments);
		}

		virtual void ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge) override;
		virtual void ProcessEdge(const FPath* Path, const FPathEdge& Edge) override;
		virtual void ProcessLastEdge(const FPath* Path, const FPathEdge& Edge) override;
	};

	class FPathEdgeAvgNormal : public TPathEdgeExtra<FVector>
	{
		FVector Up = FVector::UpVector;

	public:
		explicit FPathEdgeAvgNormal(const int32 InNumSegments, const bool InClosedLoop, const FVector& InUp = FVector::UpVector)
			: TPathEdgeExtra(InNumSegments, InClosedLoop), Up(InUp)
		{
		}

		virtual void ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge) override;
		virtual void ProcessEdge(const FPath* Path, const FPathEdge& Edge) override;
		virtual void ProcessLastEdge(const FPath* Path, const FPathEdge& Edge) override;
	};

	class FPathEdgeHalfAngle : public TPathEdgeExtra<double>
	{
		FVector Up = FVector::UpVector;

	public:
		explicit FPathEdgeHalfAngle(const int32 InNumSegments, const bool InClosedLoop, const FVector& InUp = FVector::UpVector)
			: TPathEdgeExtra(InNumSegments, InClosedLoop), Up(InUp)
		{
		}

		virtual void ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge) override;
		virtual void ProcessEdge(const FPath* Path, const FPathEdge& Edge) override;
		virtual void ProcessLastEdge(const FPath* Path, const FPathEdge& Edge) override;
	};

	class FPathEdgeFullAngle : public TPathEdgeExtra<double>
	{
		FVector Up = FVector::UpVector;

	public:
		explicit FPathEdgeFullAngle(const int32 InNumSegments, const bool InClosedLoop, const FVector& InUp = FVector::UpVector)
			: TPathEdgeExtra(InNumSegments, InClosedLoop), Up(InUp)
		{
		}

		virtual void ProcessFirstEdge(const FPath* Path, const FPathEdge& Edge) override;
		virtual void ProcessEdge(const FPath* Path, const FPathEdge& Edge) override;
		virtual void ProcessLastEdge(const FPath* Path, const FPathEdge& Edge) override;
	};

	TSharedPtr<FPath> MakePath(const TArrayView<const FPCGPoint> InPoints, const double Expansion, const bool bClosedLoop);
	TSharedPtr<FPath> MakePath(const TArrayView<const FVector> InPositions, const double Expansion, const bool bClosedLoop);

	FTransform GetClosestTransform(const FPCGSplineStruct& InSpline, const FVector& InLocation, const bool bUseScale = true);
	FTransform GetClosestTransform(const TSharedPtr<const FPCGSplineStruct>& InSpline, const FVector& InLocation, const bool bUseScale = true);

	TSharedPtr<FPCGSplineStruct> MakeSplineFromPoints(const UPCGPointData* InData, const EPCGExSplinePointTypeRedux InPointType, const bool bClosedLoop);

	template <PCGExMath::EIntersectionTestMode Mode = PCGExMath::EIntersectionTestMode::Strict>
	PCGExMath::FClosestPosition FindClosestIntersection(
		const TArray<TSharedPtr<FPath>>& Paths,
		const FPCGExPathIntersectionDetails& InDetails,
		const PCGExMath::FSegment& InSegment, int32& OutPathIndex)
	{
		OutPathIndex = -1;

		PCGExMath::FClosestPosition Intersection(InSegment.A);

		for (int i = 0; i < Paths.Num(); i++)
		{
			PCGExMath::FClosestPosition LocalIntersection = Paths[i]->FindClosestIntersection<Mode>(InDetails, InSegment);
			if (!LocalIntersection) { continue; }
			if (Intersection.Update(LocalIntersection, LocalIntersection.Index)) { OutPathIndex = i; }
		}

		return Intersection;
	}

	template <PCGExMath::EIntersectionTestMode Mode = PCGExMath::EIntersectionTestMode::Strict>
	PCGExMath::FClosestPosition FindClosestIntersection(
		const TArray<TSharedPtr<FPath>>& Paths,
		const FPCGExPathIntersectionDetails& InDetails,
		const PCGExMath::FSegment& InSegment, int32& OutPathIndex,
		PCGExMath::FClosestPosition& OutClosestPosition)
	{
		OutPathIndex = -1;

		PCGExMath::FClosestPosition Intersection(InSegment.A);

		for (int i = 0; i < Paths.Num(); i++)
		{
			PCGExMath::FClosestPosition LocalIntersection = Paths[i]->FindClosestIntersection<Mode>(InDetails, InSegment, OutClosestPosition);

			if (OutClosestPosition.Index == -2) { OutClosestPosition.Index = i; }

			if (!LocalIntersection) { continue; }
			if (Intersection.Update(LocalIntersection, LocalIntersection.Index)) { OutPathIndex = i; }
		}

		return Intersection;
	}
}


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExSplineMeshMutationDetails
{
	GENERATED_BODY()

	FPCGExSplineMeshMutationDetails() = default;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bPushStart = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bPushStart", EditConditionHides))
	EPCGExInputValueType StartPushInput = EPCGExInputValueType::Constant;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" ├─ Amount (Attr)", EditCondition="bPushStart && StartPushInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector StartPushInputAttribute;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" ├─ Amount", EditCondition="bPushStart && StartPushInput==EPCGExInputValueType::Constant", EditConditionHides))
	double StartPushConstant = 0.1;

	PCGEX_SETTING_VALUE_GET(StartPush, double, StartPushInput, StartPushInputAttribute, StartPushConstant)

	/** If enabled, value will relative to the size of the segment */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Relative", EditCondition="bPushStart", EditConditionHides))
	bool bRelativeStart = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bPushEnd = false;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bPushEnd", EditConditionHides))
	EPCGExInputValueType EndPushInput = EPCGExInputValueType::Constant;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" ├─ Amount (Attr)", EditCondition="bPushEnd && EndPushInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector EndPushInputAttribute;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" ├─ Amount", EditCondition="bPushEnd && EndPushInput==EPCGExInputValueType::Constant", EditConditionHides))
	double EndPushConstant = 0.1;

	PCGEX_SETTING_VALUE_GET(EndPush, double, EndPushInput, EndPushInputAttribute, EndPushConstant)

	/** If enabled, value will relative to the size of the segment */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName=" └─ Relative", EditCondition="bPushEnd", EditConditionHides))
	bool bRelativeEnd = true;

	bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade);
	void Mutate(const int32 PointIndex, PCGExPaths::FSplineMeshSegment& InSegment);

protected:
	TSharedPtr<PCGExDetails::TSettingValue<double>> StartAmount;
	TSharedPtr<PCGExDetails::TSettingValue<double>> EndAmount;
};

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "Collections/PCGExMeshCollection.h"
#include "Components/SplineMeshComponent.h"
#include "Graph/PCGExEdge.h"

#include "PCGExPaths.generated.h"

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Path Shrink Distance Cut Type"))
enum class EPCGExInlinePathProcessingOrder : uint8
{
	FromStart       = 0 UMETA(DisplayName = "From Start", ToolTip="Start at the index 0 of the path. If inverted, start at the last index."),
	EndpointCompare = 2 UMETA(DisplayName = "Endpoint Comparison", ToolTip="Compare an attribute on start and end point to determine which endpoint to start with. If the comparison returns true, start with first point."),
	TaggedAny       = 3 UMETA(DisplayName = "Tagged (Any)", ToolTip="Check for a tag match on the input data. If the tag is found, start with first point."),
	TaggedAll       = 4 UMETA(DisplayName = "Tagged (All)", ToolTip="Check for all tag matches on the input data. If all tags are found, start with first point."),
};

UENUM(BlueprintType, meta=(DisplayName="[PCGEx] Input Scope"))
enum class EPCGExInputScope : uint8
{
	All          = 0 UMETA(DisplayName = "All", Tooltip="All paths are considered to have the same open or closed status."),
	AllButTagged = 2 UMETA(DisplayName = "All but tagged", Tooltip="All paths are considered open or closed by default, except the ones with the specified tags which will use the opposite value."),
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathClosedLoopDetails
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

	void Init()
	{
		Tags = PCGExHelpers::GetStringArrayFromCommaSeparatedList(CommaSeparatedTags);
	}

	bool IsClosedLoop(const TSharedPtr<PCGExData::FPointIO>& InPointIO) const
	{
		if (Scope == EPCGExInputScope::All) { return bClosedLoop; }
		if (Tags.IsEmpty()) { return !bClosedLoop; }
		for (const FString& Tag : Tags) { if (InPointIO->Tags->IsTagged(Tag)) { return !bClosedLoop; } }
		return bClosedLoop;
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathClosedLoopUpdateDetails
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

	void Init()
	{
		AddTags = PCGExHelpers::GetStringArrayFromCommaSeparatedList(CommaSeparatedAddTags);
		RemoveTags = PCGExHelpers::GetStringArrayFromCommaSeparatedList(CommaSeparatedRemoveTags);
	}

	void Update(const TSharedPtr<PCGExData::FPointIO>& InPointIO)
	{
		for (const FString& Add : AddTags) { InPointIO->Tags->Add(Add); }
		for (const FString& Rem : RemoveTags) { InPointIO->Tags->Remove(Rem); }
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathProcessingDirectionDetails
{
	GENERATED_BODY()

	/** How to pick which path endpoint to start with */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInlinePathProcessingOrder Mode = EPCGExInlinePathProcessingOrder::FromStart;

	/** Invert the select mode to go with the opposite endpoint. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bInvert = false;

	/** Comma separated tags */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode==EPCGExInlinePathProcessingOrder::TaggedAny || Mode==EPCGExInlinePathProcessingOrder::TaggedAll", EditConditionHides))
	FString CommaSeparatedTags = TEXT("First");

	/** Comma separated tags */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode==EPCGExInlinePathProcessingOrder::EndpointCompare", EditConditionHides))
	FPCGAttributePropertyInputSelector ComparisonAttribute;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode==EPCGExInlinePathProcessingOrder::EndpointCompare", EditConditionHides))
	EPCGExComparison Comparison = EPCGExComparison::NearlyEqual;

	/** Rounding mode for near measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Mode==EPCGExInlinePathProcessingOrder::EndpointCompare && (Comparison==EPCGExComparison::NearlyEqual || Comparison==EPCGExComparison::NearlyNotEqual)", EditConditionHides))
	double Tolerance = DBL_COMPARE_TOLERANCE;

	TArray<FString> Tags;

	void Init()
	{
		Tags = PCGExHelpers::GetStringArrayFromCommaSeparatedList(CommaSeparatedTags);
	}

	bool StartWithFirstIndex(const TSharedRef<PCGExData::FPointIO>& InPointIO)
	{
		if (Mode == EPCGExInlinePathProcessingOrder::FromStart) { return !bInvert; }
		if (Mode == EPCGExInlinePathProcessingOrder::EndpointCompare)
		{
			const TSharedPtr<PCGEx::TAttributeBroadcaster<double>> Value = PCGEx::TAttributeBroadcaster<double>::Make(ComparisonAttribute, InPointIO);
			if (!Value) { return bInvert; }
			const int32 LastIndex = InPointIO->GetNum() - 1;
			const bool Result = PCGExCompare::Compare(
				Comparison,
				Value->SoftGet(0, InPointIO->GetInPoint(0), 0),
				Value->SoftGet(LastIndex, InPointIO->GetInPoint(LastIndex), LastIndex),
				Tolerance);
			return bInvert ? !Result : Result;
		}
		if (Mode == EPCGExInlinePathProcessingOrder::TaggedAny)
		{
			for (const FString& Tag : Tags) { if (InPointIO->Tags->IsTagged(Tag)) { return !bInvert; } }
			return bInvert;
		}
		if (Mode == EPCGExInlinePathProcessingOrder::TaggedAll)
		{
			for (const FString& Tag : Tags) { if (!InPointIO->Tags->IsTagged(Tag)) { return bInvert; } }
			return !bInvert;
		}

		return !bInvert;
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathEdgeIntersectionDetails
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

	void Init()
	{
		MaxDot = bUseMinAngle ? PCGExMath::DegreesToDot(MinAngle) : 1;
		MinDot = bUseMaxAngle ? PCGExMath::DegreesToDot(MaxAngle) : -1;
		ToleranceSquared = Tolerance * Tolerance;
	}

	FORCEINLINE bool CheckDot(const double InDot) const { return InDot <= MaxDot && InDot >= MinDot; }
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathFilterSettings
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

namespace PCGExPaths
{
	PCGEX_ASYNC_STATE(State_BuildingPaths)

	const FName SourceCanCutFilters = TEXT("Can Cut Conditions");
	const FName SourceCanBeCutFilters = TEXT("Can Be Cut Conditions");
	const FName SourceTriggerFilters = TEXT("Trigger Conditions");
	const FName SourceShiftFilters = TEXT("Shift Conditions");

	struct /*PCGEXTENDEDTOOLKIT_API*/ FPathMetrics
	{
		FPathMetrics()
		{
		}

		explicit FPathMetrics(const FVector& InStart)
		{
			Add(InStart);
		}

		explicit FPathMetrics(const TArrayView<FPCGPoint>& Points)
		{
			for (const FPCGPoint& Pt : Points) { Add(Pt.Transform.GetLocation()); }
		}

		FVector Start;
		FVector Last;
		double Length = -1;
		int32 Count = 0;

		void Reset(const FVector& InStart)
		{
			Start = InStart;
			Last = InStart;
			Length = 0;
			Count = 1;
		}

		double Add(const FVector& Location)
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

		bool IsValid() const { return Length > 0; }
		double GetTime(const double Distance) const { return (!Distance || !Length) ? 0 : Distance / Length; }
		double DistToLast(const FVector& Location) const { return FVector::Dist(Last, Location); }
		bool IsLastWithinRange(const FVector& Location, const double Range) const { return DistToLast(Location) < Range; }
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FMetadata
	{
		double Position = 0;
		double TotalLength = 0;

		double GetAlpha() const { return Position / TotalLength; }
		double GetInvertedAlpha() const { return 1 - (Position / TotalLength); }
	};

	constexpr FMetadata InvalidMetadata = FMetadata();

	struct /*PCGEXTENDEDTOOLKIT_API*/ FSplineMeshSegment
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
		FSplineMeshParams Params;

		void ApplySettings(USplineMeshComponent* Component) const
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
			Component->SetSplineUpDir(FVector::UpVector, false);

			Component->SetStartOffset(Params.StartOffset, false);
			Component->SetEndOffset(Params.EndOffset, false);

#if PCGEX_ENGINE_VERSION > 503
			Component->SplineParams.NaniteClusterBoundsScale = Params.NaniteClusterBoundsScale;
#endif

			Component->SplineBoundaryMin = 0;
			Component->SplineBoundaryMax = 0;

			Component->bSmoothInterpRollScale = bSmoothInterpRollScale;

			if (bSetMeshWithSettings) { ApplyMesh(Component); }
		}

		bool ApplyMesh(USplineMeshComponent* Component) const
		{
			check(Component)
			UStaticMesh* StaticMesh = MeshEntry->Staging.TryGet<UStaticMesh>(); //LoadSynchronous<UStaticMesh>();

			if (!StaticMesh) { return false; }

			Component->SetStaticMesh(StaticMesh); // Will trigger a force rebuild, so put this last

			return true;
		}
	};


	struct FPathEdge
	{
		int32 Start = -1;
		int32 End = -1;
		FBoxSphereBounds BSB = FBoxSphereBounds{};

		int32 AltStart = -1;

		FPathEdge(const int32 InStart, const int32 InEnd, const TArrayView<FVector>& Positions, const double Expansion = 0):
			Start(InStart), End(InEnd), AltStart(InStart)
		{
			FBox Box = FBox(ForceInit);
			Box += Positions[Start];
			Box += Positions[End];
			BSB = Box.ExpandBy(Expansion);
		}

		FORCEINLINE bool ShareIndices(const FPathEdge& Other) const
		{
			return Start == Other.Start || Start == Other.End || End == Other.Start || End == Other.End;
		}

		FORCEINLINE bool ShareIndices(const FPathEdge* Other) const
		{
			return Start == Other->Start || Start == Other->End || End == Other->Start || End == Other->End;
		}
	};

	PCGEX_OCTREE_SEMANTICS(FPathEdge, { return Element->BSB;}, { return A == B; })

	class FPath : public TSharedFromThis<FPath>
	{
	protected:
		bool bClosedLoop = false;
		TArray<FVector> Positions;
		TUniquePtr<FPathEdgeOctree> EdgeOctree;
		TArray<double> Lengths;

	public:
		virtual ~FPath() = default;
		FBox Bounds = FBox(ForceInit);
		TArray<FPathEdge> Edges;
		int32 NumPoints = 0;
		int32 NumEdges = 0;
		int32 LastIndex = 0;
		int32 LastEdge = 0;

		int32 ConvexitySign = 0;
		bool bIsConvex = true;

		int32 IOIndex = -1;

		virtual int32 SafePointIndex(const int32 Index) const = 0;

		FORCEINLINE virtual const FVector& GetPos(const int32 Index) const { return Positions[SafePointIndex(Index)]; }
		FORCEINLINE virtual const FVector& GetPosUnsafe(const int32 Index) const { return Positions[Index]; }
		FORCEINLINE bool IsValidEdgeIndex(const int32 Index) const { return Index >= 0 && Index < NumEdges; }

		virtual FVector DirToNextPoint(const int32 Index) const = 0;
		FVector DirToPrevPoint(const int32 Index) const { return DirToNextPoint(SafePointIndex(Index - 1)) * -1; }
		virtual double DistToNextPoint(const int32 Index) const = 0;
		double DistToPrevPoint(const int32 Index) const { return DistToNextPoint(SafePointIndex(Index - 1)); }

		virtual int32 NextPointIndex(const int32 Index) const { return SafePointIndex(Index + 1); }
		virtual int32 PrevPointIndex(const int32 Index) const { return SafePointIndex(Index - 1); }

		FORCEINLINE FVector GetEdgeDir(const FPathEdge& Edge) const { return (Positions[Edge.Start] - Positions[Edge.End]).GetSafeNormal(); }
		FORCEINLINE FVector GetEdgeDir(const int32 Index) const { return GetEdgeDir(Edges[Index]); }
		FORCEINLINE virtual double GetEdgeLength(const FPathEdge& Edge) const = 0;
		FORCEINLINE double GetEdgeLength(const int32 Index) const { return GetEdgeLength(Edges[Index]); }
		FORCEINLINE FVector GetEdgePositionAtAlpha(const FPathEdge& Edge, const double Alpha) const { return FMath::Lerp(Positions[Edge.Start], Positions[Edge.End], Alpha); }
		FORCEINLINE FVector GetEdgePositionAtAlpha(const int32 Index, const double Alpha) const
		{
			const FPathEdge& Edge = Edges[Index];
			return FMath::Lerp(Positions[Edge.Start], Positions[Edge.End], Alpha);
		}

		FORCEINLINE virtual bool IsEdgeValid(const FPathEdge& Edge) const { return GetEdgeLength(Edge) > 0; }
		FORCEINLINE virtual bool IsEdgeValid(const int32 Index) const { return GetEdgeLength(Edges[Index]) > 0; }

		void BuildEdgeOctree()
		{
			if (EdgeOctree) { return; }
			EdgeOctree = MakeUnique<FPathEdgeOctree>(Bounds.GetCenter(), Bounds.GetExtent().Length() + 10);
			for (FPathEdge& Edge : Edges)
			{
				if (!IsEdgeValid(Edge)) { continue; } // Skip zero-length edges
				EdgeOctree->AddElement(&Edge);        // Might be a problem if edges gets reallocated
			}
		}

		void BuildPartialEdgeOctree(const TArray<bool>& Filter)
		{
			if (EdgeOctree) { return; }
			EdgeOctree = MakeUnique<FPathEdgeOctree>(Bounds.GetCenter(), Bounds.GetExtent().Length() + 10);
			for (FPathEdge& Edge : Edges)
			{
				if (!IsEdgeValid(Edge)) { continue; } // Skip zero-length edges
				EdgeOctree->AddElement(&Edge);        // Might be a problem if edges gets reallocated
			}
		}

		const FPathEdgeOctree* GetEdgeOctree() const { return EdgeOctree.Get(); }
		FORCEINLINE bool IsClosedLoop() const { return bClosedLoop; }

		void UpdateConvexity(const int32 Index)
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
				Positions[A], Positions[Index], Positions[B],
				bIsConvex, ConvexitySign);
		}
	};

	template <bool ClosedLoop = false, bool CacheLength = false>
	class TPath : public FPath
	{
	public:
		TPath(const TArray<FPCGPoint>& InPoints, const double Expansion = 0)
		{
			bClosedLoop = ClosedLoop;

			NumPoints = InPoints.Num();
			LastIndex = NumPoints - 1;

			Positions.SetNumUninitialized(NumPoints);
			for (int i = 0; i < NumPoints; i++) { *(Positions.GetData() + i) = InPoints[i].Transform.GetLocation(); }

			Positions = MakeArrayView(Positions.GetData(), NumPoints);

			if constexpr (ClosedLoop) { NumEdges = NumPoints; }
			else { NumEdges = LastIndex; }
			LastEdge = NumEdges - 1;

			Edges.SetNumUninitialized(NumEdges);
			if constexpr (CacheLength)
			{
				Lengths.SetNumUninitialized(NumEdges);
				for (int i = 0; i < NumEdges; i++)
				{
					const FPathEdge& E = (Edges[i] = FPathEdge(i, (i + 1) % NumPoints, Positions, Expansion));
					Bounds += E.BSB.GetBox();
					Lengths[i] = FVector::Dist(Positions[E.Start], Positions[E.End]);
				}
			}
			else
			{
				for (int i = 0; i < NumEdges; i++)
				{
					const FPathEdge& E = (Edges[i] = FPathEdge(i, (i + 1) % NumPoints, Positions, Expansion));
					Bounds += E.BSB.GetBox();
				}
			}
		}

		FORCEINLINE virtual int32 SafePointIndex(const int32 Index) const override
		{
			if constexpr (ClosedLoop) { return PCGExMath::Tile(Index, 0, LastIndex); }
			else { return Index < 0 ? 0 : Index > LastIndex ? LastIndex : Index; }
		}

		FORCEINLINE virtual FVector DirToNextPoint(const int32 Index) const override
		{
			if constexpr (ClosedLoop) { return GetEdgeDir(Index); }
			else { return Index == LastIndex ? GetEdgeDir(Index - 1) * -1 : GetEdgeDir(Index); }
		}

		FORCEINLINE virtual double DistToNextPoint(const int32 Index) const override
		{
			if constexpr (CacheLength)
			{
				if constexpr (ClosedLoop) { return Lengths[Edges[Index].Start]; }
				else { return Index >= NumEdges ? 0 : Lengths[Edges[Index].Start]; }
			}
			else
			{
				if constexpr (ClosedLoop) { return FPath::GetEdgeLength(Index); }
				else { return Index >= NumEdges ? 0 : FPath::GetEdgeLength(Index); }
			}
		}

		FORCEINLINE virtual double GetEdgeLength(const FPathEdge& Edge) const override
		{
			if constexpr (CacheLength) { return Lengths[Edge.Start]; }
			else { return FVector::Dist(Positions[Edge.End], Positions[Edge.Start]); }
		}
	};

	static TSharedPtr<FPath> MakePath(
		const TArray<FPCGPoint>& InPoints,
		const double Expansion,
		const bool bClosedLoop,
		const bool bCacheLength)
	{
		if (bClosedLoop)
		{
			if (bCacheLength)
			{
				TSharedPtr<TPath<true, true>> P = MakeShared<TPath<true, true>>(InPoints, Expansion);
				return StaticCastSharedPtr<FPath>(P);
			}
			TSharedPtr<TPath<true, false>> P = MakeShared<TPath<true, false>>(InPoints, Expansion);
			return StaticCastSharedPtr<FPath>(P);
		}
		if (bCacheLength)
		{
			TSharedPtr<TPath<false, true>> P = MakeShared<TPath<false, true>>(InPoints, Expansion);
			return StaticCastSharedPtr<FPath>(P);
		}
		TSharedPtr<TPath<false, false>> P = MakeShared<TPath<false, false>>(InPoints, Expansion);
		return StaticCastSharedPtr<FPath>(P);
	}
}

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Collections/PCGExMeshCollection.h"
#include "Components/SplineMeshComponent.h"

#include "PCGExPaths.generated.h"

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
	EPCGExInputScope Scope = EPCGExInputScope::All;

	/** Whether the paths are closed loops. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bClosedLoop = false;

	/** Comma separated tags */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Scope!=EPCGExInputScope::All", EditConditionHides))
	FString CommaSeparatedTags = TEXT("Closed");

	TArray<FString> Tags;

	void Init()
	{
		Tags = PCGEx::GetStringArrayFromCommaSeparatedList(CommaSeparatedTags);
	}

	bool IsClosedLoop(const PCGExData::FPointIO* InPointIO)
	{
		if (Tags.IsEmpty()) { return bClosedLoop; }
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
	FString CommaSeparatedAddTags = TEXT("Open");

	/** Tags to be removed from closed paths that are broken into open paths */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Remove Open Tags"))
	FString CommaSeparatedRemoveTags = TEXT("Closed");

	TArray<FString> AddTags;
	TArray<FString> RemoveTags;

	void Init()
	{
		AddTags = PCGEx::GetStringArrayFromCommaSeparatedList(CommaSeparatedAddTags);
		RemoveTags = PCGEx::GetStringArrayFromCommaSeparatedList(CommaSeparatedRemoveTags);
	}

	void Update(const PCGExData::FPointIO* InPointIO)
	{
		for (const FString& Add : AddTags) { InPointIO->Tags->Add(Add); }
		for (const FString& Rem : RemoveTags) { InPointIO->Tags->Remove(Rem); }
	}
};

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathEdgeIntersectionDetails
{
	GENERATED_BODY()

	/** If disabled, edges will only be checked against other datasets. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	bool bEnableSelfIntersection = true;

	/** Distance at which two edges are considered intersecting. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	double Tolerance = 0.001;
	double ToleranceSquared = 0.001;

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

namespace PCGExPaths
{
	const FName SourceCanCutFilters = TEXT("Can Cut Conditions");
	const FName SourceCanBeCutFilters = TEXT("Can Be Cut Conditions");
	const FName SourceTriggerFilters = TEXT("Trigger Conditions");

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

	struct /*PCGEXTENDEDTOOLKIT_API*/ FPathEdge
	{
		int32 Start = -1;
		int32 End = -1;
		FBoxSphereBounds FSBounds = FBoxSphereBounds{};
		int32 OffsetedStart = -1;

		FPathEdge(const int32 InStart, const int32 InEnd, const TArray<FVector>& Positions, const double Tolerance):
			Start(InStart), End(InEnd), OffsetedStart(InStart)
		{
			FBox Box = FBox(ForceInit);
			Box += Positions[Start];
			Box += Positions[End];
			FSBounds = Box.ExpandBy(Tolerance);
		}
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FPathEdgeSemantics
	{
		enum { MaxElementsPerLeaf = 16 };

		enum { MinInclusiveElementsPerNode = 7 };

		enum { MaxNodeDepth = 12 };

		using ElementAllocator = TInlineAllocator<MaxElementsPerLeaf>;

		FORCEINLINE static const FBoxSphereBounds& GetBoundingBox(const FPathEdge* InEdge)
		{
			return InEdge->FSBounds;
		}

		FORCEINLINE static const bool AreElementsEqual(const FPathEdge* A, const FPathEdge* B)
		{
			return A == B;
		}

		FORCEINLINE static void ApplyOffset(FPathEdge& InEdge)
		{
			ensureMsgf(false, TEXT("Not implemented"));
		}

		FORCEINLINE static void SetElementId(const FPathEdge* Element, FOctreeElementId2 OctreeElementID)
		{
		}
	};

	struct /*PCGEXTENDEDTOOLKIT_API*/ FSplineMeshSegment
	{
		FSplineMeshSegment()
		{
		}

		bool bSetMeshWithSettings = false;
		bool bSmoothInterpRollScale = true;
		bool bUseDegrees = true;
		FVector UpVector = FVector::UpVector;

		ESplineMeshAxis::Type SplineMeshAxis = ESplineMeshAxis::Type::X;

		const FPCGExAssetStagingData* AssetStaging = nullptr;
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

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
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
			UStaticMesh* StaticMesh = AssetStaging->TryGet<UStaticMesh>(); //LoadSynchronous<UStaticMesh>();

			if (!StaticMesh) { return false; }

			Component->SetStaticMesh(StaticMesh); // Will trigger a force rebuild, so put this last

			return true;
		}
	};
}

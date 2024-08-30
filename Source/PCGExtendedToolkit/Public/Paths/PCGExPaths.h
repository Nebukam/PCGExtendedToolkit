// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExPaths.generated.h"

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

	/** . */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	//bool bUseMinAngle = true;

	/** Min angle. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseMinAngle", Units="Degrees", ClampMin=0, ClampMax=90))
	//double MinAngle = 0;
	//double MinDot = -1;

	/** . */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	//bool bUseMaxAngle = true;

	/** Maximum angle. */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bUseMaxAngle", Units="Degrees", ClampMin=0, ClampMax=90))
	//double MaxAngle = 90;
	//double MaxDot = 1;

	//

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteCrossing = false;

	/** Name of the attribute to flag point as crossing (result of an Edge/Edge intersection) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Metadata", meta=(PCG_Overridable, EditCondition="bWriteCrossing"))
	FName CrossingAttributeName = "bCrossing";

	void ComputeDot()
	{
		//MinDot = bUseMinAngle ? PCGExMath::DegreesToDot(MinAngle * 0.5) : -1;
		//MaxDot = bUseMaxAngle ? PCGExMath::DegreesToDot(MaxAngle * 0.5) : 1;
		ToleranceSquared = Tolerance * Tolerance;
	}
};

namespace PCGExPaths
{
	const FName SourceCanCutFilters = TEXT("Can Cut Conditions");
	const FName SourceCanBeCutFilters = TEXT("Can Be Cut Conditions");

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
		bool bCanBeCut = false;
		bool bCanCut = false;
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
}

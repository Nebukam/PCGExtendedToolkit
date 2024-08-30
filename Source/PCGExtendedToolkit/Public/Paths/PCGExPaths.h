// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

namespace PCGExPaths
{
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
		bool bCuttable = false;
		bool bCutter = false;
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

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExMT.h"
#include "Algo/Unique.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExEdge.generated.h"

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true", DisplayName="[PCGEx] Edge Type"))
enum class EPCGExEdgeType : uint8
{
	Unknown  = 0 UMETA(DisplayName = "Unknown", Tooltip="Unknown type."),
	Roaming  = 1 << 0 UMETA(DisplayName = "Roaming", Tooltip="Unidirectional edge."),
	Shared   = 1 << 1 UMETA(DisplayName = "Shared", Tooltip="Shared edge, both sockets are connected; but do not match."),
	Match    = 1 << 2 UMETA(DisplayName = "Match", Tooltip="Shared relation, considered a match by the primary socket owner; but does not match on the other."),
	Complete = 1 << 3 UMETA(DisplayName = "Complete", Tooltip="Shared, matching relation on both sockets."),
	Mirror   = 1 << 4 UMETA(DisplayName = "Mirrored relation", Tooltip="Mirrored relation, connected sockets are the same on both points."),
};


USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExDebugEdgeSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere)
	FColor ValidEdgeColor = FColor::Cyan;

	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta=(ClampMin=0.1, ClampMax=10))
	double ValidEdgeThickness = 0.5;

	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere)
	FColor InvalidEdgeColor = FColor::Red;

	UPROPERTY(BlueprintReadWrite, Category = Settings, EditAnywhere, meta=(ClampMin=0.1, ClampMax=10))
	double InvalidEdgeThickness = 0.5;
};

namespace PCGExGraph
{
	const FName SourceEdgesLabel = TEXT("Edges");
	const FName OutputEdgesLabel = TEXT("Edges");

	const FName SourcePackedClustersLabel = TEXT("Packed Clusters");
	const FName OutputPackedClustersLabel = TEXT("Packed Clusters");

	const FName Tag_EdgeEndpoints = TEXT("PCGEx/EdgeEndpoints");
	const FName Tag_EdgeIndex = TEXT("PCGEx/CachedIndex");
	const FName Tag_EdgesNum = TEXT("PCGEx/CachedEdgeNum");
	const FName Tag_ClusterIndex = TEXT("PCGEx/ClusterIndex");

	const FName Tag_ClusterPair = TEXT("PCGEx/ClusterPair");
	const FString TagStr_ClusterPair = Tag_ClusterPair.ToString();
	const FName Tag_ClusterId = TEXT("PCGEx/ClusterId");

	constexpr PCGExMT::AsyncState State_ReadyForNextEdges = __COUNTER__;
	constexpr PCGExMT::AsyncState State_ProcessingEdges = __COUNTER__;
	constexpr PCGExMT::AsyncState State_BuildingClusters = __COUNTER__;

	FORCEINLINE static uint32 HCID(const PCGMetadataEntryKey Key)
	{
		uint32 A;
		uint32 B;
		PCGEx::H64(Key, A, B);
		// Only use the right part if it exist
		// It's probably safe to assume that metadataEntry busted the left bits, there's no trace of the first ones anymore.
		return A == 0 ? B : A;
	}

	FORCEINLINE static uint64 HCID(const PCGMetadataEntryKey A, const PCGMetadataEntryKey B) { return PCGEx::H64(HCID(A), HCID(B)); }

	struct PCGEXTENDEDTOOLKIT_API FEdge
	{
		uint32 Start = 0;
		uint32 End = 0;
		EPCGExEdgeType Type = EPCGExEdgeType::Unknown;
		bool bValid = true;

		FEdge()
		{
		}

		FEdge(const int32 InStart, const int32 InEnd):
			Start(InStart), End(InEnd)
		{
			bValid = InStart != InEnd && InStart != -1 && InEnd != -1;
		}

		FEdge(const int32 InStart, const int32 InEnd, const EPCGExEdgeType InType)
			: FEdge(InStart, InEnd)
		{
			Type = InType;
		}

		bool Contains(const int32 InIndex) const { return Start == InIndex || End == InIndex; }

		uint32 Other(const int32 InIndex) const
		{
			check(InIndex == Start || InIndex == End)
			return InIndex == Start ? End : Start;
		}

		bool operator==(const FEdge& Other) const
		{
			return Start == Other.Start && End == Other.End;
		}

		explicit operator uint64() const
		{
			return static_cast<uint64>(Start) | (static_cast<uint64>(End) << 32);
		}

		explicit FEdge(const uint64 InValue)
		{
			Start = static_cast<uint32>(InValue & 0xFFFFFFFF);
			End = static_cast<uint32>((InValue >> 32) & 0xFFFFFFFF);
			Type = EPCGExEdgeType::Unknown;
		}
	};

	struct PCGEXTENDEDTOOLKIT_API FUnsignedEdge : public FEdge
	{
		FUnsignedEdge()
		{
		}

		FUnsignedEdge(const int32 InStart, const int32 InEnd):
			FEdge(InStart, InEnd)
		{
		}

		FUnsignedEdge(const int32 InStart, const int32 InEnd, const EPCGExEdgeType InType):
			FEdge(InStart, InEnd, InType)
		{
		}

		bool operator==(const FUnsignedEdge& Other) const
		{
			return H64U() == Other.H64U();
		}

		explicit FUnsignedEdge(const uint64 InValue)
		{
			PCGEx::H64(InValue, Start, End);
			Type = EPCGExEdgeType::Unknown;
		}

		FORCEINLINE uint64 H64U() const { return PCGEx::H64U(Start, End); }
	};

	struct PCGEXTENDEDTOOLKIT_API FIndexedEdge : public FUnsignedEdge
	{
		int32 EdgeIndex = -1;
		int32 PointIndex = -1;
		int32 IOIndex = -1;

		FIndexedEdge()
		{
		}

		FIndexedEdge(const FIndexedEdge& Other)
			: FUnsignedEdge(Other.Start, Other.End),
			  EdgeIndex(Other.EdgeIndex), PointIndex(Other.PointIndex), IOIndex(Other.IOIndex)
		{
		}

		FIndexedEdge(const int32 InIndex, const int32 InStart, const int32 InEnd, const int32 InPointIndex = -1, const int32 InIOIndex = -1)
			: FUnsignedEdge(InStart, InEnd),
			  EdgeIndex(InIndex), PointIndex(InPointIndex), IOIndex(InIOIndex)
		{
		}
	};

	static bool BuildIndexedEdges(
		const PCGExData::FPointIO& EdgeIO,
		const TMap<int64, int32>& EndpointsLookup,
		TArray<FIndexedEdge>& OutEdges,
		const bool bStopOnError = false)
	{

		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExEdge::BuildIndexedEdges-Vanilla);
		
		PCGEx::TFAttributeReader<int64>* EndpointsReader = new PCGEx::TFAttributeReader<int64>(Tag_EdgeEndpoints);
		if (!EndpointsReader->Bind(const_cast<PCGExData::FPointIO&>(EdgeIO)))
		{
			PCGEX_DELETE(EndpointsReader)
			return false;
		}

		bool bValid = true;
		const int32 NumEdges = EdgeIO.GetNum();

		OutEdges.SetNum(NumEdges);

		if (!bStopOnError)
		{
			int32 EdgeIndex = 0;

			for (int i = 0; i < NumEdges; i++)
			{
				uint32 A;
				uint32 B;
				PCGEx::H64(EndpointsReader->Values[i], A, B);

				const int32* StartPointIndexPtr = EndpointsLookup.Find(A);
				const int32* EndPointIndexPtr = EndpointsLookup.Find(B);

				if ((!StartPointIndexPtr || !EndPointIndexPtr)) { continue; }

				OutEdges[EdgeIndex] = FIndexedEdge(EdgeIndex, *StartPointIndexPtr, *EndPointIndexPtr, EdgeIndex, EdgeIO.IOIndex);
				EdgeIndex++;
			}

			OutEdges.SetNum(EdgeIndex);
		}
		else
		{
			for (int i = 0; i < NumEdges; i++)
			{
				uint32 A;
				uint32 B;
				PCGEx::H64(EndpointsReader->Values[i], A, B);

				const int32* StartPointIndexPtr = EndpointsLookup.Find(A);
				const int32* EndPointIndexPtr = EndpointsLookup.Find(B);

				if ((!StartPointIndexPtr || !EndPointIndexPtr))
				{
					bValid = false;
					break;
				}

				OutEdges[i] = FIndexedEdge(i, *StartPointIndexPtr, *EndPointIndexPtr, i, EdgeIO.IOIndex);
			}
		}

		PCGEX_DELETE(EndpointsReader)

		return bValid;
	}

	static bool BuildIndexedEdges(
		const PCGExData::FPointIO& EdgeIO,
		const TMap<int64, int32>& EndpointsLookup,
		TArray<FIndexedEdge>& OutEdges,
		TSet<int32>& OutNodePoints,
		const bool bStopOnError = false)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExEdge::BuildIndexedEdges-WithPoints);
		//EdgeIO.CreateInKeys();

		PCGEx::TFAttributeReader<int64>* EndpointsReader = new PCGEx::TFAttributeReader<int64>(Tag_EdgeEndpoints);

		if (!EndpointsReader->Bind(const_cast<PCGExData::FPointIO&>(EdgeIO)))
		{
			PCGEX_DELETE(EndpointsReader)
			return false;
		}

		bool bValid = true;
		const int32 NumEdges = EdgeIO.GetNum();

		OutEdges.SetNumUninitialized(NumEdges);

		if (!bStopOnError)
		{
			int32 EdgeIndex = 0;

			for (int i = 0; i < NumEdges; i++)
			{
				uint32 A;
				uint32 B;
				PCGEx::H64(EndpointsReader->Values[i], A, B);

				const int32* StartPointIndexPtr = EndpointsLookup.Find(A);
				const int32* EndPointIndexPtr = EndpointsLookup.Find(B);

				if ((!StartPointIndexPtr || !EndPointIndexPtr)) { continue; }

				OutNodePoints.Add(*StartPointIndexPtr);
				OutNodePoints.Add(*EndPointIndexPtr);

				OutEdges[EdgeIndex] = FIndexedEdge(EdgeIndex, *StartPointIndexPtr, *EndPointIndexPtr, EdgeIndex, EdgeIO.IOIndex);
				EdgeIndex++;
			}

			OutEdges.SetNum(EdgeIndex);
		}
		else
		{
			for (int i = 0; i < NumEdges; i++)
			{
				uint32 A;
				uint32 B;
				PCGEx::H64(EndpointsReader->Values[i], A, B);

				const int32* StartPointIndexPtr = EndpointsLookup.Find(A);
				const int32* EndPointIndexPtr = EndpointsLookup.Find(B);

				if ((!StartPointIndexPtr || !EndPointIndexPtr))
				{
					bValid = false;
					break;
				}

				OutNodePoints.Add(*StartPointIndexPtr);
				OutNodePoints.Add(*EndPointIndexPtr);

				OutEdges[i] = FIndexedEdge(i, *StartPointIndexPtr, *EndPointIndexPtr, i, EdgeIO.IOIndex);
			}
		}

		//OutNodePoints = UniquePoints.Array();

		PCGEX_DELETE(EndpointsReader)

		return bValid;
	}
}

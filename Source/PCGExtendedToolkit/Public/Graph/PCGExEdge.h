// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "PCGExMT.h"
#include "Data/PCGExAttributeHelpers.h"

#include "PCGExEdge.generated.h"


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
	const FName SourcePickersLabel = TEXT("Pickers");

	const FName SourceEdgesLabel = TEXT("Edges");
	const FName OutputEdgesLabel = TEXT("Edges");
	const FName OutputSitesLabel = TEXT("Sites");

	const FName SourcePackedClustersLabel = TEXT("Packed Clusters");
	const FName OutputPackedClustersLabel = TEXT("Packed Clusters");

	const FName Tag_EdgeEndpoints = FName(PCGEx::PCGExPrefix + TEXT("EdgeEndpoints"));
	const FName Tag_VtxEndpoint = FName(PCGEx::PCGExPrefix + TEXT("VtxEndpoint"));
	const FName Tag_ClusterIndex = FName(PCGEx::PCGExPrefix + TEXT("ClusterIndex"));

	const FName Tag_ClusterPair = FName(PCGEx::PCGExPrefix + TEXT("ClusterPair"));
	const FString TagStr_ClusterPair = Tag_ClusterPair.ToString();
	const FName Tag_ClusterId = FName(PCGEx::PCGExPrefix + TEXT("ClusterId"));

	const FName Tag_PCGExVtx = FName(PCGEx::PCGExPrefix + TEXT("ClusterVtx"));
	const FString TagStr_PCGExVtx = Tag_PCGExVtx.ToString();
	const FName Tag_PCGExEdges = FName(PCGEx::PCGExPrefix + TEXT("ClusterEdges"));
	const FString TagStr_PCGExEdges = Tag_PCGExEdges.ToString();

	PCGEX_ASYNC_STATE(State_ReadyForNextEdges)
	PCGEX_ASYNC_STATE(State_ProcessingEdges)
	PCGEX_ASYNC_STATE(State_BuildingClusters)

	FORCEINLINE static uint32 NodeGUID(const uint64 Base, const int32 Index)
	{
		uint32 A;
		uint32 B;
		PCGEx::H64(Base, A, B);
		return HashCombineFast(A == 0 ? B : A, Index);
	}

	struct PCGEXTENDEDTOOLKIT_API FEdge
	{
		uint32 Start = 0;
		uint32 End = 0;
		bool bValid = true;

		FEdge()
		{
		}

		FEdge(const int32 InStart, const int32 InEnd):
			Start(InStart), End(InEnd)
		{
			bValid = InStart != InEnd && InStart != -1 && InEnd != -1;
		}

		bool Contains(const int32 InIndex) const { return Start == InIndex || End == InIndex; }

		FORCEINLINE uint32 Other(const int32 InIndex) const
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

		bool operator==(const FUnsignedEdge& Other) const
		{
			return H64U() == Other.H64U();
		}

		explicit FUnsignedEdge(const uint64 InValue)
		{
			PCGEx::H64(InValue, Start, End);
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
		const PCGExData::FPointIO* EdgeIO,
		const TMap<uint32, int32>& EndpointsLookup,
		TArray<FIndexedEdge>& OutEdges,
		const bool bStopOnError = false)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExEdge::BuildIndexedEdges-Vanilla);

		PCGEx::TFAttributeReader<int64>* EndpointsReader = new PCGEx::TFAttributeReader<int64>(Tag_EdgeEndpoints);
		if (!EndpointsReader->Bind(const_cast<PCGExData::FPointIO*>(EdgeIO)))
		{
			PCGEX_DELETE(EndpointsReader)
			return false;
		}

		bool bValid = true;
		const int32 NumEdges = EdgeIO->GetNum();

		PCGEX_SET_NUM_UNINITIALIZED(OutEdges, NumEdges)

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

				OutEdges[EdgeIndex] = FIndexedEdge(EdgeIndex, *StartPointIndexPtr, *EndPointIndexPtr, EdgeIndex, EdgeIO->IOIndex);
				EdgeIndex++;
			}

			PCGEX_SET_NUM_UNINITIALIZED(OutEdges, EdgeIndex)
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

				OutEdges[i] = FIndexedEdge(i, *StartPointIndexPtr, *EndPointIndexPtr, i, EdgeIO->IOIndex);
			}
		}

		PCGEX_DELETE(EndpointsReader)

		return bValid;
	}

	static bool BuildIndexedEdges(
		const PCGExData::FPointIO* EdgeIO,
		const TMap<uint32, int32>& EndpointsLookup,
		TArray<FIndexedEdge>& OutEdges,
		TSet<int32>& OutNodePoints,
		const bool bStopOnError = false)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExEdge::BuildIndexedEdges-WithPoints);
		//EdgeIO.CreateInKeys();

		PCGEx::TFAttributeReader<int64>* EndpointsReader = new PCGEx::TFAttributeReader<int64>(Tag_EdgeEndpoints);

		if (!EndpointsReader->Bind(const_cast<PCGExData::FPointIO*>(EdgeIO)))
		{
			PCGEX_DELETE(EndpointsReader)
			return false;
		}

		bool bValid = true;
		const int32 NumEdges = EdgeIO->GetNum();

		PCGEX_SET_NUM_UNINITIALIZED(OutEdges, NumEdges)

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

				OutEdges[EdgeIndex] = FIndexedEdge(EdgeIndex, *StartPointIndexPtr, *EndPointIndexPtr, EdgeIndex, EdgeIO->IOIndex);
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

				OutEdges[i] = FIndexedEdge(i, *StartPointIndexPtr, *EndPointIndexPtr, i, EdgeIO->IOIndex);
			}
		}

		PCGEX_DELETE(EndpointsReader)

		return bValid;
	}

	static void SetClusterVtx(const PCGExData::FPointIO* IO, FString& OutId)
	{
		IO->Tags->Set(TagStr_ClusterPair, IO->GetOutIn()->UID, OutId);
		IO->Tags->RawTags.Add(TagStr_PCGExVtx);
		IO->Tags->RawTags.Remove(TagStr_PCGExEdges);
	}

	static void MarkClusterVtx(const PCGExData::FPointIO* IO, const FString& Id)
	{
		IO->Tags->Set(TagStr_ClusterPair, Id);
		IO->Tags->RawTags.Add(TagStr_PCGExVtx);
		IO->Tags->RawTags.Remove(TagStr_PCGExEdges);
	}

	static void MarkClusterEdges(const PCGExData::FPointIO* IO, const FString& Id)
	{
		IO->Tags->Set(TagStr_ClusterPair, Id);
		IO->Tags->RawTags.Add(TagStr_PCGExEdges);
		IO->Tags->RawTags.Remove(TagStr_PCGExVtx);
	}

	static void MarkClusterEdges(const TArrayView<PCGExData::FPointIO*> Edges, const FString& Id)
	{
		for (const PCGExData::FPointIO* IO : Edges) { MarkClusterEdges(IO, Id); }
	}

	static void CleanupClusterTags(const PCGExData::FPointIO* IO, const bool bKeepPairTag = false)
	{
		IO->Tags->Remove(TagStr_ClusterPair);
		IO->Tags->Remove(TagStr_PCGExEdges);
		if (!bKeepPairTag) { IO->Tags->Remove(TagStr_PCGExVtx); }
	}

	static void CleanupClusterTags(const TArrayView<PCGExData::FPointIO*> IOs, const bool bKeepPairTag = false)
	{
		for (const PCGExData::FPointIO* IO : IOs) { CleanupClusterTags(IO, bKeepPairTag); }
	}
}

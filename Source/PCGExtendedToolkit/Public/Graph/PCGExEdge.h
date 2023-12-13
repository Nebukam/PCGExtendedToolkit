// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

#include "Data/PCGExPointIO.h"
#include "Data/PCGExAttributeHelpers.h"
#include "PCGExMT.h"

#include "PCGExEdge.generated.h"

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class EPCGExEdgeType : uint8
{
	Unknown  = 0 UMETA(DisplayName = "Unknown", Tooltip="Unknown type."),
	Roaming  = 1 << 0 UMETA(DisplayName = "Roaming", Tooltip="Unidirectional edge."),
	Shared   = 1 << 1 UMETA(DisplayName = "Shared", Tooltip="Shared edge, both sockets are connected; but do not match."),
	Match    = 1 << 2 UMETA(DisplayName = "Match", Tooltip="Shared relation, considered a match by the primary socket owner; but does not match on the other."),
	Complete = 1 << 3 UMETA(DisplayName = "Complete", Tooltip="Shared, matching relation on both sockets."),
	Mirror   = 1 << 4 UMETA(DisplayName = "Mirrored relation", Tooltip="Mirrored relation, connected sockets are the same on both points."),
};

namespace PCGExGraph
{
	const FName SourceEdgesLabel = TEXT("Edges");
	const FName OutputEdgesLabel = TEXT("Edges");

	struct PCGEXTENDEDTOOLKIT_API FEdge
	{
		uint32 Start = 0;
		uint32 End = 0;
		EPCGExEdgeType Type = EPCGExEdgeType::Unknown;

		FEdge()
		{
		}

		FEdge(const int32 InStart, const int32 InEnd, const EPCGExEdgeType InType):
			Start(InStart), End(InEnd), Type(InType)
		{
		}

		uint32 Other(int32 InIndex) const
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
		FUnsignedEdge(): FEdge()
		{
		}

		FUnsignedEdge(const int32 InStart, const int32 InEnd, const EPCGExEdgeType InType):
			FEdge(InStart, InEnd, InType)
		{
		}

		bool operator==(const FUnsignedEdge& Other) const
		{
			return GetUnsignedHash() == Other.GetUnsignedHash();
		}

		explicit FUnsignedEdge(const uint64 InValue)
		{
			Start = static_cast<uint32>(InValue & 0xFFFFFFFF);
			End = static_cast<uint32>((InValue >> 32) & 0xFFFFFFFF);
			Type = EPCGExEdgeType::Unknown;
		}

		uint64 GetUnsignedHash() const
		{
			return Start > End ?
				       static_cast<uint64>(Start) | (static_cast<uint64>(End) << 32) :
				       static_cast<uint64>(End) | (static_cast<uint64>(Start) << 32);
		}
	};

	struct FNode
	{
		FNode()
		{
		}

		FNode(int32 InIndex, int32 InReserve)
			: Index(InIndex)
		{
			Edges.Reserve(InReserve);
		}

		int32 Index = -1;
		TSet<int32> Edges;
		
		void GetNeighbors(TSet<int32>& OutNeighbors, TArray<uint64>& InEdges)
		{
			for (const int32 Edge : Edges) { OutNeighbors.Add(FUnsignedEdge(InEdges[Edge]).Other(Index)); }
		}
	};

	struct FNetwork
	{
		int32 NumEdgesReserve = 0;
		TArray<FNode> Nodes;
		TSet<uint64> UniqueEdges;
		TArray<uint64> Edges;
		
		void SetNumNodes(const int32 Num)
		{
			Nodes.SetNum(Num);
			int32 Index = 0;
			for (FNode& Node : Nodes)
			{
				Node.Index = Index;
				Node.Edges.Reserve(NumEdgesReserve);
				Index++;
			}
		}

		bool AddEdge(uint64 Edge)
		{
			if (UniqueEdges.Contains(Edge)) { return false; }
			
			UniqueEdges.Add(Edge);
			const int32 EdgeIndex = Edges.Add(Edge);

			const FUnsignedEdge TempEdge = FUnsignedEdge(Edge);
			Nodes[TempEdge.Start].Edges.Add(EdgeIndex);
			Nodes[TempEdge.End].Edges.Add(EdgeIndex);
			
			return true;
		}
	};

#pragma endregion
}

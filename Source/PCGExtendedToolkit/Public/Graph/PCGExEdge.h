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

	const FName EdgeStartAttributeName = TEXT("PCGEx/EdgeStart");
	const FName EdgeEndAttributeName = TEXT("PCGEx/EdgeEnd");
	const FName IslandIndexAttributeName = TEXT("PCGEx/Island");

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

		int32 Index = -1;
		int32 Island = -1;
		TSet<int32> Edges;

		bool IsIsolated() { return Island == -1; }

		bool GetNeighbors(TArray<int32>& OutIndices, const TArray<FUnsignedEdge>& InEdges)
		{
			if (Edges.IsEmpty()) { return false; }
			for (const int32 Edge : Edges) { OutIndices.Add(InEdges[Edge].Other(Index)); }
			return true;
		}

		template <typename Func, typename... Args>
		void CallbackOnNeighbors(const TArray<FUnsignedEdge>& InEdges, Func Callback, Args&&... InArgs)
		{
			for (const int32 Edge : Edges) { Callback(InEdges[Edge].Other(Index, std::forward<Args>(InArgs)...)); }
		}
	};

	struct FNetwork
	{
		const int32 NumEdgesReserve;
		int32 IslandIncrement = 0;
		int32 NumIslands = 0;
		int32 NumEdges = 0;

		TArray<FNode> Nodes;
		TSet<uint64> UniqueEdges;
		TArray<FUnsignedEdge> Edges;
		TMap<int32, int32> IslandSizes;

		FNetwork(const int32 InNumEdgesReserve, const int32 InNumNodes)
			: NumEdgesReserve(InNumEdgesReserve)
		{
			SetNumNodes(InNumNodes);
		}

		void SetNumNodes(const int32 Num)
		{
			Nodes.SetNum(Num);
			int32 Index = 0;
			for (FNode& Node : Nodes)
			{
				Node.Index = Index++;
				Node.Edges.Reserve(NumEdgesReserve);
			}
		}

		bool InsertEdge(FUnsignedEdge Edge)
		{
			const uint64 Hash = Edge.GetUnsignedHash();
			if (UniqueEdges.Contains(Hash)) { return false; }

			UniqueEdges.Add(Hash);
			const int32 EdgeIndex = Edges.Add(Edge);

			FNode& NodeA = Nodes[Edge.Start];
			FNode& NodeB = Nodes[Edge.End];

			NodeA.Edges.Add(EdgeIndex);
			NodeB.Edges.Add(EdgeIndex);

			if (NodeA.Island == -1 && NodeB.Island == -1)
			{
				// New island
				NumIslands++;
				NodeA.Island = NodeB.Island = IslandIncrement++;
			}
			else if (NodeA.Island != -1 && NodeB.Island != -1)
			{
				if (NodeA.Island != NodeB.Island)
				{
					// Merge islands
					NumIslands--;
					MergeIsland(NodeB.Index, NodeA.Island);
				}
			}
			else
			{
				// Expand island
				NodeA.Island = NodeB.Island = FMath::Max(NodeA.Island, NodeB.Island);
			}

			return true;
		}

		void MergeIsland(int32 NodeIndex, int32 Island)
		{
			FNode& Node = Nodes[NodeIndex];
			if (Node.Island == Island) { return; }
			Node.Island = Island;
			Node.CallbackOnNeighbors(
				Edges,
				[&](const int32 OtherIndex)
				{
					MergeIsland(OtherIndex, Island);
				});
		}

		void PrepareIslands(int32 MinSize = 1, int32 MaxSize = TNumericLimits<int32>::Max())
		{
			UniqueEdges.Empty();
			IslandSizes.Empty(NumIslands);
			NumEdges = 0;

			for(const FUnsignedEdge& Edge : Edges)
			{
				FNode& NodeA = Nodes[Edge.Start];
				if (const int32* SizePtr = IslandSizes.Find(NodeA.Island); !SizePtr) { IslandSizes.Add(NodeA.Island, 1); }
				else { IslandSizes.Add(NodeA.Island, *SizePtr + 1); }				
			}
			
			for (TPair<int32, int32>& Pair : IslandSizes)
			{
				if (FMath::IsWithin(Pair.Value, MinSize, MaxSize)) { NumEdges += Pair.Value; }
				else { Pair.Value = -1; }
			}
		}
	};

#pragma endregion
}

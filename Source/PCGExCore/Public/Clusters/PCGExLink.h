// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExH.h"

namespace PCGExGraphs
{
	struct PCGEXCORE_API FLink
	{
		int32 Node = -1;
		int32 Edge = -1;

		constexpr FLink() = default;

		constexpr explicit FLink(const uint64 Hash)
			: Node(PCGEx::H64A(Hash)), Edge(PCGEx::H64A(Hash))
		{
		}

		constexpr FLink(const uint32 InNode, const uint32 InEdge)
			: Node(InNode), Edge(InEdge)
		{
		}

		FORCEINLINE uint64 H64() const { return PCGEx::H64U(Node, Edge); }

		bool operator==(const FLink& Other) const { return Node == Other.Node && Edge == Other.Edge; }
		FORCEINLINE friend uint32 GetTypeHash(const FLink& Key) { return HashCombineFast(Key.Node, Key.Edge); }
	};

	using NodeLinks = TArray<FLink, TInlineAllocator<8>>;
}

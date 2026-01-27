// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include <functional>

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGExH.h"

namespace PCGExData
{
	class FFacade;
}

namespace PCGExGraphs
{
	class FSubGraph;
	class FGraphBuilder;
	struct FEdge;

	using FGraphCompilationEndCallback = std::function<void(const TSharedRef<FGraphBuilder>& InBuilder, const bool bSuccess)>;

	// Legacy callback - prefer context-based callbacks for new code
	using FSubGraphPostProcessCallback = std::function<void(const TSharedRef<FSubGraph>& InSubGraph)>;

#pragma region SubGraphContext

	/**
	 * Base class for user-defined subgraph compilation context.
	 * Derive from this to store custom data between PreCompile and PostCompile callbacks.
	 */
	struct PCGEXGRAPHS_API FSubGraphUserContext
	{
		virtual ~FSubGraphUserContext() = default;
	};

	/**
	 * Data available during PreCompile callback.
	 * Provides access to edge mappings and facades before CompileRange processes edges.
	 */
	struct PCGEXGRAPHS_API FSubGraphPreCompileData
	{
		/** Output edges with point indices (Start/End are point indices in vtx data) */
		TArrayView<const FEdge> FlattenedEdges;

		/** Edge keys where EdgeKeys[i].Index = original edge index in parent graph */
		TArrayView<const PCGEx::FIndexKey> EdgeKeys;

		/** Edge data facade for setting up attribute writers */
		TSharedPtr<PCGExData::FFacade> EdgesDataFacade;

		/** Vertex data facade */
		TSharedPtr<PCGExData::FFacade> VtxDataFacade;

		/** Number of edges in this subgraph */
		int32 NumEdges = 0;

		/** Number of nodes in this subgraph */
		int32 NumNodes = 0;
	};

	/**
	 * Factory callback to create user context.
	 * Return nullptr to skip PreCompile/PostCompile callbacks entirely (zero overhead).
	 */
	using FCreateSubGraphContextCallback = std::function<TSharedPtr<FSubGraphUserContext>()>;

	/**
	 * Called after FlattenedEdges is built but before CompileRange processes edges.
	 * Use this to build index mappings or set up blenders with the edge facade.
	 */
	using FSubGraphPreCompileCallback = std::function<void(FSubGraphUserContext& Context, const FSubGraphPreCompileData& Data)>;

	/**
	 * Called after CompileRange completes, before edge data is written.
	 * Use this for post-processing that requires the context built in PreCompile.
	 */
	using FSubGraphPostCompileCallback = std::function<void(FSubGraphUserContext& Context, const TSharedRef<FSubGraph>& SubGraph)>;

#pragma endregion

	namespace States
	{
		PCGEX_CTX_STATE(State_WritingClusters)
		PCGEX_CTX_STATE(State_ReadyToCompile)
		PCGEX_CTX_STATE(State_Compiling)

		PCGEX_CTX_STATE(State_Pathfinding)
		PCGEX_CTX_STATE(State_WaitingPathfinding)
	}
}

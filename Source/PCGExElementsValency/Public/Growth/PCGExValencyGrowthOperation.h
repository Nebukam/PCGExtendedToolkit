// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Factories/PCGExInstancedFactory.h"
#include "Factories/PCGExOperation.h"
#include "Core/PCGExValencyCommon.h"
#include "Core/PCGExValencyBondingRules.h"
#include "Core/PCGExValencySocketRules.h"
#include "Core/PCGExValencySolverOperation.h"
#include "PCGExValencyGenerativeCommon.h"

#include "PCGExValencyGrowthOperation.generated.h"

/**
 * Base class for Valency growth operations.
 * Subclass this and override SelectNextSocket() to create custom growth strategies.
 *
 * Growth operations expand structures from seed modules by:
 * 1. Selecting an open socket from the frontier
 * 2. Finding compatible modules for that socket
 * 3. Computing attachment transforms and checking bounds
 * 4. Placing modules or marking sockets as failed
 */
class PCGEXELEMENTSVALENCY_API FPCGExValencyGrowthOperation : public FPCGExOperation
{
public:
	FPCGExValencyGrowthOperation() = default;
	virtual ~FPCGExValencyGrowthOperation() override = default;

	/**
	 * Initialize the growth operation with rules and tracking state.
	 */
	virtual void Initialize(
		const FPCGExValencyBondingRulesCompiled* InCompiledRules,
		const UPCGExValencySocketRules* InSocketRules,
		FPCGExBoundsTracker& InBoundsTracker,
		FPCGExGrowthBudget& InBudget,
		int32 InSeed);

	/**
	 * Run the full growth from seed modules.
	 * @param OutPlaced Output array of all placed modules (including seeds)
	 */
	virtual void Grow(TArray<FPCGExPlacedModule>& OutPlaced);

	/**
	 * Compute world bounds for a module at a given transform.
	 * Public so the element can compute seed bounds before growth starts.
	 */
	FBox ComputeWorldBounds(int32 ModuleIndex, const FTransform& WorldTransform) const;

	/** Cached local bounds per module (indexed by module index). Set by element before Grow(). */
	TArray<FBox> ModuleLocalBounds;

protected:
	/** Subclass interface: pick next socket from frontier. Return INDEX_NONE if empty. */
	virtual int32 SelectNextSocket(TArray<FPCGExOpenSocket>& Frontier) PCGEX_NOT_IMPLEMENTED_RET(SelectNextSocket, INDEX_NONE);

	// ========== Shared Utilities ==========

	/**
	 * Find all modules whose sockets are compatible with the given socket type.
	 * @param SocketType The socket type to find compatible modules for
	 * @param OutModuleIndices Module indices that have a compatible socket
	 * @param OutSocketIndices Corresponding socket indices on those modules
	 */
	void FindCompatibleModules(
		FName SocketType,
		TArray<int32>& OutModuleIndices,
		TArray<int32>& OutSocketIndices) const;

	/**
	 * Compute world transform for placing a child module attached at a socket.
	 * Uses plug/receptacle semantics: sockets face each other.
	 */
	FTransform ComputeAttachmentTransform(
		const FPCGExOpenSocket& ParentSocket,
		int32 ChildModuleIndex,
		int32 ChildSocketIndex) const;

	/**
	 * Try to place a module at a socket. Returns true if placed.
	 */
	bool TryPlaceModule(
		const FPCGExOpenSocket& Socket,
		int32 ModuleIndex,
		int32 ChildSocketIndex,
		TArray<FPCGExPlacedModule>& OutPlaced,
		TArray<FPCGExOpenSocket>& OutFrontier);

	/**
	 * Add a placed module's remaining sockets to the frontier.
	 */
	void ExpandFrontier(
		const FPCGExPlacedModule& Placed,
		int32 PlacedIndex,
		int32 UsedSocketIndex,
		TArray<FPCGExOpenSocket>& OutFrontier);

	/**
	 * Weighted random module selection from candidates.
	 * @return Index into CandidateModules array (not module index)
	 */
	int32 SelectWeightedRandom(const TArray<int32>& CandidateModules);

	// ========== State ==========

	const FPCGExValencyBondingRulesCompiled* CompiledRules = nullptr;
	const UPCGExValencySocketRules* SocketRules = nullptr;
	FPCGExBoundsTracker* BoundsTracker = nullptr;
	FPCGExGrowthBudget* Budget = nullptr;
	FRandomStream RandomStream;
	PCGExValency::FDistributionTracker DistributionTracker;
};

/**
 * Base factory for creating Valency growth operations.
 */
UCLASS(Abstract, DefaultToInstanced, EditInlineNew, BlueprintType)
class PCGEXELEMENTSVALENCY_API UPCGExValencyGrowthFactory : public UPCGExInstancedFactory
{
	GENERATED_BODY()

public:
	virtual TSharedPtr<FPCGExValencyGrowthOperation> CreateOperation() const PCGEX_NOT_IMPLEMENTED_RET(CreateOperation, nullptr);
};

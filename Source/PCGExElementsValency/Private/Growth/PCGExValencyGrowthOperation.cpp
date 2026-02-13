// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Growth/PCGExValencyGrowthOperation.h"

#pragma region FPCGExValencyGrowthOperation

void FPCGExValencyGrowthOperation::Initialize(
	const FPCGExValencyBondingRulesCompiled* InCompiledRules,
	const UPCGExValencySocketRules* InSocketRules,
	FPCGExBoundsTracker& InBoundsTracker,
	FPCGExGrowthBudget& InBudget,
	int32 InSeed)
{
	CompiledRules = InCompiledRules;
	SocketRules = InSocketRules;
	BoundsTracker = &InBoundsTracker;
	Budget = &InBudget;
	RandomStream.Initialize(InSeed);
	DistributionTracker.Initialize(CompiledRules);
}

void FPCGExValencyGrowthOperation::Grow(TArray<FPCGExPlacedModule>& OutPlaced)
{
	if (!CompiledRules || !SocketRules || !BoundsTracker || !Budget) { return; }

	// Build initial frontier from seed modules' sockets
	TArray<FPCGExOpenSocket> Frontier;

	for (int32 PlacedIdx = 0; PlacedIdx < OutPlaced.Num(); ++PlacedIdx)
	{
		const FPCGExPlacedModule& Placed = OutPlaced[PlacedIdx];

		// Dead-end modules don't expand
		if (CompiledRules->ModuleIsDeadEnd[Placed.ModuleIndex]) { continue; }

		// Expand all sockets (seeds have no "used" socket)
		ExpandFrontier(Placed, PlacedIdx, -1, Frontier);
	}

	// Growth loop
	while (Frontier.Num() > 0 && Budget->CanPlaceMore())
	{
		const int32 SelectedIdx = SelectNextSocket(Frontier);
		if (SelectedIdx == INDEX_NONE) { break; }

		const FPCGExOpenSocket Socket = Frontier[SelectedIdx];
		Frontier.RemoveAtSwap(SelectedIdx);

		// Check depth budget
		if (!Budget->CanGrowDeeper(Socket.Depth + 1)) { continue; }

		// Find compatible modules for this socket type
		TArray<int32> CandidateModules;
		TArray<int32> CandidateSocketIndices;
		FindCompatibleModules(Socket.SocketType, CandidateModules, CandidateSocketIndices);

		if (CandidateModules.IsEmpty()) { continue; }

		// Shuffle candidates for variety (Fisher-Yates)
		for (int32 i = CandidateModules.Num() - 1; i > 0; --i)
		{
			const int32 j = RandomStream.RandRange(0, i);
			CandidateModules.Swap(i, j);
			CandidateSocketIndices.Swap(i, j);
		}

		// Try candidates in weighted-random order
		bool bPlaced = false;
		TArray<int32> WeightedOrder;
		WeightedOrder.Reserve(CandidateModules.Num());
		for (int32 i = 0; i < CandidateModules.Num(); ++i) { WeightedOrder.Add(i); }

		// Sort by weight (descending) with randomization
		WeightedOrder.Sort([&](int32 A, int32 B)
		{
			const float WeightA = CompiledRules->ModuleWeights[CandidateModules[A]];
			const float WeightB = CompiledRules->ModuleWeights[CandidateModules[B]];
			// Add jitter for randomization
			return (WeightA + RandomStream.FRand() * 0.1f) > (WeightB + RandomStream.FRand() * 0.1f);
		});

		for (int32 OrderIdx : WeightedOrder)
		{
			const int32 ModuleIdx = CandidateModules[OrderIdx];
			const int32 SocketIdx = CandidateSocketIndices[OrderIdx];

			// Check weight budget
			if (!Budget->CanAfford(Socket.CumulativeWeight, CompiledRules->ModuleWeights[ModuleIdx])) { continue; }

			// Check distribution constraints
			if (!DistributionTracker.CanSpawn(ModuleIdx)) { continue; }

			if (TryPlaceModule(Socket, ModuleIdx, SocketIdx, OutPlaced, Frontier))
			{
				bPlaced = true;
				break;
			}
		}

		if (!bPlaced && Budget->bStopOnFirstFailure)
		{
			// Remove all frontier sockets from the same seed branch
			// (simple approach: just stop the whole growth)
			break;
		}
	}
}

void FPCGExValencyGrowthOperation::FindCompatibleModules(
	FName SocketType,
	TArray<int32>& OutModuleIndices,
	TArray<int32>& OutSocketIndices) const
{
	if (!CompiledRules || !SocketRules) { return; }

	// Find the socket type index in the rules
	const int32 SourceTypeIndex = SocketRules->FindSocketTypeIndex(SocketType);
	if (SourceTypeIndex == INDEX_NONE) { return; }

	// Get the compatibility mask for this socket type
	const int64 CompatMask = SocketRules->GetCompatibilityMask(SourceTypeIndex);

	// Scan all modules for compatible sockets
	for (int32 ModuleIdx = 0; ModuleIdx < CompiledRules->ModuleCount; ++ModuleIdx)
	{
		const TConstArrayView<FPCGExValencyModuleSocket> Sockets = CompiledRules->GetModuleSockets(ModuleIdx);

		for (int32 SocketIdx = 0; SocketIdx < Sockets.Num(); ++SocketIdx)
		{
			const FPCGExValencyModuleSocket& ModuleSocket = Sockets[SocketIdx];

			// Find this socket's type index
			const int32 TargetTypeIndex = SocketRules->FindSocketTypeIndex(ModuleSocket.SocketType);
			if (TargetTypeIndex == INDEX_NONE) { continue; }

			// Check compatibility via bitmask
			if ((CompatMask & (1LL << TargetTypeIndex)) != 0)
			{
				OutModuleIndices.Add(ModuleIdx);
				OutSocketIndices.Add(SocketIdx);
			}
		}
	}
}

FTransform FPCGExValencyGrowthOperation::ComputeAttachmentTransform(
	const FPCGExOpenSocket& ParentSocket,
	int32 ChildModuleIndex,
	int32 ChildSocketIndex) const
{
	// Get child socket's effective offset (local space)
	const TConstArrayView<FPCGExValencyModuleSocket> ChildSockets = CompiledRules->GetModuleSockets(ChildModuleIndex);
	check(ChildSockets.IsValidIndex(ChildSocketIndex));
	const FPCGExValencyModuleSocket& ChildSocket = ChildSockets[ChildSocketIndex];
	const FTransform ChildSocketLocal = ChildSocket.GetEffectiveOffset(SocketRules);

	// Socket attachment: T_B = T_ParentSocket * Rotate180_X * Inverse(S_B[j])
	// ParentSocket.WorldTransform already includes parent module transform * parent socket offset

	// 180-degree rotation around local X axis (sockets face each other)
	const FQuat FlipRotation(FVector::XAxisVector, PI);
	const FTransform FlipTransform(FlipRotation);

	// Inverse of child socket's local offset
	const FTransform ChildSocketInverse = ChildSocketLocal.Inverse();

	// Compose: ParentSocketWorld * Flip * InverseChildSocket
	return ChildSocketInverse * FlipTransform * ParentSocket.WorldTransform;
}

FBox FPCGExValencyGrowthOperation::ComputeWorldBounds(int32 ModuleIndex, const FTransform& WorldTransform) const
{
	FBox LocalBounds = ModuleLocalBounds[ModuleIndex];

	// Apply bounds modifier
	const FPCGExBoundsModifier& Modifier = CompiledRules->ModuleBoundsModifiers[ModuleIndex];
	LocalBounds = Modifier.Apply(LocalBounds);

	return LocalBounds.TransformBy(WorldTransform);
}

bool FPCGExValencyGrowthOperation::TryPlaceModule(
	const FPCGExOpenSocket& Socket,
	int32 ModuleIndex,
	int32 ChildSocketIndex,
	TArray<FPCGExPlacedModule>& OutPlaced,
	TArray<FPCGExOpenSocket>& OutFrontier)
{
	// Compute attachment transform
	const FTransform WorldTransform = ComputeAttachmentTransform(Socket, ModuleIndex, ChildSocketIndex);

	// Compute world bounds
	const FBox WorldBounds = ComputeWorldBounds(ModuleIndex, WorldTransform);

	// Check overlap (skip for degenerate bounds)
	if (WorldBounds.IsValid && BoundsTracker->Overlaps(WorldBounds))
	{
		return false;
	}

	// Place the module
	FPCGExPlacedModule NewModule;
	NewModule.ModuleIndex = ModuleIndex;
	NewModule.WorldTransform = WorldTransform;
	NewModule.WorldBounds = WorldBounds;
	NewModule.ParentIndex = Socket.PlacedModuleIndex;
	NewModule.ParentSocketIndex = Socket.SocketIndex;
	NewModule.ChildSocketIndex = ChildSocketIndex;
	NewModule.Depth = Socket.Depth + 1;
	NewModule.SeedIndex = OutPlaced.IsValidIndex(Socket.PlacedModuleIndex) ? OutPlaced[Socket.PlacedModuleIndex].SeedIndex : 0;
	NewModule.CumulativeWeight = Socket.CumulativeWeight + CompiledRules->ModuleWeights[ModuleIndex];

	const int32 NewIndex = OutPlaced.Num();
	OutPlaced.Add(NewModule);

	// Track bounds and distribution
	if (WorldBounds.IsValid)
	{
		BoundsTracker->Add(WorldBounds);
	}
	Budget->CurrentTotal++;
	DistributionTracker.RecordSpawn(ModuleIndex, CompiledRules);

	// Expand frontier (unless dead-end)
	if (!CompiledRules->ModuleIsDeadEnd[ModuleIndex])
	{
		ExpandFrontier(NewModule, NewIndex, ChildSocketIndex, OutFrontier);
	}

	return true;
}

void FPCGExValencyGrowthOperation::ExpandFrontier(
	const FPCGExPlacedModule& Placed,
	int32 PlacedIndex,
	int32 UsedSocketIndex,
	TArray<FPCGExOpenSocket>& OutFrontier)
{
	const TConstArrayView<FPCGExValencyModuleSocket> Sockets = CompiledRules->GetModuleSockets(Placed.ModuleIndex);

	for (int32 SocketIdx = 0; SocketIdx < Sockets.Num(); ++SocketIdx)
	{
		// Skip the socket that was used for attachment
		if (SocketIdx == UsedSocketIndex) { continue; }

		const FPCGExValencyModuleSocket& ModuleSocket = Sockets[SocketIdx];

		// Compute world-space socket transform
		const FTransform SocketLocal = ModuleSocket.GetEffectiveOffset(SocketRules);
		const FTransform SocketWorld = SocketLocal * Placed.WorldTransform;

		FPCGExOpenSocket OpenSocket;
		OpenSocket.PlacedModuleIndex = PlacedIndex;
		OpenSocket.SocketIndex = SocketIdx;
		OpenSocket.SocketType = ModuleSocket.SocketType;
		OpenSocket.WorldTransform = SocketWorld;
		OpenSocket.Depth = Placed.Depth;
		OpenSocket.CumulativeWeight = Placed.CumulativeWeight;

		OutFrontier.Add(OpenSocket);
	}
}

int32 FPCGExValencyGrowthOperation::SelectWeightedRandom(const TArray<int32>& CandidateModules)
{
	if (CandidateModules.IsEmpty()) { return INDEX_NONE; }
	if (CandidateModules.Num() == 1) { return 0; }

	float TotalWeight = 0.0f;
	for (int32 ModuleIdx : CandidateModules)
	{
		TotalWeight += CompiledRules->ModuleWeights[ModuleIdx];
	}

	if (TotalWeight <= 0.0f) { return 0; }

	float Pick = RandomStream.FRand() * TotalWeight;
	for (int32 i = 0; i < CandidateModules.Num(); ++i)
	{
		Pick -= CompiledRules->ModuleWeights[CandidateModules[i]];
		if (Pick <= 0.0f) { return i; }
	}

	return CandidateModules.Num() - 1;
}

#pragma endregion

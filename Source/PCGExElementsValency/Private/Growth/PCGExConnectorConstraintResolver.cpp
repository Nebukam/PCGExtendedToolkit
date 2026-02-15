// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Growth/PCGExConnectorConstraintResolver.h"

#include "Core/PCGExValencyConnectorSet.h"

#pragma region FPCGExConstraintResolver

void FPCGExConstraintResolver::Resolve(
	const FPCGExConstraintContext& Context,
	TConstArrayView<FInstancedStruct> Constraints,
	FRandomStream& Random,
	TArray<FTransform>& OutCandidates) const
{
	// 1. Collect constraints by role
	TArray<const FPCGExConnectorConstraint*, TInlineAllocator<4>> Generators;
	TArray<const FPCGExConnectorConstraint*, TInlineAllocator<4>> Modifiers;
	TArray<const FPCGExConnectorConstraint*, TInlineAllocator<4>> Filters;

	for (const FInstancedStruct& Instance : Constraints)
	{
		const FPCGExConnectorConstraint* Constraint = Instance.GetPtr<FPCGExConnectorConstraint>();
		if (!Constraint || !Constraint->bEnabled) { continue; }

		switch (Constraint->GetRole())
		{
		case EPCGExConstraintRole::Generator: Generators.Add(Constraint); break;
		case EPCGExConstraintRole::Modifier:  Modifiers.Add(Constraint);  break;
		case EPCGExConstraintRole::Filter:    Filters.Add(Constraint);    break;
		}
	}

	// 2. Generate variant pool
	if (Generators.IsEmpty())
	{
		// No generators: pool is just the base transform
		OutCandidates.Add(Context.BaseAttachment);
	}
	else
	{
		// First generator seeds the pool
		Generators[0]->GenerateVariants(Context, Random, OutCandidates);

		// Subsequent generators cross-product with existing pool
		for (int32 i = 1; i < Generators.Num(); ++i)
		{
			TArray<FTransform> Expanded;
			Expanded.Reserve(OutCandidates.Num() * Generators[i]->GetMaxVariants());

			for (const FTransform& Existing : OutCandidates)
			{
				FPCGExConstraintContext SubContext = Context;
				SubContext.BaseAttachment = Existing;
				Generators[i]->GenerateVariants(SubContext, Random, Expanded);
			}

			OutCandidates = MoveTemp(Expanded);
		}

		// Cap at MaxCandidates via uniform random sampling
		while (OutCandidates.Num() > MaxCandidates)
		{
			OutCandidates.RemoveAtSwap(Random.RandRange(0, OutCandidates.Num() - 1));
		}
	}

	// 3. Apply modifiers sequentially to each variant
	for (const FPCGExConnectorConstraint* Modifier : Modifiers)
	{
		for (FTransform& Variant : OutCandidates)
		{
			Modifier->ApplyModification(Context, Variant, Random);
		}
	}

	// 4. Filter pass (AND logic: all filters must pass)
	for (int32 i = OutCandidates.Num() - 1; i >= 0; --i)
	{
		for (const FPCGExConnectorConstraint* Filter : Filters)
		{
			if (!Filter->IsValid(Context, OutCandidates[i]))
			{
				OutCandidates.RemoveAtSwap(i);
				break;
			}
		}
	}
}

void FPCGExConstraintResolver::MergeConstraints(
	TConstArrayView<FInstancedStruct> ParentConstraints,
	TConstArrayView<FInstancedStruct> ChildConstraints,
	TArray<FInstancedStruct>& OutMerged)
{
	// Start with parent constraints (they take precedence)
	TSet<const UScriptStruct*> ParentTypes;

	for (const FInstancedStruct& Instance : ParentConstraints)
	{
		OutMerged.Add(Instance);
		if (Instance.GetScriptStruct())
		{
			ParentTypes.Add(Instance.GetScriptStruct());
		}
	}

	// Add child constraints whose type isn't already represented by parent
	for (const FInstancedStruct& Instance : ChildConstraints)
	{
		if (!Instance.GetScriptStruct() || !ParentTypes.Contains(Instance.GetScriptStruct()))
		{
			OutMerged.Add(Instance);
		}
	}
}

#pragma endregion

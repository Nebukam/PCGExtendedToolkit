// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"

struct FPCGExOpenConnector;
struct FPCGExConnectorConstraint;
class UPCGExValencyConnectorSet;
enum class EPCGExConstraintRole : uint8;

/**
 * Context passed to constraint evaluation methods.
 * Contains all the information a constraint needs to generate/modify/filter transforms.
 */
struct PCGEXELEMENTSVALENCY_API FPCGExConstraintContext
{
	/** Parent connector's world-space transform */
	FTransform ParentConnectorWorld = FTransform::Identity;

	/** Computed base child placement (from ComputeAttachmentTransform) */
	FTransform BaseAttachment = FTransform::Identity;

	/** Child's local connector offset */
	FTransform ChildConnectorLocal = FTransform::Identity;

	/** Full frontier entry for the open connector */
	const FPCGExOpenConnector* OpenConnector = nullptr;

	/** Index of the child module being placed */
	int32 ChildModuleIndex = -1;

	/** Index of the child's connector being used for attachment */
	int32 ChildConnectorIndex = -1;
};

/**
 * Runs the constraint pipeline: generate -> modify -> filter.
 * Produces candidate transforms for module placement.
 */
struct PCGEXELEMENTSVALENCY_API FPCGExConstraintResolver
{
	/** Maximum candidate transforms per evaluation (caps generator cross-product) */
	int32 MaxCandidates = 16;

	/**
	 * Run the full constraint pipeline.
	 * @param Context Evaluation context (parent/child transforms, connector info)
	 * @param Constraints Array of FInstancedStruct containing FPCGExConnectorConstraint subclasses
	 * @param Random Seeded random stream for deterministic evaluation
	 * @param OutCandidates Output candidate transforms (first = preferred)
	 */
	void Resolve(
		const FPCGExConstraintContext& Context,
		TConstArrayView<FInstancedStruct> Constraints,
		FRandomStream& Random,
		TArray<FTransform>& OutCandidates) const;

	/**
	 * Merge parent + child constraints. Parent wins on type collision.
	 * @param ParentConstraints Constraints from the parent connector
	 * @param ChildConstraints Constraints from the child connector
	 * @param OutMerged Output merged constraint list
	 */
	static void MergeConstraints(
		TConstArrayView<FInstancedStruct> ParentConstraints,
		TConstArrayView<FInstancedStruct> ChildConstraints,
		TArray<FInstancedStruct>& OutMerged);
};

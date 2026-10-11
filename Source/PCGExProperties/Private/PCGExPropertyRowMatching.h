// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

struct FPCGExPropertyOverrideEntry;
struct FPCGExPropertyResolved;

namespace PCGExPropertyOverridesSync
{
	/**
	 * The resolved property row RowIndex stands for, matched on the schema's outer identity; null when there is none.
	 * The one rule for "which property is this row": a HeaderId outranks the name only while it links one row to one
	 * property. Implemented in PCGExProperties.cpp.
	 */
	const FPCGExPropertyResolved* FindProperty(TConstArrayView<FPCGExPropertyOverrideEntry> Rows, int32 RowIndex, TConstArrayView<FPCGExPropertyResolved> Resolved);
}

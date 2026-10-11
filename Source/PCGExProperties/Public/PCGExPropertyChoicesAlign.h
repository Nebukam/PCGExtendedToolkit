// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

class UPCGExPropertySchemaAsset;

/**
 * The align lever: snap every enabled row whose value is none of a property's choices onto the target choice (the
 * effective default when it is itself a choice, else the first). Rows already on a choice are untouched; it keeps
 * data on the list, it does not set values. Editor-only; callers own the transaction and the user feedback.
 */
namespace PCGExPropertyChoicesAlign
{
	struct FResult
	{
		int32 RowsAligned = 0;
		int32 HostsTouched = 0;

		FResult& operator+=(const FResult& Other)
		{
			RowsAligned += Other.RowsAligned;
			HostsTouched += Other.HostsTouched;
			return *this;
		}
	};

#if WITH_EDITOR
	/** Host must implement IPCGExPropertyRowHost; the property is matched by HeaderId first, then by name. */
	PCGEXPROPERTIES_API FResult AlignHostRows(UObject* Host, int32 HeaderId, FName PropertyName);

	/** AlignHostRows over every loaded row host whose schema transitively imports Asset. */
	PCGEXPROPERTIES_API FResult AlignImporters(UPCGExPropertySchemaAsset* Asset, int32 HeaderId, FName PropertyName);
#endif
}

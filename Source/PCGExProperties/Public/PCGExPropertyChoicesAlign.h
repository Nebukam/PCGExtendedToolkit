// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

class UPCGExPropertySchemaAsset;

/**
 * The align lever: snap every enabled row whose value is none of a property's choices onto a target choice, which is
 * the default that row falls back to when that default is itself a choice, else the first choice fitting the property.
 * Rows already on a choice are untouched: it keeps data on the list, it does not set values. Editor-only; callers own
 * the transaction and the user feedback.
 */
namespace PCGExPropertyChoicesAlign
{
	struct FResult
	{
		/** Rows written. */
		int32 RowsAligned = 0;

		/** Hosts with at least one row written. */
		int32 HostsTouched = 0;

		/** Hosts whose rows were examined: they resolve the property, and it offers at least one choice. */
		int32 HostsVisited = 0;

		FResult& operator+=(const FResult& Other)
		{
			RowsAligned += Other.RowsAligned;
			HostsTouched += Other.HostsTouched;
			HostsVisited += Other.HostsVisited;
			return *this;
		}
	};

#if WITH_EDITOR
	/**
	 * Align Host's rows of the property its schema resolves as PropertyName, the schema's own import layer first.
	 * With DeclaringAsset set, a host resolving that name to anything but DeclaringAsset's declaration is left alone.
	 * Host must implement IPCGExPropertyRowHost; it is Modify()'d only if a row is written.
	 */
	PCGEXPROPERTIES_API FResult AlignHostRows(UObject* Host, FName PropertyName, const UPCGExPropertySchemaAsset* DeclaringAsset = nullptr);

	/**
	 * AlignHostRows over every loaded host importing Through, for the property DeclaringAsset declares as
	 * PropertyName (Through itself when null; another asset when Through only patches what it imports).
	 * Blueprint templates, transient and play-in-editor objects are not swept.
	 */
	PCGEXPROPERTIES_API FResult AlignImporters(UPCGExPropertySchemaAsset* Through, FName PropertyName, const UPCGExPropertySchemaAsset* DeclaringAsset = nullptr);
#endif
}

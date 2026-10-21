// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGComponent.h"
#include "PCGCrc.h"
#include "PCGManagedResource.h"

namespace PCGExManagedHelpers
{
	/** Collect every managed resource of type T matching CRC + validator, in container order. Nothing is marked. */
	template <typename T>
	void FindManagedResources(UPCGComponent* Component, const FPCGCrc& Crc, TFunctionRef<bool(const T*)> Validator, TArray<T*>& OutResources)
	{
		if (!Component || !Crc.IsValid())
		{
			return;
		}

		Component->ForEachManagedResource(
			[&](UPCGManagedResource* InResource)
			{
				T* Typed = Cast<T>(InResource);
				if (!Typed)
				{
					return;
				}

				if (!Typed->GetCrc().IsValid() || !(Typed->GetCrc() == Crc))
				{
					return;
				}

				if (!Validator(Typed))
				{
					return;
				}

				OutResources.Add(Typed);
			});
	}

	/** Find the first managed resource of type T matching CRC + validator. Mark as reused if found. */
	template <typename T>
	T* TryReuseManagedResource(UPCGComponent* Component, const FPCGCrc& Crc, TFunctionRef<bool(const T*)> Validator)
	{
		TArray<T*> Found;
		FindManagedResources<T>(Component, Crc, Validator, Found);
		if (Found.IsEmpty())
		{
			return nullptr;
		}

		Found[0]->MarkAsReused();
		return Found[0];
	}

	/** Overload without validator (CRC-only match). */
	template <typename T>
	T* TryReuseManagedResource(UPCGComponent* Component, const FPCGCrc& Crc)
	{
		return TryReuseManagedResource<T>(Component, Crc, [](const T*)
		{
			return true;
		});
	}
}

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "PCGExHelpers.h"
#include "PCGExPointIO.h"

#include "PCGExSettingsOverrides.generated.h"

#pragma region OVERRIDES MACROS

#ifndef PCGEX_OVERRIDES_MACROS
#define PCGEX_OVERRIDES_MACROS

#endif
#pragma endregion

USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPerInputOverrideDetails
{
	GENERATED_BODY()

	FPCGExPerInputOverrideDetails()
	{
		// We want to support :
		// - Index mapping
	}

	/** Is forwarding enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayPriority=0))
	bool bEnabled = false;
};

namespace PCGExData
{
	class FSettingsOverrides : public TSharedFromThis<FSettingsOverrides>
	{
	protected:
		TWeakPtr<FPointIO> BoundData;

	public:
		explicit FSettingsOverrides(TSharedPtr<FPointIO> InBoundData);
		virtual ~FSettingsOverrides() = default;
	};
}

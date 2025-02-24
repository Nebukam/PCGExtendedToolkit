// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "PCGEx.h"
#include "PCGExPicker.generated.h"

namespace PCGExPicker
{
	struct FPickerSample;
}

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExPickerConfigBase
{
	GENERATED_BODY()

	explicit FPCGExPickerConfigBase()
	{
	}

	virtual ~FPCGExPickerConfigBase()
	{
	}

	/** Whether to treat values as discrete indices or relative ones */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bTreatAsNormalized = false;

	/** How to truncate relative picks */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="bTreatAsNormalized", EditConditionHides))
	EPCGExTruncateMode TruncateMode = EPCGExTruncateMode::Round;

	/** How to sanitize index pick when they're out-of-bounds */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExIndexSafety Safety = EPCGExIndexSafety::Ignore;

	virtual void Init()
	{
	}
};

namespace PCGExPicker
{
	const FName OutputPickerLabel = TEXT("Picker");
	const FName SourcePickersLabel = TEXT("Pickers");
}

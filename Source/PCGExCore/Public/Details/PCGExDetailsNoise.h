// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGExInputShorthandsDetails.h"
#include "Math/PCGExMathMean.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "PCGExDetailsNoise.generated.h"

#pragma region PCG exposition
// Exposed copy of the otherwise private PCG' spatial noise mode enum
UENUM(BlueprintType)
enum class PCGExSpatialNoiseMode : uint8
{
	/** Your classic perlin noise. */
	Perlin,
	/** Based on underwater fake caustic rendering, gives swirly look. */
	Caustic,
	/** Voronoi noise, result a the distance to edge and cell ID. */
	Voronoi,
	/** Based on fractional brownian motion. */
	FractionalBrownian,
	/** Used to create masks to blend out edges. */
	EdgeMask,
};

UENUM(BlueprintType)
enum class PCGExSpatialNoiseMask2DMode : uint8
{
	/** Your classic perlin noise. */
	Perlin,
	/** Based on underwater fake caustic rendering, gives swirly look. */
	Caustic,
	/** Based on fractional brownian motion. */
	FractionalBrownian,
};

#pragma endregion

struct FPCGExContext;

namespace PCGExData
{
	class FFacade;
}

namespace PCGExDetails
{
	template <typename T>
	class TSettingValue;
}

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExRandomRatioDetails
{
	GENERATED_BODY()

	FPCGExRandomRatioDetails() = default;

	explicit FPCGExRandomRatioDetails(double DefaultAmount)
	{
		Amount_DEPRECATED = DefaultAmount;
	}

	/** Type of seed input */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExInputShorthandSelectorInteger32 BaseSeed = FPCGExInputShorthandSelectorInteger32(FName("@Data.Seed"), 42);

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExMeanMeasure Units = EPCGExMeanMeasure::Relative;

	/** Amount (relative) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Amount (Relative)", EditCondition="Units == EPCGExMeanMeasure::Relative", EditConditionHides))
	FPCGExInputShorthandSelectorDouble01 RelativeAmount = FPCGExInputShorthandSelectorDouble01(FName("@Data.Amount"), 0.5, false);

	/** Amount (fixed) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Amount (Fixed)", EditCondition="Units == EPCGExMeanMeasure::Discrete", EditConditionHides))
	FPCGExInputShorthandSelectorInteger32Abs DiscreteAmount = FPCGExInputShorthandSelectorInteger32Abs(FName("@Data.Amount"), 42, false);

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bDoClampMin = false;

	/** Min Amount (fixed) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bDoClampMin"))
	FPCGExInputShorthandSelectorInteger32Abs ClampMin = FPCGExInputShorthandSelectorInteger32Abs(FName("@Data.ClampMin"), 1, false);

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bDoClampMax = false;

	/** Max Amount (fixed) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bDoClampMax"))
	FPCGExInputShorthandSelectorInteger32Abs ClampMax = FPCGExInputShorthandSelectorInteger32Abs(FName("@Data.ClampMax"), 500, false);

#pragma region DEPRECATED

	UPROPERTY()
	EPCGExInputValueType SeedInput_DEPRECATED = EPCGExInputValueType::Constant;

	UPROPERTY()
	FPCGAttributePropertyInputSelector LocalSeed_DEPRECATED;

	UPROPERTY()
	int32 SeedValue_DEPRECATED = 42;

	UPROPERTY()
	EPCGExInputValueType AmountInput_DEPRECATED = EPCGExInputValueType::Constant;

	UPROPERTY()
	FPCGAttributePropertyInputSelector LocalAmount_DEPRECATED;

	UPROPERTY()
	double Amount_DEPRECATED = 0.5;

	UPROPERTY()
	int32 FixedAmount_DEPRECATED = 1;

#pragma endregion

	int32 GetNumPicks(FPCGExContext* InContext, const UPCGData* InData, const int32 NumMaxItems) const;

	void GetPicks(FPCGExContext* InContext, const UPCGData* InData, const int32 NumMaxItems, TSet<int32>& OutPicks) const;
	void GetPicks(FPCGExContext* InContext, const UPCGData* InData, const int32 NumMaxItems, TArray<int32>& OutPicks) const;

#if WITH_EDITOR
	void ApplyDeprecation();
#endif
};

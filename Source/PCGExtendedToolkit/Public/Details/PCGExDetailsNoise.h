// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCommon.h"
#include "PCGExMathMean.h"
#include "PCGExSettingsMacros.h"
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
struct PCGEXTENDEDTOOLKIT_API FPCGExRandomRatioDetails
{
	GENERATED_BODY()

	FPCGExRandomRatioDetails() = default;

	explicit FPCGExRandomRatioDetails(double DefaultAmount)
	{
		Amount = DefaultAmount;
	}

	/** Type of seed input */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Seed", meta=(PCG_Overridable))
	EPCGExInputValueType SeedInput = EPCGExInputValueType::Constant;

	/** Fetch the seed value from a @Data attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Seed", meta = (PCG_Overridable, DisplayName="Seed (Attr)", EditCondition="SeedInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector LocalSeed;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Seed", meta=(PCG_Overridable, DisplayName="Seed", EditCondition="SeedInput == EPCGExInputValueType::Constant", EditConditionHides))
	int32 SeedValue = 42;


	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExMeanMeasure Units = EPCGExMeanMeasure::Relative;

	/** Type of amount input */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType AmountInput = EPCGExInputValueType::Constant;

	/** Fetch the amount value from a @Data attribute.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Amount (Attr)", EditCondition="AmountInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector LocalAmount;

	/** Ratio relative to maximum number of items. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Amount", EditCondition="AmountInput == EPCGExInputValueType::Constant && Units == EPCGExMeanMeasure::Relative", EditConditionHides, ClampMin=0, ClampMax=1))
	double Amount = 0.5;

	/** Fixed number of items. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Fixed Amount", EditCondition="AmountInput == EPCGExInputValueType::Constant && Units == EPCGExMeanMeasure::Discrete", EditConditionHides, ClampMin=1))
	int32 FixedAmount = 1;

	PCGEX_SETTING_DATA_VALUE_DECL(Seed, int32)
	PCGEX_SETTING_DATA_VALUE_DECL(Amount, double)
	PCGEX_SETTING_DATA_VALUE_DECL(FixedAmount, int32)

	int32 GetNumPicks(FPCGExContext* InContext, const UPCGData* InData, const int32 NumMaxItems) const;

	void GetPicks(FPCGExContext* InContext, const UPCGData* InData, const int32 NumMaxItems, TSet<int32>& OutPicks) const;
	void GetPicks(FPCGExContext* InContext, const UPCGData* InData, const int32 NumMaxItems, TArray<int32>& OutPicks) const;
};

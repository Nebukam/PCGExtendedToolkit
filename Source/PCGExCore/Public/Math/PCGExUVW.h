// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "Engine/EngineTypes.h"

#include "PCGExCommon.h"
#include "Data/PCGExDataCommon.h"
#include "Details/PCGExSettingsMacros.h"
#include "Metadata/PCGAttributePropertySelector.h"

#include "PCGExUVW.generated.h"

struct FPCGExContext;
class UPCGBasePointData;
enum class EPCGExMinimalAxis : uint8;

namespace PCGExData
{
	struct FConstPoint;
	class FFacade;
}

namespace PCGExDetails
{
	template <typename T>
	class TSettingValue;
}

USTRUCT(BlueprintType)
struct PCGEXCORE_API FPCGExUVW
{
	GENERATED_BODY()

	FPCGExUVW()
	{
	}

	explicit FPCGExUVW(const double DefaultW)
		: WConstant(DefaultW)
	{
	}

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExPointBoundsSource BoundsReference = EPCGExPointBoundsSource::ScaledBounds;

	/** U Source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType UInput = EPCGExInputValueType::Constant;

	/** U Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="U (Attr)", EditCondition="UInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector UAttribute;

	/** U Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="U", EditCondition="UInput == EPCGExInputValueType::Constant", EditConditionHides))
	double UConstant = 0;


	/** V Source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType VInput = EPCGExInputValueType::Constant;

	/** V Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="V (Attr)", EditCondition="VInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector VAttribute;

	/** V Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="V", EditCondition="VInput == EPCGExInputValueType::Constant", EditConditionHides))
	double VConstant = 0;


	/** W Source */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType WInput = EPCGExInputValueType::Constant;

	/** W Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="W (Attr)", EditCondition="WInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector WAttribute;

	/** W Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="W", EditCondition="WInput == EPCGExInputValueType::Constant", EditConditionHides))
	double WConstant = 0;

	PCGEX_SETTING_VALUE_DECL(U, double)
	PCGEX_SETTING_VALUE_DECL(V, double)
	PCGEX_SETTING_VALUE_DECL(W, double)

	bool Init(FPCGExContext* InContext, const TSharedRef<PCGExData::FFacade>& InDataFacade);

	// Without axis

	FVector GetUVW(const int32 PointIndex) const;

	FVector GetPosition(const int32 PointIndex) const;

	FVector GetPosition(const int32 PointIndex, FVector& OutOffset) const;

	// With axis

	FVector GetUVW(const int32 PointIndex, const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const;

	FVector GetPosition(const int32 PointIndex, const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const;

	FVector GetPosition(const int32 PointIndex, FVector& OutOffset, const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const;

protected:
	TSharedPtr<PCGExDetails::TSettingValue<double>> UGetter;
	TSharedPtr<PCGExDetails::TSettingValue<double>> VGetter;
	TSharedPtr<PCGExDetails::TSettingValue<double>> WGetter;

	const UPCGBasePointData* PointData = nullptr;
};

namespace PCGExMath
{
	struct PCGEXCORE_API FPCGExConstantUVW
	{
		FPCGExConstantUVW()
		{
		}

		EPCGExPointBoundsSource BoundsReference = EPCGExPointBoundsSource::ScaledBounds;
		double U = 0;
		double V = 0;
		double W = 0;

		FORCEINLINE FVector GetUVW() const { return FVector(U, V, W); }

		FVector GetPosition(const PCGExData::FConstPoint& Point) const;

		FVector GetPosition(const PCGExData::FConstPoint& Point, FVector& OutOffset) const;

		// With axis

		FVector GetUVW(const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const;

		FVector GetPosition(const PCGExData::FConstPoint& Point, const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const;

		FVector GetPosition(const PCGExData::FConstPoint& Point, FVector& OutOffset, const EPCGExMinimalAxis Axis, const bool bMirrorAxis = false) const;
	};
}

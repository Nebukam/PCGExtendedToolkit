// Copyright 2024 Edward Boucher and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "PCGSettings.h"
#include "PCGExConstantsDefinitions.h"
#include "PCGExGlobalSettings.h"
#include "PCGExMacros.h"
#include "PCGExConstants.generated.h"

UENUM(BlueprintType)
enum class EPCGExNumericOutput : uint8
{
	Double,
	Float,
	Int32,
	Int64,
};

#define PCGEX_FOREACH_NUMERIC_OUTPUT(_MACRO)\
_MACRO(Double, double)\
_MACRO(Float, float)\
_MACRO(Int32, int32)\
_MACRO(Int64, int64)

UCLASS(BlueprintType, ClassGroup=(Procedural))
class UPCGExConstantsSettings : public UPCGSettings
{
public:
	GENERATED_BODY()

	bool bCacheResult = true;

#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(Constant, "Constant", "Constants.", GetEnumName());
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorConstant; }

	FName GetEnumName() const;

	virtual bool OnlyExposePreconfiguredSettings() const override { return true; };

#if PCGEX_ENGINE_VERSION > 503
	virtual bool CanUserEditTitle() const override { return false; }
#endif
	virtual TArray<FPCGPreConfiguredSettingsInfo> GetPreconfiguredInfo() const override;

#endif

	virtual void ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo) override;

	// Used by the preconfigured settings to load the right constants
	UPROPERTY(BlueprintReadWrite)
	EPCGExConstantListID ConstantList;

	// Export the negative of the constant instead of the constant itself
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="ConstantList!=EPCGExConstantListID::Booleans", EditConditionHides))
	bool NegateOutput = false;

	// Output 1/x instead of x (e.g. 2 becomes 1/2)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="ConstantList!=EPCGExConstantListID::Booleans && ConstantList != EPCGExConstantListID::Vectors", EditConditionHides))
	bool OutputReciprocal = false;

	// Apply a custom (constant, numeric) multiplier to the output
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="ConstantList!=EPCGExConstantListID::Booleans", EditConditionHides))
	double CustomMultiplier = 1.0;

	// Cast to a specific type (double will be used by default)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="ConstantList != EPCGExConstantListID::Booleans && ConstantList != EPCGExConstantListID::Vectors", EditConditionHides))
	EPCGExNumericOutput NumericOutputType;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return {}; }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	static PCGExConstants::TDescriptorList<double> GetNumericConstantList(EPCGExConstantListID ConstantList)
	{
		return PCGExConstants::Numbers.ExportedConstants[static_cast<uint8>(ConstantList)];
	}

	static PCGExConstants::TDescriptorList<FVector> GetVectorConstantList(EPCGExConstantListID ConstantList)
	{
		return PCGExConstants::Vectors.ExportedConstants[0];
	}
};

class FPCGExConstantsElement : public IPCGElement
{
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

	template <typename T>
	void PostProcessNumericValue(T& InValue, const bool bNegate, const bool bReciprocal, const double Scale) const
	{
		if (bNegate) { InValue *= -1; }

		if constexpr (std::is_floating_point_v<T>)
		{
			if (bReciprocal)
			{
				if (!FMath::IsNearlyZero(InValue)) { InValue = 1 / InValue; }
				else { InValue = 0; }
			}
		}

		InValue *= Scale;
	}

public:
	virtual FPCGContext* Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node) override;
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return true; }
};

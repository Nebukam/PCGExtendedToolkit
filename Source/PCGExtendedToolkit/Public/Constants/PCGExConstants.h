// Copyright 2024 Edward Boucher and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once
#include "PCGSettings.h"
#include "PCGExConstantsDefinitions.h"
#include "PCGExConstants.generated.h"

UENUM(BlueprintType) enum class ENumericOutput : uint8 {
	Double,
	Float,
	Int32,
	Int64,
};

UCLASS(BlueprintType, ClassGroup=(Procedural))
class UPCGExConstantsSettings : public UPCGSettings {
public:
	GENERATED_BODY()

#if WITH_EDITOR
	virtual FName GetDefaultNodeName() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetDefaultNodeTitle() const override;
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Param; }
	virtual bool OnlyExposePreconfiguredSettings() const override { return true; };
	virtual bool CanUserEditTitle() const override { return false; }
	virtual bool HasFlippedTitleLines() const override { return true; }
	virtual TArray<FPCGPreConfiguredSettingsInfo> GetPreconfiguredInfo() const override;
#endif
	
	virtual FString GetAdditionalTitleInformation() const override;
	virtual void ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo) override;

	// Used by the preconfigured settings to load the right constants
	UPROPERTY(BlueprintReadWrite) EConstantListID ConstantList;
	
	// Export the negative of the constant instead of the constant itself
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="ConstantList!=EConstantListID::Booleans", EditConditionHides))
	bool NegateOutput = false;

	// Output 1/x instead of x (e.g. 2 becomes 1/2)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="ConstantList!=EConstantListID::Booleans && ConstantList != EConstantListID::Vectors", EditConditionHides))
	bool OutputReciprocal  = false;

	// Apply a custom (constant, numeric) multiplier to the output
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="ConstantList!=EConstantListID::Booleans", EditConditionHides))
	double CustomMultiplier = 1.0;

	// Cast to a specific type (double will be used by default)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(EditCondition="ConstantList != EConstantListID::Booleans && ConstantList != EConstantListID::Vectors", EditConditionHides)) 
	ENumericOutput NumericOutputType;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override { return {}; }
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;

public:
	static FConstantDescriptorList<double> GetNumericConstantList(EConstantListID ConstantList);
	static FConstantDescriptorList<FVector> GetVectorConstantList(EConstantListID ConstantList);
	
	
};

class FPCGConstantsElement : public IPCGElement {
protected:
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

public:
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return true; }
	
};

﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "Constants/PCGExConstantEnum.h"

#include "PCGExBranchOnDataAttribute.generated.h"

UENUM()
enum class EPCGExControlFlowSelectionMode : uint8
{
	UserDefined = 0,
	EnumInteger = 1,
	EnumName    = 2,
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExBranchOnDataPin
{
	GENERATED_BODY()

	explicit FPCGExBranchOnDataPin(const bool InNumeric = true)
	{
	}

	virtual ~FPCGExBranchOnDataPin()
	{
	}

	/** Name of the output pin */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName Label = FName("None");

	/** How should the data be compared. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	EPCGExComparisonDataType Check = EPCGExComparisonDataType::Numeric;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Comparison", EditCondition="Check == EPCGExComparisonDataType::Numeric", EditConditionHides))
	EPCGExComparison NumericCompare = EPCGExComparison::StrictlyEqual;

	/** Value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Value", EditCondition="Check == EPCGExComparisonDataType::Numeric", EditConditionHides))
	int64 NumericValue = 0;

	/** Rounding mode for near measures */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="Check == EPCGExComparisonDataType::Numeric && NumericCompare == EPCGExComparison::NearlyEqual || NumericCompare == EPCGExComparison::NearlyNotEqual", EditConditionHides))
	double Tolerance = DBL_COMPARE_TOLERANCE;

	/** Comparison */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Comparison", EditCondition="Check == EPCGExComparisonDataType::String", EditConditionHides))
	EPCGExStringComparison StringCompare = EPCGExStringComparison::StrictlyEqual;

	/** Value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Value", EditCondition="Check == EPCGExComparisonDataType::String", EditConditionHides))
	FString StringValue = TEXT("");


	virtual void Init()
	{
	}
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(PCGExNodeLibraryDoc="quality-of-life/branch-on-data"))
class UPCGExBranchOnDataAttributeSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject interface

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		BranchOnDataAttribute, "Branch on Data", "Branch on @Data domain attribute.",
		BranchSource);
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::ControlFlow; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->ColorFilterHub); }
#endif

	virtual bool HasDynamicPins() const override { return true; }
	virtual bool OutputPinsCanBeDeactivated() const override { return false; }
	virtual FName GetMainInputPin() const override { return PCGPinConstants::DefaultInputLabel; }
	virtual FName GetMainOutputPin() const override { return DefaultPinName; }

protected:
	virtual bool IsInputless() const override { return true; }
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** The @Data domain attribute to check */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FName BranchSource = FName("@Data.Branch");

	/** Determines the type of value to be used to select an output. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings)
	EPCGExControlFlowSelectionMode SelectionMode = EPCGExControlFlowSelectionMode::UserDefined;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="SelectionMode == EPCGExControlFlowSelectionMode::UserDefined", EditConditionHides))
	TArray<FPCGExBranchOnDataPin> Branches;

	UPROPERTY(meta = (PCG_NotOverridable))
	TArray<FPCGExBranchOnDataPin> InternalBranches;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="Settings", meta=(PCG_NotOverridable, EditCondition="SelectionMode != EPCGExControlFlowSelectionMode::UserDefined", EditConditionHides))
	EPCGExEnumConstantSourceType EnumSource = EPCGExEnumConstantSourceType::Selector;

	/** Determines which Enum be used. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(PCG_NotOverridable, EditCondition="SelectionMode != EPCGExControlFlowSelectionMode::UserDefined && EnumSource == EPCGExEnumConstantSourceType::Picker", EditConditionHides))
	TObjectPtr<UEnum> EnumClass;

	/** Determines which Enum be used. Enum selection is ignored here, it's only using the class value internally. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, meta=(PCG_NotOverridable, EditCondition="SelectionMode != EPCGExControlFlowSelectionMode::UserDefined && EnumSource == EPCGExEnumConstantSourceType::Selector", EditConditionHides, ShowOnlyInnerProperties))
	FEnumSelector EnumPicker;

	/** Name of the default/fallback output pin. This is exposed because to allow easy disambiguation when 'default' is a valid switch. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Settings, AdvancedDisplay)
	FName DefaultPinName = FName("Default");

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Warnings and Errors")
	bool bQuietMissingAttribute = false;

	TObjectPtr<UEnum> GetEnumClass() const;
};

struct FPCGExBranchOnDataAttributeContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExBranchOnDataAttributeElement;
};

class FPCGExBranchOnDataAttributeElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(BranchOnDataAttribute)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExFillControlOperation.h"
#include "PCGExFillControlsFactoryProvider.h"
#include "Clusters/PCGExClusterCommon.h"
#include "Details/PCGExSettingsMacros.h"

#include "PCGExFillControlAttributeAccumulation.generated.h"

UENUM(BlueprintType)
enum class EPCGExAccumulationMode : uint8
{
	Sum     UMETA(DisplayName = "Sum", ToolTip = "Add attribute values along the path"),
	Max     UMETA(DisplayName = "Maximum", ToolTip = "Track maximum value encountered"),
	Min     UMETA(DisplayName = "Minimum", ToolTip = "Track minimum value encountered"),
	Average UMETA(DisplayName = "Average", ToolTip = "Running average along path"),
};

USTRUCT(BlueprintType)
struct FPCGExFillControlConfigAttributeAccumulation : public FPCGExFillControlConfigBase
{
	GENERATED_BODY()

	FPCGExFillControlConfigAttributeAccumulation() = default;

	/** Attribute to accumulate. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector Attribute;

	/** Read attribute from vertex or edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExClusterElement AttributeSource = EPCGExClusterElement::Vtx;

	/** Accumulation mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExAccumulationMode Mode = EPCGExAccumulationMode::Sum;

	/** Maximum accumulated value input type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExInputValueType MaxAccumulationInput = EPCGExInputValueType::Constant;

	/** Max accumulation attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName = "Max Accumulation (Attr)", EditCondition = "MaxAccumulationInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName MaxAccumulationAttribute = FName("MaxAccumulation");

	/** Max accumulation constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName = "Max Accumulation", EditCondition = "MaxAccumulationInput == EPCGExInputValueType::Constant", EditConditionHides))
	double MaxAccumulation = 100.0;

	PCGEX_SETTING_VALUE_DECL(MaxAccumulation, double)

	/** Store accumulated value in FCandidate::AccumulatedValue for use by other controls. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, AdvancedDisplay, meta = (PCG_NotOverridable))
	bool bWriteToAccumulatedValue = true;
};

/**
 * Accumulation control that tracks an attribute value along the path and stops when threshold exceeded.
 * Uses FCandidate::AccumulatedValue to store the running total.
 */
class FPCGExFillControlAttributeAccumulation : public FPCGExFillControlOperation
{
	friend class UPCGExFillControlsFactoryAttributeAccumulation;

public:
	virtual bool DoesScoring() const override { return true; }
	virtual bool ChecksCandidate() const override { return true; }

	virtual bool PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler) override;
	virtual void ScoreCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, PCGExFloodFill::FCandidate& OutCandidate) override;
	virtual bool IsValidCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, const PCGExFloodFill::FCandidate& Candidate) override;

protected:
	TSharedPtr<PCGExData::TBuffer<double>> AttributeBuffer;
	TSharedPtr<PCGExDetails::TSettingValue<double>> MaxAccumulation;
	EPCGExClusterElement AttributeSource = EPCGExClusterElement::Vtx;
	EPCGExAccumulationMode Mode = EPCGExAccumulationMode::Sum;
	bool bWriteToAccumulatedValue = true;

	double ComputeAccumulation(double PreviousAccumulated, double NewValue, int32 Depth) const;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExFillControlsFactoryAttributeAccumulation : public UPCGExFillControlsFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExFillControlConfigAttributeAccumulation Config;

	virtual TSharedPtr<FPCGExFillControlOperation> CreateOperation(FPCGExContext* InContext) const override;

	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params", meta=(PCGExNodeLibraryDoc="clusters/flood-fill/fc-attribute-accumulation"))
class UPCGExFillControlsAttributeAccumulationProviderSettings : public UPCGExFillControlsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(FillControlsAttributeAccumulation, "Fill Control : Attribute Accumulation", "Track accumulated attribute value along path, stop when threshold exceeded.", FName(GetDisplayName()))
#endif
	//~End UPCGSettings

public:
	/** Control Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExFillControlConfigAttributeAccumulation Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

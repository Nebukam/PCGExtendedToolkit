// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Core/PCGExFillControlOperation.h"
#include "Core/PCGExFillControlsFactoryProvider.h"
#include "Clusters/PCGExClusterCommon.h"
#include "Utils/PCGExCompare.h"
#include "Details/PCGExSettingsMacros.h"

#include "PCGExFillControlAttributeThreshold.generated.h"

USTRUCT(BlueprintType)
struct FPCGExFillControlConfigAttributeThreshold : public FPCGExFillControlConfigBase
{
	GENERATED_BODY()

	FPCGExFillControlConfigAttributeThreshold() = default;

	/** Attribute to check. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGAttributePropertyInputSelector Attribute;

	/** Read attribute from vertex or edge. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExClusterElement AttributeSource = EPCGExClusterElement::Vtx;

	/** Threshold input type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExInputValueType ThresholdInput = EPCGExInputValueType::Constant;

	/** Threshold attribute. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName = "Threshold (Attr)", EditCondition = "ThresholdInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName ThresholdAttribute = FName("Threshold");

	/** Threshold constant. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName = "Threshold", EditCondition = "ThresholdInput == EPCGExInputValueType::Constant", EditConditionHides))
	double Threshold = 0.5;

	PCGEX_SETTING_VALUE_DECL(Threshold, double)

	/** Comparison operator. Candidate is valid if: AttributeValue [Comparison] Threshold */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExComparison Comparison = EPCGExComparison::StrictlySmaller;
};

/**
 * Threshold control that stops diffusion when vertex/edge attribute crosses a threshold.
 */
class FPCGExFillControlAttributeThreshold : public FPCGExFillControlOperation
{
	friend class UPCGExFillControlsFactoryAttributeThreshold;

public:
	virtual bool PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler) override;

	virtual bool IsValidCapture(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate) override;
	virtual bool IsValidProbe(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate) override;
	virtual bool IsValidCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, const PCGExFloodFill::FCandidate& Candidate) override;

protected:
	TSharedPtr<PCGExData::TBuffer<double>> AttributeBuffer;
	TSharedPtr<PCGExDetails::TSettingValue<double>> Threshold;
	EPCGExClusterElement AttributeSource = EPCGExClusterElement::Vtx;
	EPCGExComparison Comparison = EPCGExComparison::StrictlySmaller;

	bool TestCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate) const;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExFillControlsFactoryAttributeThreshold : public UPCGExFillControlsFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExFillControlConfigAttributeThreshold Config;

	virtual TSharedPtr<FPCGExFillControlOperation> CreateOperation(FPCGExContext* InContext) const override;

	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params", meta=(PCGExNodeLibraryDoc="clusters/flood-fill/fc-attribute-threshold"))
class UPCGExFillControlsAttributeThresholdProviderSettings : public UPCGExFillControlsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(FillControlsAttributeThreshold, "Fill Control : Attribute Threshold", "Stop diffusion when vertex/edge attribute crosses a threshold.", FName(GetDisplayName()))
#endif
	//~End UPCGSettings

public:
	/** Control Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExFillControlConfigAttributeThreshold Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

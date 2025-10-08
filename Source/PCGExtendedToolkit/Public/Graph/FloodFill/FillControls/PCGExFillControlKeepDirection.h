﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExCompare.h"
#include "UObject/Object.h"
#include "PCGExFillControlOperation.h"
#include "PCGExFillControlsFactoryProvider.h"
#include "Details/PCGExSettingsMacros.h"

#include "PCGExFillControlKeepDirection.generated.h"

USTRUCT(BlueprintType)
struct FPCGExFillControlConfigKeepDirection : public FPCGExFillControlConfigBase
{
	GENERATED_BODY()

	FPCGExFillControlConfigKeepDirection() :
		FPCGExFillControlConfigBase()
	{
		bSupportSteps = false;
	}

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType WindowSizeInput = EPCGExInputValueType::Constant;

	/** Window Size Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Window Size (Attr)", EditCondition="WindowSizeInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector WindowSizeAttribute;

	/** Window Size Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Window Size", EditCondition="WindowSizeInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	int32 WindowSize = 1;

	PCGEX_SETTING_VALUE_DECL(WindowSize, int32)

	/** Hash comparison settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExVectorHashComparisonDetails HashComparisonDetails = FPCGExVectorHashComparisonDetails(0.1);
};

/**
 * 
 */
class FPCGExFillControlKeepDirection : public FPCGExFillControlOperation
{
	friend class UPCGExFillControlsFactoryKeepDirection;

public:
	virtual bool PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler) override;

	virtual bool ChecksCapture() const override { return false; }
	virtual bool ChecksProbe() const override { return false; }
	virtual bool ChecksCandidate() const override { return true; }
	virtual bool IsValidCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, const PCGExFloodFill::FCandidate& Candidate) override;

protected:
	FPCGExVectorHashComparisonDetails HashComparisonDetails;
	TSharedPtr<PCGExDetails::TSettingValue<int32>> WindowSize;
	TSharedPtr<PCGExDetails::TSettingValue<double>> DistanceLimit;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExFillControlsFactoryKeepDirection : public UPCGExFillControlsFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExFillControlConfigKeepDirection Config;

	virtual TSharedPtr<FPCGExFillControlOperation> CreateOperation(FPCGExContext* InContext) const override;

	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params", meta=(PCGExNodeLibraryDoc="clusters/flood-fill/fc-keep-direction"))
class UPCGExFillControlsKeepDirectionProviderSettings : public UPCGExFillControlsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(
		FillControlsKeepDirection, "Fill Control : Keep Direction", "Stop fill after a certain number of vtx have been captured.",
		FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	/** Control Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExFillControlConfigKeepDirection Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

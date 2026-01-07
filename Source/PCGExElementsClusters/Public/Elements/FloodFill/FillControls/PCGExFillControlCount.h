// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExFillControlOperation.h"
#include "PCGExFillControlsFactoryProvider.h"
#include "Details/PCGExSettingsMacros.h"

#include "PCGExFillControlCount.generated.h"

USTRUCT(BlueprintType)
struct FPCGExFillControlConfigCount : public FPCGExFillControlConfigBase
{
	GENERATED_BODY()

	FPCGExFillControlConfigCount() = default;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType MaxCountInput = EPCGExInputValueType::Constant;

	/** Max Count Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Max Count (Attr)", EditCondition="MaxCountInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName MaxCountAttribute = FName("MaxCount");

	/** Max Count Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Max Count", EditCondition="MaxCountInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	int32 MaxCount = 10;

	PCGEX_SETTING_VALUE_DECL(MaxCount, int32)
};

/**
 * 
 */
class FPCGExFillControlCount : public FPCGExFillControlOperation
{
	friend class UPCGExFillControlsFactoryCount;

public:
	virtual bool PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler) override;

	virtual bool ChecksCapture() const override { return true; }
	virtual bool IsValidCapture(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate) override;

	virtual bool ChecksProbe() const override { return false; }
	virtual bool ChecksCandidate() const override { return false; }

protected:
	TSharedPtr<PCGExDetails::TSettingValue<int32>> CountLimit;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExFillControlsFactoryCount : public UPCGExFillControlsFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExFillControlConfigCount Config;

	virtual TSharedPtr<FPCGExFillControlOperation> CreateOperation(FPCGExContext* InContext) const override;

	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params", meta=(PCGExNodeLibraryDoc="clusters/flood-fill/fc-count"))
class UPCGExFillControlsCountProviderSettings : public UPCGExFillControlsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(FillControlsCount, "Fill Control : Count", "Stop fill after a certain number of vtx have been captured.", FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	/** Control Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExFillControlConfigCount Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

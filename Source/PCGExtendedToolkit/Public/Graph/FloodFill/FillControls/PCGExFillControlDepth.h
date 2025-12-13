// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExFillControlOperation.h"
#include "PCGExFillControlsFactoryProvider.h"
#include "Details/PCGExSettingsMacros.h"

#include "PCGExFillControlDepth.generated.h"

USTRUCT(BlueprintType)
struct FPCGExFillControlConfigDepth : public FPCGExFillControlConfigBase
{
	GENERATED_BODY()

	FPCGExFillControlConfigDepth() = default;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType MaxDepthInput = EPCGExInputValueType::Constant;

	/** Max depth Attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Max Depth (Attr)", EditCondition="MaxDepthInput != EPCGExInputValueType::Constant", EditConditionHides))
	FName MaxDepthAttribute = FName("MaxDepth");

	/** Max depth Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Max Depth", EditCondition="MaxDepthInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=1))
	int32 MaxDepth = 10;

	PCGEX_SETTING_VALUE_DECL(MaxDepth, int32)
};

/**
 * 
 */
class FPCGExFillControlDepth : public FPCGExFillControlOperation
{
	friend class UPCGExFillControlsFactoryDepth;

public:
	virtual bool PrepareForDiffusions(FPCGExContext* InContext, const TSharedPtr<PCGExFloodFill::FFillControlsHandler>& InHandler) override;

	virtual bool IsValidCapture(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate) override;
	virtual bool IsValidProbe(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& Candidate) override;
	virtual bool IsValidCandidate(const PCGExFloodFill::FDiffusion* Diffusion, const PCGExFloodFill::FCandidate& From, const PCGExFloodFill::FCandidate& Candidate) override;

protected:
	TSharedPtr<PCGExDetails::TSettingValue<int32>> DepthLimit;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class UPCGExFillControlsFactoryDepth : public UPCGExFillControlsFactoryData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FPCGExFillControlConfigDepth Config;

	virtual TSharedPtr<FPCGExFillControlOperation> CreateOperation(FPCGExContext* InContext) const override;

	virtual void RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params", meta=(PCGExNodeLibraryDoc="clusters/flood-fill/fc-depth"))
class UPCGExFillControlsDepthProviderSettings : public UPCGExFillControlsFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS_CUSTOM_SUBTITLE(FillControlsDepth, "Fill Control : Depth", "Control fill based on diffusion depth.", FName(GetDisplayName()))
#endif
	//~End UPCGSettings

	/** Control Config.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExFillControlConfigDepth Config;

	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;

#if WITH_EDITOR
	virtual FString GetDisplayName() const override;
#endif
};

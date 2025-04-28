// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Graph/FloodFill/PCGExFloodFill.h"

#include "PCGExFillControlsFactoryProvider.generated.h"

#define PCGEX_FORWARD_FILLCONTROL_FACTORY \
	NewFactory->Config = Config; \
	NewFactory->Config.Init(); \
	NewFactory->ConfigBase = NewFactory->Config;

#define PCGEX_FORWARD_FILLCONTROL_OPERATION \
	NewOperation->Factory = this;

class UPCGExFillControlOperation;

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExFillControlConfigBase
{
	GENERATED_BODY()

	FPCGExFillControlConfigBase()
	{
	}

	~FPCGExFillControlConfigBase()
	{
	}

	/** Where to fetch the attribute from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_NotOverridable))
	EPCGExFloodFillSettingSource Source = EPCGExFloodFillSettingSource::Seed;

	/** At which diffusion step should this control be applied. Note that this may not be supported by all controls. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable, Bitmask, BitmaskEnum="/Script/PCGExtendedToolkit.EPCGExFloodFillControlStepsFlags"))
	uint8 Steps = static_cast<uint8>(EPCGExFloodFillControlStepsFlags::Candidate);

	void Init();
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Data")
class PCGEXTENDEDTOOLKIT_API UPCGExFillControlsFactoryData : public UPCGExFactoryData
{
	GENERATED_BODY()

public:
	FPCGExFillControlConfigBase ConfigBase;

	virtual PCGExFactories::EType GetFactoryType() const override { return PCGExFactories::EType::FillControls; }

	virtual bool RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const override;

	virtual UPCGExFillControlOperation* CreateOperation(FPCGExContext* InContext) const;
};

UCLASS(Abstract, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Graph|Params")
class PCGEXTENDEDTOOLKIT_API UPCGExFillControlsFactoryProviderSettings : public UPCGExFactoryProviderSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(AbstractFillControls, "Fill Controls Definition", "Creates a single Fill Control node, to be used with flood fill nodes.")
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->NodeColorFilter); }
#endif
	//~End UPCGSettings

	virtual FName GetMainOutputPin() const override { return PCGExFloodFill::OutputFillControlsLabel; }
	virtual UPCGExFactoryData* CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const override;
};

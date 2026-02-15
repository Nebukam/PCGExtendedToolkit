// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExPointsProcessor.h"
#include "Core/PCGExValencyMap.h"
#include "Core/PCGExValencyPropertyWriter.h"

#include "PCGExValencyWriteProperties.generated.h"

/**
 * Valency : Write Properties - extracts valency-specific data as point attributes.
 * Reads ValencyEntry + Valency Map, resolves to BondingRules + module index,
 * then writes module name, cage properties, and tags.
 * This is the points-only counterpart, mirroring "Staging : Load Properties".
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Valency", meta=(Keywords = "valency property write module cage tags", PCGExNodeLibraryDoc="valency/valency-write-properties"))
class PCGEXELEMENTSVALENCY_API UPCGExValencyWritePropertiesSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ValencyWriteProperties, "Valency : Write Properties", "Write valency module properties, names, and tags as point attributes.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_OPTIN_NAME(Sampling); }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	virtual PCGExData::EIOInit GetMainDataInitializationPolicy() const override;

	/** Suffix for the ValencyEntry attribute to read (e.g. "Main" -> "PCGEx/V/Entry/Main") */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName EntrySuffix = FName("Main");

	/** If enabled, write the module name as a point attribute */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bOutputModuleName = false;

	/** Attribute name for the module name output */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable, EditCondition="bOutputModuleName"))
	FName ModuleNameAttributeName = FName("ModuleName");

	/** Cage property output configuration */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Properties", meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExValencyPropertyOutputSettings PropertiesOutput;

#if WITH_EDITOR
	/** Import property output configs from a BondingRules asset */
	UFUNCTION(CallInEditor, Category = "Settings|Properties")
	void ImportBondingRulesPropertyOutputConfigs();
#endif
};

struct PCGEXELEMENTSVALENCY_API FPCGExValencyWritePropertiesContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExValencyWritePropertiesElement;

	TSharedPtr<PCGExValency::FValencyUnpacker> ValencyUnpacker;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class PCGEXELEMENTSVALENCY_API FPCGExValencyWritePropertiesElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ValencyWriteProperties)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExValencyWriteProperties
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExValencyWritePropertiesContext, UPCGExValencyWritePropertiesSettings>
	{
		TSharedPtr<PCGExData::TBuffer<int64>> ValencyEntryReader;
		TSharedPtr<PCGExData::TBuffer<FName>> ModuleNameWriter;

		/** Per-BondingRules property writers, keyed by pointer */
		TMap<const UPCGExValencyBondingRules*, TSharedPtr<FPCGExValencyPropertyWriter>> PropertyWriters;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override = default;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
	};
}

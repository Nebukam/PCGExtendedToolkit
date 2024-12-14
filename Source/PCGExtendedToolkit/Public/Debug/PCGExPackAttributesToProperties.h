// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "PCGExPackAttributesToProperties.generated.h"

#define PCGEX_FOREACH_SAMPLING_FIELD(MACRO)\
MACRO(EdgeLength, double)


USTRUCT(BlueprintType)
struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExDebugAttributeToProperty
{
	GENERATED_BODY()

	FPCGExDebugAttributeToProperty()
	{
	}

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	FName AttributeName = NAME_None;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	EPCGExPointPropertyOutput Output = EPCGExPointPropertyOutput::Density;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bNormalize = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bClamp = true;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	bool bOneMinus = false;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bDeleteAttribute = false;

	TSharedPtr<PCGExData::TBuffer<double>> Buffer;

	bool Init(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bThrowError = false)
	{
		Buffer = InDataFacade->GetBroadcaster<double>(AttributeName, bNormalize);

		if (!Buffer)
		{
			if (bThrowError)
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Missing attribute: {0}."), FText::FromName(AttributeName)));
			}
			return false;
		}

		if (bDeleteAttribute) { InDataFacade->Source->DeleteAttribute(AttributeName); }

		return true;
	}

	void ProcessSinglePoint(FPCGPoint& InPoint, const int32 Index) const
	{
		double OutValue = Buffer->Read(Index);
		if (bNormalize) { OutValue /= Buffer->Max; }
		if (bClamp) { OutValue = FMath::Clamp(OutValue, 0.0f, 1.0f); }
		if (bOneMinus) { OutValue = 1 - OutValue; }

		switch (Output)
		{
		default:
		case EPCGExPointPropertyOutput::None:
			break;
		case EPCGExPointPropertyOutput::Density:
			InPoint.Density = OutValue;
			break;
		case EPCGExPointPropertyOutput::Steepness:
			InPoint.Steepness = OutValue;
			break;
		case EPCGExPointPropertyOutput::ColorR:
			InPoint.Color.X = OutValue;
			break;
		case EPCGExPointPropertyOutput::ColorG:
			InPoint.Color.Y = OutValue;
			break;
		case EPCGExPointPropertyOutput::ColorB:
			InPoint.Color.Z = OutValue;
			break;
		case EPCGExPointPropertyOutput::ColorA:
			InPoint.Color.W = OutValue;
			break;
		}
	}
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Debug")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPackAttributesToPropertiesSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PackAttributesToProperties, "To Float Properties", "Quick and dirty remap & pack attributes value to debug-able point properties");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Debug; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorDebug; }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin IPCGExDebug interface
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	//~End IPCGExDebug interface

public:
	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	TArray<FPCGExDebugAttributeToProperty> Remaps;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	bool bQuietWarnings = true;

private:
	friend class FPCGExPackAttributesToPropertiesElement;
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPackAttributesToPropertiesContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExPackAttributesToPropertiesElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPackAttributesToPropertiesElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExPackAttributesToProperties
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExPackAttributesToPropertiesContext, UPCGExPackAttributesToPropertiesSettings>
	{
		TArray<FPCGExDebugAttributeToProperty> Remaps;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
	};
}

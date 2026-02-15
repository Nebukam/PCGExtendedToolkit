// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExPointsProcessor.h"
#include "Core/PCGExValencyMap.h"
#include "Fitting/PCGExFitting.h"

#include "PCGExValencyAssetStaging.generated.h"

namespace PCGExCollections
{
	class FPickPacker;
}

/**
 * Valency : Staging - Bridge from valency-space to collection-space.
 * Reads ValencyEntry + Valency Map, resolves to Collection Entry + Collection Map + fitting/transforms.
 * This is the points-only counterpart to the cluster-aware "Valency : Solve".
 */
UCLASS(BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Valency", meta=(Keywords = "valency staging collection map asset", PCGExNodeLibraryDoc="valency/valency-asset-staging"))
class PCGEXELEMENTSVALENCY_API UPCGExValencyAssetStagingSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(ValencyAssetStaging, "Valency : Staging", "Resolve ValencyEntry hashes to Collection Entry + Collection Map for spawning.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_OPTIN_NAME(Sampling); }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	virtual PCGExData::EIOInit GetMainDataInitializationPolicy() const override;

	/** Suffix for the ValencyEntry attribute to read (e.g. "Main" -> "PCGEx/V/Entry/Main") */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName EntrySuffix = FName("Main");

	/** If enabled, applies the module's local transform offset to the point's transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Output", meta=(PCG_Overridable))
	bool bApplyLocalTransforms = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta=(PCG_Overridable))
	FPCGExScaleToFitDetails ScaleToFit = FPCGExScaleToFitDetails(EPCGExFitMode::None);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta=(PCG_Overridable))
	FPCGExJustificationDetails Justification = FPCGExJustificationDetails(false);
};

struct PCGEXELEMENTSVALENCY_API FPCGExValencyAssetStagingContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExValencyAssetStagingElement;

	TSharedPtr<PCGExValency::FValencyUnpacker> ValencyUnpacker;
	TSharedPtr<PCGExCollections::FPickPacker> PickPacker;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class PCGEXELEMENTSVALENCY_API FPCGExValencyAssetStagingElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(ValencyAssetStaging)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExValencyAssetStaging
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExValencyAssetStagingContext, UPCGExValencyAssetStagingSettings>
	{
		TSharedPtr<PCGExData::TBuffer<int64>> ValencyEntryReader;
		TSharedPtr<PCGExData::TBuffer<int64>> CollectionEntryWriter;

		FPCGExFittingDetailsHandler FittingHandler;

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

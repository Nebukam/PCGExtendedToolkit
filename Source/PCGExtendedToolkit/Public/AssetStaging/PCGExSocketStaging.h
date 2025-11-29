// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFactories.h"
#include "PCGExGlobalSettings.h"
#include "PCGExLabels.h"
#include "PCGExPointsProcessor.h"
#include "PCGExStaging.h"

#include "PCGExSocketStaging.generated.h"

namespace PCGExMT
{
	template <typename T>
	class TScopedNumericValue;
}

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Misc", meta=(Keywords = "stage prepare spawn proxy", PCGExNodeLibraryDoc="assets-management/asset-staging"))
class UPCGExSocketStagingSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SocketStaging, "Socket Staging", "Socket staging from Asset Staging' Collection Map.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->ColorSampling); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourcePointFiltersLabel, "Filters which points get staged.", PCGExFactories::PointFilters, false)
	//~End UPCGSettings

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExSocketOutputDetails OutputSocketDetails;
};

struct FPCGExSocketStagingContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExSocketStagingElement;

	TSharedPtr<PCGExStaging::TPickUnpacker<UPCGExAssetCollection, FPCGExAssetCollectionEntry>> CollectionPickDatasetUnpacker;
	FPCGExSocketOutputDetails OutputSocketDetails;
	TSharedPtr<PCGExData::FPointIOCollection> SocketsCollection;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExSocketStagingElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(SocketStaging)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;

	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override;
};

namespace PCGExSocketStaging
{
	const FName SourceStagingMap = TEXT("Map");

	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExSocketStagingContext, UPCGExSocketStagingSettings>
	{
		TSharedPtr<PCGExStaging::FSocketHelper> SocketHelper;
		TSharedPtr<PCGExData::TBuffer<int64>> EntryHashGetter;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void OnPointsProcessingComplete() override;
	};
}

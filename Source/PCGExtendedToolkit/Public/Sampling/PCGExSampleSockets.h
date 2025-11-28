// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPointsProcessor.h"
#include "Collections/PCGExAssetLoader.h"
#include "Collections/PCGExMeshCollection.h"
#include "Data/PCGExPointFilter.h"


#include "PCGExSampleSockets.generated.h"

namespace PCGExStaging
{
	class FSocketHelper;
}

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Sampling", meta=(PCGExNodeLibraryDoc="sampling/sockets"))
class UPCGExSampleSocketsSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(SampleSockets, "Sample : Sockets", "Parse static mesh paths and output sockets as points.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Sampler; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(GetDefault<UPCGExGlobalSettings>()->ColorSampling); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourcePointFiltersLabel, "Filters which points get processed.", PCGExFactories::PointFilters, false)
	//~End UPCGSettings

public:
	/** How the asset gets selected */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType AssetType = EPCGExInputValueType::Attribute;

	/** The name of the attribute to read asset path from.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Asset (Attr)", EditCondition="AssetType != EPCGExInputValueType::Constant", EditConditionHides))
	FName AssetPathAttributeName = "AssetPath";

	/** Constant static mesh .*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Asset", EditCondition="AssetType == EPCGExInputValueType::Constant", EditConditionHides))
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ShowOnlyInnerProperties))
	FPCGExSocketOutputDetails OutputSocketDetails;

protected:
	virtual bool IsCacheable() const override { return false; }
};

struct FPCGExSampleSocketsContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExSampleSocketsElement;

	TSharedPtr<PCGEx::TAssetLoader<UStaticMesh>> StaticMeshLoader;
	TObjectPtr<UStaticMesh> StaticMesh;

	FPCGExSocketOutputDetails OutputSocketDetails;
	TSharedPtr<PCGExData::FPointIOCollection> SocketsCollection;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL

	virtual void AddExtraStructReferencedObjects(FReferenceCollector& Collector) override;
};

class FPCGExSampleSocketsElement final : public FPCGExPointsProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(SampleSockets)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
};

namespace PCGExSampleSockets
{
	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExSampleSocketsContext, UPCGExSampleSocketsSettings>
	{
	protected:
		TSharedPtr<PCGExStaging::FSocketHelper> SocketHelper;
		TSharedPtr<TArray<PCGExValueHash>> Keys;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		virtual void OnPointsProcessingComplete() override;
	};
}

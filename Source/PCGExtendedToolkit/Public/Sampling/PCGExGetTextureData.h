// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"
#include "Data/PCGExDataForward.h"

#include "PCGExGetTextureData.generated.h"

UENUM()
enum class EPCGExGetTexturePathType : uint8
{
	TexturePath = 0 UMETA(DisplayName = "Texture Path", ToolTip="Point attribute contains a texture path"),
	MaterialPath = 1 UMETA(DisplayName = "Material Path", ToolTip="Point attribute contains a material path")
};

class UPCGExFilterFactoryBase;

UCLASS(Abstract, MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Sampling")
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExGetTextureDataSettings : public UPCGExPointsProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExGetTextureDataSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(GetTextureData, "Get Texture Data", "Create texture data object from paths.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->NodeColorSampler; }
#endif

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

	//~Begin UPCGExPointsProcessorSettings
public:
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourcePointFiltersLabel, "Filters", PCGExFactories::PointFilters, false)
	//~End UPCGExPointsProcessorSettings

	/** Type of path */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExGetTexturePathType SourceType = EPCGExGetTexturePathType::MaterialPath;
	
	/** Name of the attribute to read asset path from.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditConditionHides))
	FName SourceAttributeName = FName("AssetPath");

	/** Where to read the texture parameter name to look for in the material */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="PathType==EPCGExGetTexturePathType::MaterialPath", EditConditionHides))
	EPCGExInputValueType TextureParameterNameType = EPCGExInputValueType::Constant;

	/**  Value is expected to be 0-1 normalized, 0 being bounds min and 1 being bounds min + size. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, EditCondition="PathType==EPCGExGetTexturePathType::MaterialPath", EditConditionHides))
	FName TextureParameterName = FName("TextureParamName");
	
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExGetTextureDataContext final : FPCGExPointsProcessorContext
{
	friend class FPCGExGetTextureDataElement;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExGetTextureDataElement final : public FPCGExPointsProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

	PCGEX_CAN_ONLY_EXECUTE_ON_MAIN_THREAD(true)
	
protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
};

namespace PCGExGetTextureData
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExGetTextureDataContext, UPCGExGetTextureDataSettings>
	{
		TSharedPtr<PCGExData::TBuffer<double>> MaxDistanceGetter;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
		virtual void Write() override;
	};
}

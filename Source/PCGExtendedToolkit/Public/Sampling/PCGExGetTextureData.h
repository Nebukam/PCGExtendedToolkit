// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExGlobalSettings.h"

#include "PCGExPointsProcessor.h"
#include "PCGExSampling.h"
#include "PCGExTexParamFactoryProvider.h"
#include "Data/PCGExDataForward.h"
#include "Data/PCGTextureData.h"

#include "PCGExGetTextureData.generated.h"

UENUM()
enum class EPCGExGetTexturePathType : uint8
{
	TexturePath  = 0 UMETA(DisplayName = "Texture Path", ToolTip="Point attribute contains a texture path"),
	MaterialPath = 1 UMETA(DisplayName = "Material Path", ToolTip="Point attribute contains a material path")
};

/** Method used to determine the value for a sample based on the value of nearby texels. */
UENUM(BlueprintType)
enum class EPCGExTextureFilter : uint8
{
	Point UMETA(Tooltip="Takes the value of whatever texel the sample lands in."),
	Bilinear UMETA(Tooltip="Bilinearly interpolates the values of the four nearest texels to the sample location.")
};

class UPCGExFilterFactoryBase;

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Sampling")
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

	/** Name of the attribute to read asset path from (material or texture).*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName SourceAttributeName = FName("AssetPath");

	/** If enabled, will write resolved texture paths as per their definitions. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="SourceType==EPCGExGetTexturePathType::MaterialPath", EditConditionHides))
	bool bOutputTexturePaths = false;

	/** If enabled, will build PCG Texture data for each unique texture reference found. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="SourceType==EPCGExGetTexturePathType::MaterialPath", EditConditionHides))
	bool bBuildTextureData = true;

	
	/** Method used to determine the value for a sample based on the value of nearby texels. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data", meta = (PCG_Overridable))
	EPCGExTextureFilter Filter = EPCGExTextureFilter::Bilinear;

	/** Surface transform */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data", meta = (PCG_Overridable))
	FTransform Transform = FTransform::Identity;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data", meta = (PCG_Overridable))
	bool bUseAbsoluteTransform = false;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data")
	EPCGTextureColorChannel ColorChannel = EPCGTextureColorChannel::Alpha;

	/** The size of one texel in cm, used when calling ToPointData. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data", meta = (UIMin = "1.0", ClampMin = "1.0"))
	float TexelSize = 50.0f;

	/** Rotation to apply when sampling texture. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data", meta = (UIMin = -360, ClampMin = -360, UIMax = 360, ClampMax = 360, Units = deg, EditCondition = "bUseAdvancedTiling"))
	float Rotation = 0;
	
	/** Whether to tile the source or to stretch it to fit target area. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data")
	bool bUseAdvancedTiling = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data|Tiling", meta = (EditCondition = "bUseAdvancedTiling"))
	FVector2D Tiling = FVector2D(1.0, 1.0);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data|Tiling", meta = (EditCondition = "bUseAdvancedTiling"))
	FVector2D CenterOffset = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data|Tiling", meta = (EditionCondition = "bUseAdvancedTiling"))
	bool bUseTileBounds = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Data|Tiling", meta = (EditCondition = "bUseAdvancedTiling && bUseTileBounds"))
	FBox2D TileBounds = FBox2D(FVector2D(-0.5, -0.5), FVector2D(0.5, 0.5));
	
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExGetTextureDataContext final : FPCGExPointsProcessorContext
{
	FRWLock ReferenceLock;
	friend class FPCGExGetTextureDataElement;

	TArray<TObjectPtr<const UPCGExTexParamFactoryBase>> TexParamsFactories;
	
	TSet<PCGExTexParam::FReference> TextureReferences;
	TArray<PCGExTexParam::FReference> TextureReferencesList;
	TArray<int8> TextureReady;
	
	FTransform Transform;
	
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
		FRWLock ReferenceLock;

#if PCGEX_ENGINE_VERSION <= 503
		TSharedPtr<PCGExData::TBuffer<FString>> PathGetter;
#else
		TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> PathGetter;
#endif

		TSharedPtr<PCGExTexParam::FLookup> TexParamLookup;
		
		TSharedPtr<TSet<FSoftObjectPath>> MaterialReferences;
		TSet<PCGExTexParam::FReference> TextureReferences;

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
	};

	class /*PCGEXTENDEDTOOLKIT_API*/ FCreateTextureTask final : public PCGExMT::FPCGExIndexedTask
	{
	public:
		PCGEX_ASYNC_TASK_NAME(TDiscoverAssetsTask)

		explicit FCreateTextureTask(const int32 InTaskIndex) :
			FPCGExIndexedTask(InTaskIndex)
		{
		}

		virtual void ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager) override;
	};
}

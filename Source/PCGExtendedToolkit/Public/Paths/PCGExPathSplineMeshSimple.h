// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPathProcessor.h"
#include "PCGExPaths.h"
#include "PCGExPointsProcessor.h"
#include "Collections/PCGExAssetLoader.h"
#include "Collections/PCGExMeshCollection.h"

#include "Tangents/PCGExTangentsOperation.h"
#include "Components/SplineMeshComponent.h"


#include "PCGExPathSplineMeshSimple.generated.h"

/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path")
class UPCGExPathSplineMeshSimpleSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPathSplineMeshSimpleSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathSplineMeshSimple, "Path : Spline Mesh (Simple)", "Create spline mesh components from paths.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->WantsColor(UPCGExPathProcessorSettings::GetNodeTitleColor()); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourcePointFiltersLabel, "Filters", PCGExFactories::PointFilters, false)

	/** How the asset gets selected */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType AssetType = EPCGExInputValueType::Attribute;

	/** The name of the attribute to write asset path to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Asset (Attr)", EditCondition="AssetType != EPCGExInputValueType::Constant", EditConditionHides))
	FName AssetPathAttributeName = "AssetPath";

	/** Constant static mesh .*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Asset", EditCondition="AssetType == EPCGExInputValueType::Constant", EditConditionHides))
	TSoftObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Target Actor", meta = (PCG_Overridable))
	TSoftObjectPtr<AActor> TargetActor;

	/**  */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Target Actor", meta = (PCG_Overridable))
	//bool bPerSegmentTargetActor = false;

	/**  */
	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Target Actor", meta=(PCG_Overridable, EditCondition="bPerSegmentTargetActor", EditConditionHides))
	//FName TargetActorAttributeName;

	/** Whether to read tangents from attributes or not. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bApplyCustomTangents = false;

	/** Arrive tangent attribute (expects FVector) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bApplyCustomTangents"))
	FName ArriveTangentAttribute = "ArriveTangent";

	/** Leave tangent attribute (expects FVector) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="bApplyCustomTangents"))
	FName LeaveTangentAttribute = "LeaveTangent";

	/** Type of Start Offset */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Mutations|Offsets", meta=(PCG_NotOverridable))
	EPCGExInputValueType StartOffsetInput = EPCGExInputValueType::Constant;

	/** Start Offset Attribute (Vector 2 expected)*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Mutations|Offsets", meta=(PCG_Overridable, DisplayName="Start Offset (Attr)", EditCondition="StartOffsetInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FName StartOffsetAttribute = FName("StartOffset");

	/** Start Offset Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Mutations|Offsets", meta=(PCG_Overridable, DisplayName="Start Offset", EditCondition="StartOffsetInput==EPCGExInputValueType::Constant", EditConditionHides))
	FVector2D StartOffset = FVector2D::ZeroVector;

	PCGEX_SETTING_VALUE_GET(StartOffset, FVector2D, StartOffsetInput, StartOffsetAttribute, StartOffset)

	/** Type of End Offset */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Mutations|Offsets", meta=(PCG_NotOverridable))
	EPCGExInputValueType EndOffsetInput = EPCGExInputValueType::Constant;

	/** End Offset Attribute (Vector 2 expected)*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Mutations|Offsets", meta=(PCG_Overridable, DisplayName="End Offset (Attr)", EditCondition="EndOffsetInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FName EndOffsetAttribute = FName("EndOffset");

	/** End Offset Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Mutations|Offsets", meta=(PCG_Overridable, DisplayName="End Offset", EditCondition="EndOffsetInput==EPCGExInputValueType::Constant", EditConditionHides))
	FVector2D EndOffset = FVector2D::ZeroVector;

	PCGEX_SETTING_VALUE_GET(EndOffset, FVector2D, EndOffsetInput, EndOffsetAttribute, EndOffset)

	/** Push details */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Mutations", meta=(PCG_Overridable, DisplayName="Expansion"))
	FPCGExSplineMeshMutationDetails MutationDetails;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExSplineMeshUpMode SplineMeshUpMode = EPCGExSplineMeshUpMode::Constant;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Spline Mesh Up Vector (Attr)", EditCondition="SplineMeshUpMode==EPCGExSplineMeshUpMode::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector SplineMeshUpVectorAttribute;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Spline Mesh Up Vector", EditCondition="SplineMeshUpMode==EPCGExSplineMeshUpMode::Constant", EditConditionHides))
	FVector SplineMeshUpVector = FVector::UpVector;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Spline Axis Align"))
	EPCGExMinimalAxis SplineMeshAxisConstant = EPCGExMinimalAxis::X;

	/** Tagging details */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Additional Outputs", meta=(PCG_Overridable))
	FPCGExAssetTaggingDetails TaggingDetails;

	/** Default static mesh config applied to spline mesh components. */
	UPROPERTY(EditAnywhere, Category = Settings)
	FPCGExStaticMeshComponentDescriptor StaticMeshDescriptor;

	/** Specify a list of functions to be called on the target actor after spline mesh creation. Functions need to be parameter-less and with "CallInEditor" flag enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FName> PostProcessFunctionNames;

protected:
	virtual bool IsCacheable() const override { return false; }
};

struct FPCGExPathSplineMeshSimpleContext final : FPCGExPathProcessorContext
{
	friend class FPCGExPathSplineMeshSimpleElement;

	TSharedPtr<PCGEx::TAssetLoader<UStaticMesh>> StaticMeshLoader;

	UPROPERTY()
	TObjectPtr<UStaticMesh> StaticMesh;
};

class FPCGExPathSplineMeshSimpleElement final : public FPCGExPathProcessorElement
{
public:
	// Generates artifacts
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(PathSplineMeshSimple)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
};

namespace PCGExPathSplineMeshSimple
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExPathSplineMeshSimpleContext, UPCGExPathSplineMeshSimpleSettings>
	{
	protected:
		bool bClosedLoop = false;
		bool bUseTags = false;

		FPCGExSplineMeshMutationDetails MutationDetails;

		int32 LastIndex = 0;

		int32 C1 = 1;
		int32 C2 = 2;

		TSharedPtr<PCGExData::TBuffer<FVector>> UpGetter;
		TSharedPtr<PCGExDetails::TSettingValue<FVector2D>> StartOffset;
		TSharedPtr<PCGExDetails::TSettingValue<FVector2D>> EndOffset;

		TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> AssetPathReader;

		TSharedPtr<PCGExData::TBuffer<FVector>> ArriveReader;
		TSharedPtr<PCGExData::TBuffer<FVector>> LeaveReader;

		TArray<PCGExPaths::FSplineMeshSegment> Segments;
		TArray<TObjectPtr<UStaticMesh>> Meshes;
		//TArray<USplineMeshComponent*> SplineMeshComponents;

		ESplineMeshAxis::Type SplineMeshAxisConstant = ESplineMeshAxis::Type::X;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade):
			TPointsProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;
		
		virtual void CompleteWork() override;

		virtual void Output() override;
	};
}

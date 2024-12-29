// Copyright 2024 Timothé Lapetite and contributors
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
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExPathSplineMeshSimpleSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(PathSplineMeshSimple, "Path : Spline Mesh (Simple)", "Create spline mesh components from paths.");
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	PCGEX_NODE_POINT_FILTER(PCGExPointFilter::SourcePointFiltersLabel, "Filters", PCGExFactories::PointFilters, false)
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;

	/** The name of the attribute to write asset path to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FName AssetPathAttributeName = "AssetPath";

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
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType StartOffsetInput = EPCGExInputValueType::Constant;

	/** Start Offset Attribute (Vector 2 expected)*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Start Offset", EditCondition="StartOffsetInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FName StartOffsetAttribute = FName("StartOffset");

	/** Start Offset Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Start Offset", EditCondition="StartOffsetInput==EPCGExInputValueType::Constant", EditConditionHides))
	FVector2D StartOffset = FVector2D::ZeroVector;

	/** Type of End Offset */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExInputValueType EndOffsetInput = EPCGExInputValueType::Constant;

	/** End Offset Attribute (Vector 2 expected)*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="End Offset", EditCondition="EndOffsetInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FName EndOffsetAttribute = FName("EndOffset");

	/** End Offset Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="End Offset", EditCondition="EndOffsetInput==EPCGExInputValueType::Constant", EditConditionHides))
	FVector2D EndOffset = FVector2D::ZeroVector;

	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
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
};

struct /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathSplineMeshSimpleContext final : FPCGExPathProcessorContext
{
	friend class FPCGExPathSplineMeshSimpleElement;

	TSet<AActor*> NotifyActors;
	TSharedPtr<PCGEx::TAssetLoader<UStaticMesh>> StaticMeshLoader;
};

class /*PCGEXTENDEDTOOLKIT_API*/ FPCGExPathSplineMeshSimpleElement final : public FPCGExPathProcessorElement
{
public:
	virtual FPCGContext* Initialize(
		const FPCGDataCollection& InputData,
		TWeakObjectPtr<UPCGComponent> SourceComponent,
		const UPCGNode* Node) override;

protected:
	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* Context) const override;
	virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }
};

namespace PCGExPathSplineMeshSimple
{
	class FProcessor final : public PCGExPointsMT::TPointsProcessor<FPCGExPathSplineMeshSimpleContext, UPCGExPathSplineMeshSimpleSettings>
	{
		bool bClosedLoop = false;
		bool bUseTags = false;

		int32 LastIndex = 0;

		int32 C1 = 1;
		int32 C2 = 2;

		TSharedPtr<PCGExData::TBuffer<FVector2D>> StartOffsetGetter;
		TSharedPtr<PCGExData::TBuffer<FVector2D>> EndOffsetGetter;

#if PCGEX_ENGINE_VERSION <= 503
		TSharedPtr<PCGExData::TBuffer<FString>> AssetPathReader;
#else
		TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> AssetPathReader;
#endif


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

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager) override;
		virtual void PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope) override;
		virtual void ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;

		virtual void Output() override;
	};
}

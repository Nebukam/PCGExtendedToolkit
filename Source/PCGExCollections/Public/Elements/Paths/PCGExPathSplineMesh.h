// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFilterCommon.h"
#include "Core/PCGExPathProcessor.h"
#include "Collections/PCGExMeshCollection.h"
#include "Details/PCGExRoamingAssetCollectionDetails.h"
#include "Details/PCGExSplineMeshDetails.h"
#include "Details/PCGExStagingDetails.h"
#include "Factories/PCGExFactories.h"
#include "Fitting/PCGExFitting.h"
#include "Math/PCGExMathAxis.h"
#include "Metadata/PCGObjectPropertyOverride.h"

#include "Tangents/PCGExTangentsInstancedFactory.h"

#include "PCGExPathSplineMesh.generated.h"

namespace PCGExCollections
{
	class FMicroDistributionHelper;
	class FDistributionHelper;
}

namespace PCGExMT
{
	class FTimeSlicedMainThreadLoop;

	template <typename T>
	class TScopedSet;
}


/**
 * 
 */
UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Path", meta=(PCGExNodeLibraryDoc="paths/spline-mesh"))
class UPCGExPathSplineMeshSettings : public UPCGExPathProcessorSettings
{
	GENERATED_BODY()

public:
	UPCGExPathSplineMeshSettings(const FObjectInitializer& ObjectInitializer);

	//~Begin UPCGSettings
#if WITH_EDITOR
	virtual void ApplyDeprecation(UPCGNode* InOutNode) override;

	PCGEX_NODE_INFOS(PathSplineMesh, "Path : Spline Mesh", "Create spline mesh components from paths.");
	virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spawner; }
	virtual FLinearColor GetNodeTitleColor() const override { return PCGEX_NODE_COLOR_OPTIN(UPCGExPathProcessorSettings::GetNodeTitleColor()); }
#endif

protected:
	virtual FPCGElementPtr CreateElement() const override;
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	//~End UPCGSettings

	virtual PCGExData::EIOInit GetMainDataInitializationPolicy() const override;

public:
	PCGEX_NODE_POINT_FILTER(PCGExFilters::Labels::SourcePointFiltersLabel, "Filters", PCGExFactories::PointFilters, false)

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExCollectionSource CollectionSource = EPCGExCollectionSource::Asset;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CollectionSource == EPCGExCollectionSource::Asset", EditConditionHides))
	TSoftObjectPtr<UPCGExMeshCollection> AssetCollection;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, EditCondition="CollectionSource == EPCGExCollectionSource::AttributeSet", EditConditionHides))
	FPCGExRoamingAssetCollectionDetails AttributeSetDetails = FPCGExRoamingAssetCollectionDetails(UPCGExMeshCollection::StaticClass());

	/** Distribution details */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExAssetDistributionDetails DistributionSettings;

	/** How should materials be distributed and picked. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExMicroCacheDistributionDetails MaterialDistributionSettings;

#pragma region DEPRECATED

	UPROPERTY()
	bool bApplyCustomTangents_DEPRECATED = false;

	UPROPERTY()
	FName ArriveTangentAttribute_DEPRECATED = "ArriveTangent";

	UPROPERTY()
	FName LeaveTangentAttribute_DEPRECATED = "LeaveTangent";

	UPROPERTY()
	EPCGExMinimalAxis SplineMeshAxisConstant_DEPRECATED = EPCGExMinimalAxis::X;

#pragma endregion

	/** Per-point tangent settings. Can't be set if the spline is linear. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	FPCGExTangentsDetails Tangents;

	/** If enabled, will break scaling interpolation across the spline. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta=(PCG_Overridable))
	FPCGExScaleToFitDetails ScaleToFit = FPCGExScaleToFitDetails(EPCGExFitMode::None);

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Fitting", meta=(PCG_Overridable))
	FPCGExJustificationDetails Justification;

	/** Push details */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Expansion"))
	FPCGExSplineMeshMutationDetails MutationDetails;

	/** The name of the attribute to write asset path to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable))
	FName AssetPathAttributeName = "AssetPath";

	/** Tagging details */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable))
	FPCGExAssetTaggingDetails TaggingDetails;

	/** Update point scale so staged asset fits within its bounds */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable))
	EPCGExWeightOutputMode WeightToAttribute = EPCGExWeightOutputMode::NoOutput;

	/** The name of the attribute to write asset weight to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, EditCondition="WeightToAttribute != EPCGExWeightOutputMode::NoOutput && WeightToAttribute != EPCGExWeightOutputMode::NormalizedToDensity && WeightToAttribute != EPCGExWeightOutputMode::NormalizedInvertedToDensity"))
	FName WeightAttributeName = "AssetWeight";

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_NotOverridable))
	EPCGExSplineMeshUpMode SplineMeshUpMode = EPCGExSplineMeshUpMode::Constant;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Spline Mesh Up Vector (Attr)", EditCondition="SplineMeshUpMode == EPCGExSplineMeshUpMode::Attribute", EditConditionHides))
	FPCGAttributePropertyInputSelector SplineMeshUpVectorAttribute;

	/** */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Spline Mesh Up Vector", EditCondition="SplineMeshUpMode == EPCGExSplineMeshUpMode::Constant", EditConditionHides))
	FVector SplineMeshUpVector = FVector::UpVector;


	/** Default static mesh config applied to spline mesh components. */
	UPROPERTY(EditAnywhere, Category = Settings)
	FPCGExStaticMeshComponentDescriptor DefaultDescriptor;

	/** If enabled, override collection settings with the default descriptor settings */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName=" └─ Force Default Descriptor", EditCondition="CollectionSource != EPCGExCollectionSource::AttributeSet"))
	bool bForceDefaultDescriptor = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
	TArray<FPCGObjectPropertyOverrideDescription> PropertyOverrideDescriptions;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable), AdvancedDisplay)
	TSoftObjectPtr<AActor> TargetActor;

	/** Specify a list of functions to be called on the target actor after spline mesh creation. Functions need to be parameter-less and with "CallInEditor" flag enabled. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, AdvancedDisplay)
	TArray<FName> PostProcessFunctionNames;

protected:
	virtual bool IsCacheable() const override { return false; }
};

struct FPCGExPathSplineMeshContext final : FPCGExPathProcessorContext
{
	friend class FPCGExPathSplineMeshElement;

	virtual void RegisterAssetDependencies() override;

	FPCGExTangentsDetails Tangents;

	TObjectPtr<UPCGExMeshCollection> MainCollection;

protected:
	PCGEX_ELEMENT_BATCH_POINT_DECL
};

class FPCGExPathSplineMeshElement final : public FPCGExPathProcessorElement
{
public:
	// Generates artifacts
	virtual bool IsCacheable(const UPCGSettings* InSettings) const override { return false; }

protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(PathSplineMesh)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual void PostLoadAssetsDependencies(FPCGExContext* InContext) const override;
	virtual bool PostBoot(FPCGExContext* InContext) const override;
	virtual bool AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const override;
};

namespace PCGExPathSplineMesh
{
	struct FSplineMeshSegment : PCGExPaths::FSplineMeshSegment
	{
		FSplineMeshSegment()
		{
		}

		bool bSetMeshWithSettings = false;

		const FPCGExMeshCollectionEntry* MeshEntry = nullptr;
		int16 MaterialPick = -1;

		virtual void ApplySettings(USplineMeshComponent* Component) const override;
		bool ApplyMesh(USplineMeshComponent* Component) const;
	};

	class FProcessor final : public PCGExPointsMT::TProcessor<FPCGExPathSplineMeshContext, UPCGExPathSplineMeshSettings>
	{
	protected:
		bool bOutputWeight = false;
		bool bOneMinusWeight = false;
		bool bNormalizedWeight = false;
		int8 bHasValidSegments = false;

		bool bIsPreviewMode = false;
		bool bClosedLoop = false;
		bool bApplyScaleToFit = false;
		bool bUseTags = false;

		int32 LastIndex = 0;

		TSharedPtr<PCGExTangents::FTangentsHandler> TangentsHandler;

		TSharedPtr<PCGExCollections::FDistributionHelper> Helper;
		TSharedPtr<PCGExCollections::FMicroDistributionHelper> MicroHelper;

		FPCGExJustificationDetails Justification;
		FPCGExSplineMeshMutationDetails SegmentMutationDetails;

		TSharedPtr<PCGExMT::TScopedSet<FSoftObjectPath>> ScopedMaterials;
		TSharedPtr<PCGExData::TBuffer<FVector>> UpGetter;

		TSharedPtr<PCGExData::TBuffer<int32>> WeightWriter;
		TSharedPtr<PCGExData::TBuffer<double>> NormalizedWeightWriter;

		TArray<FName> DataTags;

		TSharedPtr<PCGExData::TBuffer<FSoftObjectPath>> PathWriter;

		TSharedPtr<PCGExMT::FTimeSlicedMainThreadLoop> MainThreadLoop;
		TArray<FSplineMeshSegment> Segments;

		AActor* TargetActor = nullptr;
		EObjectFlags ObjectFlags = RF_NoFlags;

	public:
		explicit FProcessor(const TSharedRef<PCGExData::FFacade>& InPointDataFacade)
			: TProcessor(InPointDataFacade)
		{
		}

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager) override;

		virtual void PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops) override;
		virtual void ProcessPoints(const PCGExMT::FScope& Scope) override;

		virtual void OnPointsProcessingComplete() override;
		void ProcessSegment(const int32 Index);

		virtual void CompleteWork() override;
	};
}

﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Data/Blending/PCGExDataBlending.h"
#include "Details/PCGExSettingsMacros.h"
#include "Graph/PCGExClusterMT.h"
#include "Graph/PCGExEdgesProcessor.h"
#include "Sampling/PCGExSampling.h"

#include "PCGExWriteEdgeProperties.generated.h"

#define PCGEX_FOREACH_FIELD_EDGEEXTRAS(MACRO) \
MACRO(EdgeLength, double, 0) \
MACRO(EdgeDirection, FVector, FVector::OneVector) \
MACRO(Heuristics, double, 0)

class UPCGExBlendOpFactory;

namespace PCGExDataBlending
{
	class IBlender;
	class FMetadataBlender;
	class FBlendOpsManager;
}

UENUM()
enum class EPCGExHeuristicsWriteMode : uint8
{
	EndpointsOrder = 0 UMETA(DisplayName = "Endpoints Order", ToolTip="Use endpoint order heuristics."),
	Smallest       = 1 UMETA(DisplayName = "Smallest Score", ToolTip="Compute heuristics both ways a keep smallest score"),
	Highest        = 2 UMETA(DisplayName = "Highest Score", ToolTip="Compute heuristics both ways a keep highest score."),
};

UCLASS(MinimalAPI, BlueprintType, ClassGroup = (Procedural), Category="PCGEx|Clusters", meta=(PCGExNodeLibraryDoc="metadata/edge-properties"))
class UPCGExWriteEdgePropertiesSettings : public UPCGExEdgesProcessorSettings
{
	GENERATED_BODY()

public:
	//~Begin UPCGSettings
#if WITH_EDITOR
	PCGEX_NODE_INFOS(WriteEdgeProperties, "Cluster : Edge Properties", "Extract & write extra edge informations to the point representing the edge.");
	virtual FLinearColor GetNodeTitleColor() const override { return GetDefault<UPCGExGlobalSettings>()->ColorNeighborSampler; }
#endif

	virtual bool SupportsEdgeSorting() const override { return DirectionSettings.RequiresSortingRules(); }
	virtual PCGExData::EIOInit GetMainOutputInitMode() const override;
	virtual PCGExData::EIOInit GetEdgeOutputInitMode() const override;

protected:
	virtual TArray<FPCGPinProperties> InputPinProperties() const override;
	virtual bool IsPinUsedByNodeExecution(const UPCGPin* InPin) const override;
	virtual FPCGElementPtr CreateElement() const override;
	//~End UPCGSettings

public:
	/** Defines the direction in which points will be ordered to form the final paths. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	FPCGExEdgeDirectionSettings DirectionSettings;

	/** Output Edge Length. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteEdgeLength = false;

	/** Name of the 'boolean' attribute to write sampling success to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="EdgeLength", PCG_Overridable, EditCondition="bWriteEdgeLength"))
	FName EdgeLengthAttributeName = FName("EdgeLength");

	/** Output Edge Direction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteEdgeDirection = false;

	/** Name of the 'boolean' attribute to write sampling success to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="EdgeDirection", PCG_Overridable, EditCondition="bWriteEdgeDirection"))
	FName EdgeDirectionAttributeName = FName("EdgeDirection");

	/** Edges will inherit point attributes*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta = (PCG_Overridable))
	bool bEndpointsBlending = false;

	/** Balance between start/end point ( When enabled, this value will be overriden by EdgePositionLerp, and Solidification, in that order. )*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, EditCondition="bEndpointsBlending && !bWriteEdgePosition && SolidificationAxis == EPCGExMinimalAxis::None", ClampMin=0, ClampMax=1))
	double EndpointsWeights = 0.5;

	/** How to blend data from sampled points */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Blending", meta=(PCG_Overridable, EditCondition="bEndpointsBlending"))
	EPCGExBlendingInterface BlendingInterface = EPCGExBlendingInterface::Individual;

	/** Defines how fused point properties and attributes are merged together. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(EditCondition="bEndpointsBlending && BlendingInterface == EPCGExBlendingInterface::Monolithic", EditConditionHides))
	FPCGExBlendingDetails BlendingSettings = FPCGExBlendingDetails(EPCGExDataBlendingType::Average);

	/** Output Edge Heuristics. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, InlineEditConditionToggle))
	bool bWriteHeuristics = false;

	/** Name of the 'double' attribute to write heuristics to.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(DisplayName="Heuristics", PCG_Overridable, EditCondition="bWriteHeuristics"))
	FName HeuristicsAttributeName = FName("Heuristics");

	/** Heuristic write mode. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Outputs", meta=(PCG_Overridable, DisplayName=" └─ Heuristics Mode", EditCondition="bWriteHeuristics", EditConditionHides, HideEditConditionToggle))
	EPCGExHeuristicsWriteMode HeuristicsMode = EPCGExHeuristicsWriteMode::EndpointsOrder;


	/** Update Edge position as a lerp between endpoints (according to the direction method selected above) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification", meta=(PCG_Overridable))
	bool bWriteEdgePosition = false;

	/** Position position lerp between start & end points*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification", meta=(PCG_Overridable, EditCondition="bWriteEdgePosition && SolidificationAxis == EPCGExMinimalAxis::None", ClampMin=0, ClampMax=1))
	double EdgePositionLerp = 0.5;

	/** Align the edge point to the edge direction over the selected axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification", meta = (PCG_Overridable))
	EPCGExMinimalAxis SolidificationAxis = EPCGExMinimalAxis::None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification", meta=(PCG_Overridable, EditCondition="SolidificationAxis != EPCGExMinimalAxis::None"))
	EPCGExInputValueType SolidificationLerpInput = EPCGExInputValueType::Constant;

	/** Solidification Lerp attribute (read from Edge).*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification", meta=(PCG_Overridable, DisplayName="Solidification Lerp (Attr)", EditCondition="SolidificationLerpInput == EPCGExInputValueType::Attribute && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	FPCGAttributePropertyInputSelector SolidificationLerpAttribute;

	/** Solidification Lerp constant.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification", meta=(PCG_Overridable, DisplayName="Solidification Lerp", EditCondition="SolidificationLerpInput == EPCGExInputValueType::Constant && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	double SolidificationLerpConstant = 0.5;

	PCGEX_SETTING_VALUE_DECL(SolidificationLerp, double)

	// Edge radiuses

	/** Whether or not to write the edge extents over the local X axis.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta=(PCG_Overridable, EditCondition="SolidificationAxis != EPCGExMinimalAxis::X && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	bool bWriteRadiusX = false;

	/** Type of Radius X value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta=(PCG_Overridable, EditCondition="bWriteRadiusX && SolidificationAxis != EPCGExMinimalAxis::X && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	EPCGExInputValueType RadiusXInput = EPCGExInputValueType::Constant;

	/** Source from which to fetch the Radius X value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta=(PCG_Overridable, EditCondition="bWriteRadiusX && SolidificationAxis != EPCGExMinimalAxis::X && SolidificationAxis != EPCGExMinimalAxis::None && RadiusXInput != EPCGExInputValueType::Constant", EditConditionHides))
	EPCGExClusterElement RadiusXSource = EPCGExClusterElement::Vtx;

	/** Attribute read on edge endpoints */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta = (PCG_Overridable, DisplayName="Radius X (Attr)", EditCondition="bWriteRadiusX && SolidificationAxis != EPCGExMinimalAxis::X && SolidificationAxis != EPCGExMinimalAxis::None && RadiusXInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector RadiusXSourceAttribute;

	/** Radius X Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta = (PCG_Overridable, DisplayName="Radius X", EditCondition="bWriteRadiusX && SolidificationAxis != EPCGExMinimalAxis::X && SolidificationAxis != EPCGExMinimalAxis::None && RadiusXInput == EPCGExInputValueType::Constant", EditConditionHides))
	double RadiusXConstant = 1;


	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta=(PCG_Overridable, EditCondition="SolidificationAxis != EPCGExMinimalAxis::Y && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	bool bWriteRadiusY = false;

	/** Type of Radius Y value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta=(PCG_Overridable, EditCondition="bWriteRadiusY && SolidificationAxis != EPCGExMinimalAxis::Y && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	EPCGExInputValueType RadiusYInput = EPCGExInputValueType::Constant;

	/** Source from which to fetch the Radius Y value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta=(PCG_Overridable, EditCondition="bWriteRadiusY && SolidificationAxis != EPCGExMinimalAxis::Y && SolidificationAxis != EPCGExMinimalAxis::None && RadiusYInput != EPCGExInputValueType::Constant", EditConditionHides))
	EPCGExClusterElement RadiusYSource = EPCGExClusterElement::Vtx;

	/** Attribute read on edge endpoints */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta = (PCG_Overridable, DisplayName="Radius Y (Attr)", EditCondition="bWriteRadiusY && SolidificationAxis != EPCGExMinimalAxis::Y && SolidificationAxis != EPCGExMinimalAxis::None && RadiusYInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector RadiusYSourceAttribute;

	/** Radius Y Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta = (PCG_Overridable, DisplayName="Radius Y", EditCondition="bWriteRadiusY && SolidificationAxis != EPCGExMinimalAxis::Y && SolidificationAxis != EPCGExMinimalAxis::None && RadiusYInput == EPCGExInputValueType::Constant", EditConditionHides))
	double RadiusYConstant = 1;


	/**  */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta=(PCG_Overridable, EditCondition="SolidificationAxis != EPCGExMinimalAxis::Z && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	bool bWriteRadiusZ = false;

	/** Type of Radius Z value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta=(PCG_Overridable, EditCondition="bWriteRadiusZ && SolidificationAxis != EPCGExMinimalAxis::Z && SolidificationAxis != EPCGExMinimalAxis::None", EditConditionHides))
	EPCGExInputValueType RadiusZInput = EPCGExInputValueType::Constant;

	/** Source from which to fetch the Radius Z value */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta=(PCG_Overridable, EditCondition="bWriteRadiusZ && SolidificationAxis != EPCGExMinimalAxis::Z && SolidificationAxis != EPCGExMinimalAxis::None && RadiusZInput != EPCGExInputValueType::Constant", EditConditionHides))
	EPCGExClusterElement RadiusZSource = EPCGExClusterElement::Vtx;

	/** Attribute read on edge endpoints */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta = (PCG_Overridable, DisplayName="Radius Z (Attr)", EditCondition="bWriteRadiusZ && SolidificationAxis != EPCGExMinimalAxis::Z && SolidificationAxis != EPCGExMinimalAxis::None && RadiusZInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector RadiusZSourceAttribute;

	/** Radius Z Constant */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings|Solidification|Radiuses", meta = (PCG_Overridable, DisplayName="Radius Z", EditCondition="bWriteRadiusZ && SolidificationAxis != EPCGExMinimalAxis::Z && SolidificationAxis != EPCGExMinimalAxis::None && RadiusZInput == EPCGExInputValueType::Constant", EditConditionHides))
	double RadiusZConstant = 1;

private:
	friend class FPCGExWriteEdgePropertiesElement;
};

struct FPCGExWriteEdgePropertiesContext final : FPCGExEdgesProcessorContext
{
	friend class FPCGExWriteEdgePropertiesElement;

	PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_DECL_TOGGLE)

	TArray<TObjectPtr<const UPCGExBlendOpFactory>> BlendingFactories;

protected:
	PCGEX_ELEMENT_BATCH_EDGE_DECL
};

class FPCGExWriteEdgePropertiesElement final : public FPCGExEdgesProcessorElement
{
protected:
	PCGEX_ELEMENT_CREATE_CONTEXT(WriteEdgeProperties)

	virtual bool Boot(FPCGExContext* InContext) const override;
	virtual bool ExecuteInternal(FPCGContext* InContext) const override;
};

namespace PCGExWriteEdgeProperties
{
	class FProcessor final : public PCGExClusterMT::TProcessor<FPCGExWriteEdgePropertiesContext, UPCGExWriteEdgePropertiesSettings>
	{
		FPCGExEdgeDirectionSettings DirectionSettings;

		double StartWeight = 0;
		double EndWeight = 1;

		TSharedPtr<PCGExDataBlending::FBlendOpsManager> BlendOpsManager;
		TSharedPtr<PCGExDataBlending::FMetadataBlender> MetadataBlender;
		TSharedPtr<PCGExDataBlending::IBlender> DataBlender;

		TSharedPtr<PCGExDetails::TSettingValue<double>> SolidificationLerp;

		PCGEX_FOREACH_FIELD_EDGEEXTRAS(PCGEX_OUTPUT_DECL)

		bool bSolidify = false;

#define PCGEX_LOCAL_EDGE_GETTER_DECL(_AXIS) TSharedPtr<PCGExDetails::TSettingValue<double>> SolidificationRad##_AXIS;
		PCGEX_FOREACH_XYZ(PCGEX_LOCAL_EDGE_GETTER_DECL)
#undef PCGEX_LOCAL_EDGE_GETTER_DECL

	public:
		FProcessor(const TSharedRef<PCGExData::FFacade>& InVtxDataFacade, const TSharedRef<PCGExData::FFacade>& InEdgeDataFacade)
			: TProcessor(InVtxDataFacade, InEdgeDataFacade)
		{
		}

		virtual ~FProcessor() override;

		virtual bool Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager) override;
		virtual void ProcessEdges(const PCGExMT::FScope& Scope) override;
		virtual void CompleteWork() override;
		virtual void Cleanup() override;
	};

	class FBatch final : public PCGExClusterMT::TBatch<FProcessor>
	{
		friend class FProcessor;

		FPCGExEdgeDirectionSettings DirectionSettings;

	public:
		FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, const TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges):
			TBatch(InContext, InVtx, InEdges)
		{
			bAllowVtxDataFacadeScopedGet = true;
		}

		virtual void RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) override;
		virtual void OnProcessingPreparationComplete() override;
	};
}

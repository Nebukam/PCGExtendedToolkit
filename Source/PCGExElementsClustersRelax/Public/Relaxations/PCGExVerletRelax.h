// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExRelaxClusterOperation.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Details/PCGExSettingsDetails.h"
#include "Elements/Meta/NeighborSamplers/PCGExNeighborSampleAttribute.h"
#include "PCGExVerletRelax.generated.h"

UENUM()
enum class EPCGExRelaxEdgeRestLength : uint8
{
	Fixed     = 0 UMETA(DisplayName = "Fixed", ToolTip="Aim for constant edge length while fitting"),
	Existing  = 1 UMETA(DisplayName = "Existing", ToolTip="Attempts to preserve existing edge length"),
	Attribute = 2 UMETA(DisplayName = "Attribute", ToolTip="Uses an attribute on the edges as target length"),
};

/**
 *
 */
UCLASS(MinimalAPI, meta=(DisplayName = "Verlet (Gravity)", PCGExNodeLibraryDoc="clusters/relax-cluster/Gravity"))
class UPCGExVerletRelax : public UPCGExRelaxClusterOperation
{
	GENERATED_BODY()

public:
	UPCGExVerletRelax(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
	}

	void RegisterPrimaryBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
	

	/** Type of Gravity */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType GravityInput = EPCGExInputValueType::Constant;

	/** Attribute to read weight value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Gravity (Attr)", EditCondition="GravityInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector GravityAttribute;

	/** Constant Gravity value. Think of it as gravity vector. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Gravity", EditCondition="GravityInput == EPCGExInputValueType::Constant", EditConditionHides))
	FVector Gravity = FVector(0, 0, -100);

	PCGEX_SETTING_VALUE_INLINE(Gravity, FVector, GravityInput, GravityAttribute, Gravity)

	/** Type of Friction */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType FrictionInput = EPCGExInputValueType::Constant;

	/** Attribute to read friction value from. Expected to be in the [0..1] range. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Friction (Attr)", EditCondition="FrictionInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector FrictionAttribute;

	/** Constant friction value. Expected to be in the [0..1] range. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Friction", EditCondition="FrictionInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0, ClampMax=1, UIMin=0, UIMax=1))
	double Friction = 0;

	PCGEX_SETTING_VALUE_INLINE(Friction, double, FrictionInput, FrictionAttribute, Friction)


	/** Type of Edge Scaling */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType EdgeScalingInput = EPCGExInputValueType::Constant;

	/** Attribute to read edge scaling value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Edge Scaling (Attr)", EditCondition="EdgeScalingInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector EdgeScalingAttribute;

	/** Constant Edge scaling value. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Edge Scaling", EditCondition="EdgeScalingInput == EPCGExInputValueType::Constant", EditConditionHides))
	double EdgeScaling = 1;

	PCGEX_SETTING_VALUE_INLINE(EdgeScaling, double, EdgeScalingInput, EdgeScalingAttribute, EdgeScaling)


	/** Type of Edge stiffness */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType EdgeStiffnessInput = EPCGExInputValueType::Constant;

	/** Attribute to read edge stiffness value from. Expected to be in the [0..1] range. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Edge Stiffness (Attr)", EditCondition="EdgeStiffnessInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector EdgeStiffnessAttribute;

	/** Constant Edge stiffness value. Expected to be in the [0..1] range. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Edge Stiffness", EditCondition="EdgeStiffnessInput == EPCGExInputValueType::Constant", EditConditionHides, ClampMin=0, ClampMax=1, UIMin=0, UIMax=1))
	double EdgeStiffness = 0.5;

	PCGEX_SETTING_VALUE_INLINE(EdgeStiffness, double, EdgeStiffnessInput, EdgeStiffnessAttribute, EdgeStiffness)

	/** If this was a physic simulation, represent the time advance each iteration */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	double TimeStep = 0.1;

	/** Velocity damping multiplier applied each iteration. Lower values = more damping, smoother convergence. Higher values retain momentum for more natural sag. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, ClampMin=0, ClampMax=1, UIMin=0, UIMax=1))
	double DampingScale = 0.99;

	virtual bool PrepareForCluster(FPCGExContext* InContext, const TSharedPtr<PCGExClusters::FCluster>& InCluster) override;
	virtual int32 GetNumSteps() override { return 3; }
	virtual EPCGExClusterElement PrepareNextStep(const int32 InStep) override;
	virtual void Step1(const PCGExClusters::FNode& Node) override;
	virtual void Step2(const PCGExGraphs::FEdge& Edge) override;
	virtual void Step3(const PCGExClusters::FNode& Node) override;

protected:
	TSharedPtr<TArray<double>> EdgeLengths;
	TSharedPtr<PCGExDetails::TSettingValue<FVector>> GravityBuffer;
	TSharedPtr<PCGExDetails::TSettingValue<double>> StiffnessBuffer;
	TSharedPtr<PCGExDetails::TSettingValue<double>> ScalingBuffer;
	TSharedPtr<PCGExDetails::TSettingValue<double>> FrictionBuffer;

	TArray<int8> Hits;
	TArray<FVector> HitLocations;
};

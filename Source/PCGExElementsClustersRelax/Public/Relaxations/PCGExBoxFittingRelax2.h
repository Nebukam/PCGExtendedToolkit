// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExFittingRelaxBase.h"
#include "Core/PCGExRelaxClusterOperation.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Details/PCGExSettingsDetails.h"
#include "Details/PCGExSettingsMacros.h"

#include "PCGExBoxFittingRelax2.generated.h"

UENUM()
enum class EPCGExBoxFittingSeparation : uint8
{
	MinimumPenetration = 0 UMETA(DisplayName = "Minimum Penetration", ToolTip="Separate along the axis with minimum overlap"),
	EdgeDirection      = 1 UMETA(DisplayName = "Edge Direction", ToolTip="Prefer separation along connected edge directions"),
	Centroid           = 2 UMETA(DisplayName = "Centroid", ToolTip="Separate directly away from each other's centers"),
};

/**
 * Relaxation using axis-aligned bounding boxes for collision detection.
 * More accurate than radius-based for rectangular or elongated objects.
 */
UCLASS(MinimalAPI, meta=(DisplayName = "Box Fitting v2", PCGExNodeLibraryDoc="clusters/relax-cluster/box-fitting"))
class UPCGExBoxFittingRelax2 : public UPCGExFittingRelaxBase
{
	GENERATED_BODY()

public:
	UPCGExBoxFittingRelax2(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		ExtentsAttribute.Update(TEXT("$Extents"));
	}

	virtual void RegisterPrimaryBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
	
	/** How extents are determined */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType ExtentsInput = EPCGExInputValueType::Attribute;

	/** Attribute to read extents value from. Expected to be half-size in each axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Extents (Attr)", EditCondition="ExtentsInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector ExtentsAttribute;

	/** Constant extents value. Half-size in each axis. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Extents", EditCondition="ExtentsInput == EPCGExInputValueType::Constant", EditConditionHides))
	FVector Extents = FVector(50, 50, 50);

	PCGEX_SETTING_VALUE_INLINE(Extents, FVector, ExtentsInput, ExtentsAttribute, Extents)

	/** How to determine separation direction when boxes overlap */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExBoxFittingSeparation SeparationMode = EPCGExBoxFittingSeparation::MinimumPenetration;

	/** Additional padding between boxes */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, ClampMin=0))
	double Padding = 0;

	/** Whether to consider rotation when computing bounds (more expensive) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bUseOrientedBounds = false;

	virtual bool PrepareForCluster(FPCGExContext* InContext, const TSharedPtr<PCGExClusters::FCluster>& InCluster) override;
	virtual void Step2(const PCGExClusters::FNode& Node) override;

protected:
	TSharedPtr<PCGExDetails::TSettingValue<FVector>> ExtentsBuffer;
};

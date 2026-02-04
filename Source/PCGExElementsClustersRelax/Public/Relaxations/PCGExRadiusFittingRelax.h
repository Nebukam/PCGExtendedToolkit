// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExBoxFittingRelax.h"
#include "Core/PCGExRelaxClusterOperation.h"
#include "Data/Utils/PCGExDataPreloader.h"
#include "Details/PCGExSettingsDetails.h"
#include "Details/PCGExSettingsMacros.h"

#include "PCGExRadiusFittingRelax.generated.h"

/**
 *
 */
UCLASS(MinimalAPI, meta=(DisplayName = "Radius Fitting", PCGExNodeLibraryDoc="clusters/relax-cluster/radius-fitting"))
class UPCGExRadiusFittingRelax : public UPCGExFittingRelaxBase
{
	GENERATED_BODY()

public:
	UPCGExRadiusFittingRelax(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
		RadiusAttribute.Update(TEXT("$Extents.Length"));
	}

	virtual void RegisterPrimaryBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override;
	
	/** Type of Velocity */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType RadiusInput = EPCGExInputValueType::Attribute;

	/** Attribute to read weight value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Radius (Attr)", EditCondition="RadiusInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector RadiusAttribute;

	/** Constant velocity value. Think of it as gravity vector. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Radius", EditCondition="RadiusInput == EPCGExInputValueType::Constant", EditConditionHides))
	double Radius = 100;

	PCGEX_SETTING_VALUE_INLINE(Radius, double, RadiusInput, RadiusAttribute, Radius)

	virtual bool PrepareForCluster(FPCGExContext* InContext, const TSharedPtr<PCGExClusters::FCluster>& InCluster) override;
	virtual void Step2(const PCGExClusters::FNode& Node) override;

protected:
	TSharedPtr<PCGExDetails::TSettingValue<double>> RadiusBuffer;
};

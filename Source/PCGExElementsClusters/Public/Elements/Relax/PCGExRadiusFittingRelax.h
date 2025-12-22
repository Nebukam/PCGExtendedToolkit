// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExBoxFittingRelax.h"
#include "PCGExRelaxClusterOperation.h"
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

	virtual void RegisterPrimaryBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override
	{
		Super::RegisterPrimaryBuffersDependencies(InContext, FacadePreloader);
		if (RadiusInput == EPCGExInputValueType::Attribute) { FacadePreloader.Register<double>(InContext, RadiusAttribute); }
	}

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

	virtual bool PrepareForCluster(FPCGExContext* InContext, const TSharedPtr<PCGExClusters::FCluster>& InCluster) override
	{
		if (!Super::PrepareForCluster(InContext, InCluster)) { return false; }

		RadiusBuffer = GetValueSettingRadius();
		if (!RadiusBuffer->Init(PrimaryDataFacade)) { return false; }

		return true;
	}

	virtual void Step2(const PCGExClusters::FNode& Node) override
	{
		const FVector& CurrentPos = (ReadBuffer->GetData() + Node.Index)->GetLocation();
		const double& CurrentRadius = RadiusBuffer->Read(Node.PointIndex);

		// Apply repulsion forces between all pairs of nodes

		for (int32 OtherNodeIndex = Node.Index + 1; OtherNodeIndex < Cluster->Nodes->Num(); OtherNodeIndex++)
		{
			const PCGExClusters::FNode* OtherNode = Cluster->GetNode(OtherNodeIndex);
			const FVector& OtherPos = (ReadBuffer->GetData() + OtherNodeIndex)->GetLocation();

			FVector Delta = OtherPos - CurrentPos;
			const double Distance = Delta.Size();
			const double Overlap = (CurrentRadius + RadiusBuffer->Read(OtherNode->PointIndex)) - Distance;

			if (Overlap <= 0 || Distance <= KINDA_SMALL_NUMBER) { continue; }

			AddDelta(OtherNode->Index, Node.Index, (RepulsionConstant * (Overlap / FMath::Square(Distance)) * (Delta / Distance)));
		}
	}

protected:
	TSharedPtr<PCGExDetails::TSettingValue<double>> RadiusBuffer;
};

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExBoxFittingRelax.h"
#include "PCGExRelaxClusterOperation.h"
#include "PCGExVelocityRelax.generated.h"

/**
 * 
 */
UCLASS(Hidden, MinimalAPI, meta=(DisplayName = "Velocity (Gravity)", PCGExNodeLibraryDoc="clusters/relax-cluster/velocity"))
class UPCGExVelocityRelax : public UPCGExFittingRelaxBase
{
	GENERATED_BODY()

public:
	UPCGExVelocityRelax(const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
	{
	}

	virtual void RegisterPrimaryBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const override
	{
		Super::RegisterPrimaryBuffersDependencies(InContext, FacadePreloader);
		if (VelocityInput == EPCGExInputValueType::Attribute) { FacadePreloader.Register<FVector>(InContext, VelocityAttribute); }
	}

	/** Type of Velocity */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType VelocityInput = EPCGExInputValueType::Constant;

	/** Attribute to read weight value from. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Velocity (Attr)", EditCondition="VelocityInput != EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector VelocityAttribute;

	/** Constant velocity value. Think of it as gravity vector. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Velocity", EditCondition="VelocityInput == EPCGExInputValueType::Constant", EditConditionHides))
	FVector Velocity = FVector::DownVector;

	PCGEX_SETTING_VALUE_GET(Velocity, FVector, VelocityInput, VelocityAttribute, Velocity)

	virtual bool PrepareForCluster(FPCGExContext* InContext, const TSharedPtr<PCGExCluster::FCluster>& InCluster) override
	{
		if (!Super::PrepareForCluster(InContext, InCluster)) { return false; }

		VelocityBuffer = GetValueSettingVelocity();
		if (!VelocityBuffer->Init(InContext, PrimaryDataFacade)) { return false; }

		return true;
	}

	virtual void Step2(const PCGExCluster::FNode& Node) override
	{
		const FVector& CurrentPos = (ReadBuffer->GetData() + Node.Index)->GetLocation();
		const FVector& DesiredVelocity = VelocityBuffer->Read(Node.PointIndex);

		/*
		// Apply repulsion forces between all pairs of nodes

		for (int32 OtherNodeIndex = Node.Index + 1; OtherNodeIndex < Cluster->Nodes->Num(); OtherNodeIndex++)
		{
			const PCGExCluster::FNode* OtherNode = Cluster->GetNode(OtherNodeIndex);
			const FVector& OtherPos = (ReadBuffer->GetData() + OtherNodeIndex)->GetLocation();

			FVector Delta = OtherPos - CurrentPos;
			const double Distance = Delta.Size();
			const double Overlap = (DesiredVelocity + RadiusBuffer->Read(OtherNode->PointIndex)) - Distance;

			if (Overlap <= 0 || Distance <= KINDA_SMALL_NUMBER) { continue; }

			ApplyForces(
				OtherNode->Index, Node.Index,
				(RepulsionConstant * (Overlap / FMath::Square(Distance)) * (Delta / Distance)) * Precision);
		}
		*/
	}

protected:
	TSharedPtr<PCGExDetails::TSettingValue<FVector>> VelocityBuffer;
};

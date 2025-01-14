// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExOperation.h"
#include "Graph/PCGExCluster.h"
#include "PCGExRelaxClusterOperation.generated.h"

namespace PCGExCluster
{
	struct FNode;
}

namespace PCGExCluster
{
	class FCluster;
}

/**
 * 
 */
UCLASS(Abstract)
class /*PCGEXTENDEDTOOLKIT_API*/ UPCGExRelaxClusterOperation : public UPCGExOperation
{
	GENERATED_BODY()

public:
	virtual void CopySettingsFrom(const UPCGExOperation* Other) override
	{
		Super::CopySettingsFrom(Other);
		//if (const UPCGExRelaxClusterOperation* TypedOther = Cast<UPCGExRelaxClusterOperation>(Other))		{		}
	}

	virtual bool PrepareForCluster(const TSharedPtr<PCGExCluster::FCluster>& InCluster)
	{
		Cluster = InCluster;
		return true;
	}

	virtual int32 GetNumSteps() { return 1; }
	virtual EPCGExClusterComponentSource GetStepSource(const int32 InStep) { return EPCGExClusterComponentSource::Vtx; }


	virtual EPCGExClusterComponentSource PrepareNextStep(const int32 InStep)
	{
		if (InStep == 0) { std::swap(ReadBuffer, WriteBuffer); }
		return EPCGExClusterComponentSource::Vtx;
	}

	// Node steps

	virtual void Step1(const PCGExCluster::FNode& Node)
	{
	}

	virtual void Step2(const PCGExCluster::FNode& Node)
	{
	}

	virtual void Step3(const PCGExCluster::FNode& Node)
	{
	}

	// Edge steps

	virtual void Step1(const PCGExGraph::FEdge& Edge)
	{
	}

	virtual void Step2(const PCGExGraph::FEdge& Edge)
	{
	}

	virtual void Step3(const PCGExGraph::FEdge& Edge)
	{
	}

	TSharedPtr<PCGExCluster::FCluster> Cluster;
	TArray<FTransform>* ReadBuffer = nullptr;
	TArray<FTransform>* WriteBuffer = nullptr;

	virtual void Cleanup() override
	{
		Cluster = nullptr;
		ReadBuffer = nullptr;
		WriteBuffer = nullptr;

		Super::Cleanup();
	}
};

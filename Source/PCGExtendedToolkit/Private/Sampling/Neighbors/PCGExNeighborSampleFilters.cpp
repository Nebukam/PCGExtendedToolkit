// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/Neighbors/PCGExNeighborSampleFilters.h"

#include "Data/Blending/PCGExMetadataBlender.h"


#define LOCTEXT_NAMESPACE "PCGExCreateNeighborSample"
#define PCGEX_NAMESPACE PCGExCreateNeighborSample

void UPCGExNeighborSampleFilters::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExNeighborSampleFilters* TypedOther = Cast<UPCGExNeighborSampleFilters>(Other))
	{
		// TODO
	}
}

void UPCGExNeighborSampleFilters::PrepareForCluster(FPCGExContext* InContext, const TSharedRef<PCGExCluster::FCluster> InCluster, const TSharedRef<PCGExData::FFacade> InVtxDataFacade, const TSharedRef<PCGExData::FFacade> InEdgeDataFacade)
{
	Super::PrepareForCluster(InContext, InCluster, InVtxDataFacade, InEdgeDataFacade);
	PointFilters.Reset();

	bIsValidOperation = false;

	FilterManager = MakeShared<PCGExClusterFilter::FManager>(InCluster, InVtxDataFacade, InEdgeDataFacade);


	const int32 NumNodes = Cluster->Nodes->Num();
	Inside.Init(0, NumNodes);
	InsideWeight.Init(0, NumNodes);
	Outside.Init(0, NumNodes);
	OutsideWeight.Init(0, NumNodes);

	if (Config.bWriteInsideNum)
	{
		if (!Config.bNormalizeInsideNum) { NumInsideBuffer = VtxDataFacade->GetWritable<int32>(Config.InsideNumAttributeName, 0, true, PCGExData::EBufferInit::New); }
		else { NormalizedNumInsideBuffer = VtxDataFacade->GetWritable<double>(Config.InsideNumAttributeName, 0, true, PCGExData::EBufferInit::New); }
	}

	if (Config.bWriteOutsideNum)
	{
		if (!Config.bNormalizeInsideNum) { NumOutsideBuffer = VtxDataFacade->GetWritable<int32>(Config.OutsideNumAttributeName, 0, true, PCGExData::EBufferInit::New); }
		else { NormalizedNumOutsideBuffer = VtxDataFacade->GetWritable<double>(Config.OutsideNumAttributeName, 0, true, PCGExData::EBufferInit::New); }
	}

	if (Config.bWriteTotalNum)
	{
		TotalNumBuffer = VtxDataFacade->GetWritable<int32>(Config.TotalNumAttributeName, 0, true, PCGExData::EBufferInit::New);
	}

	if (Config.bWriteInsideWeight)
	{
		if (!Config.bNormalizeInsideWeight) { WeightInsideBuffer = VtxDataFacade->GetWritable<double>(Config.InsideWeightAttributeName, 0, true, PCGExData::EBufferInit::New); }
		else { NormalizedWeightInsideBuffer = VtxDataFacade->GetWritable<double>(Config.InsideWeightAttributeName, 0, true, PCGExData::EBufferInit::New); }
	}

	if (Config.bWriteOutsideWeight)
	{
		if (!Config.bNormalizeOutsideWeight) { WeightOutsideBuffer = VtxDataFacade->GetWritable<double>(Config.OutsideWeightAttributeName, 0, true, PCGExData::EBufferInit::New); }
		else { NormalizedWeightOutsideBuffer = VtxDataFacade->GetWritable<double>(Config.OutsideWeightAttributeName, 0, true, PCGExData::EBufferInit::New); }
	}

	if (Config.bWriteTotalWeight)
	{
		TotalWeightBuffer = VtxDataFacade->GetWritable<int32>(Config.TotalWeightAttributeName, 0, true, PCGExData::EBufferInit::New);
	}

	if (SamplingConfig.NeighborSource == EPCGExClusterComponentSource::Vtx)
	{
		if (!FilterManager->Init(InContext, VtxFilterFactories))
		{
			return;
		}
	}
	else
	{
		FilterManager->bUseEdgeAsPrimary = true;
		if (!FilterManager->Init(InContext, EdgesFilterFactories))
		{
			return;
		}
	}

	bIsValidOperation = true;
}

void UPCGExNeighborSampleFilters::CompleteOperation()
{
	Super::CompleteOperation();
	Inside.Empty();
	Outside.Empty();
	FilterManager.Reset();
}

void UPCGExNeighborSampleFilters::Cleanup()
{
	FilterManager.Reset();
	Super::Cleanup();
}

UPCGExNeighborSampleOperation* UPCGExNeighborSamplerFactoryFilters::CreateOperation(FPCGExContext* InContext) const
{
	UPCGExNeighborSampleFilters* NewOperation = InContext->ManagedObjects->New<UPCGExNeighborSampleFilters>();
	PCGEX_SAMPLER_CREATE_OPERATION
	NewOperation->Config = Config;
	return NewOperation;
}

UPCGExNeighborSampleFiltersSettings::UPCGExNeighborSampleFiltersSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SamplingConfig.bSupportsBlending = false;
}

bool UPCGExNeighborSampleFiltersSettings::SupportsVtxFilters(bool& bIsRequired) const
{
	bIsRequired = true;
	return SamplingConfig.NeighborSource == EPCGExClusterComponentSource::Vtx;
}

bool UPCGExNeighborSampleFiltersSettings::SupportsEdgeFilters(bool& bIsRequired) const
{
	bIsRequired = true;
	return SamplingConfig.NeighborSource == EPCGExClusterComponentSource::Edge;
}

UPCGExFactoryData* UPCGExNeighborSampleFiltersSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	if (Config.bWriteInsideNum) { PCGEX_VALIDATE_NAME_C_RET(InContext, Config.InsideNumAttributeName, nullptr) }
	if (Config.bWriteOutsideNum) { PCGEX_VALIDATE_NAME_C_RET(InContext, Config.OutsideNumAttributeName, nullptr) }
	if (Config.bWriteTotalNum) { PCGEX_VALIDATE_NAME_C_RET(InContext, Config.TotalNumAttributeName, nullptr) }
	if (Config.bWriteInsideWeight) { PCGEX_VALIDATE_NAME_C_RET(InContext, Config.InsideWeightAttributeName, nullptr) }
	if (Config.bWriteOutsideWeight) { PCGEX_VALIDATE_NAME_C_RET(InContext, Config.OutsideWeightAttributeName, nullptr) }
	if (Config.bWriteTotalWeight) { PCGEX_VALIDATE_NAME_C_RET(InContext, Config.TotalWeightAttributeName, nullptr) }

	UPCGExNeighborSamplerFactoryFilters* SamplerFactory = InContext->ManagedObjects->New<UPCGExNeighborSamplerFactoryFilters>();
	SamplerFactory->Config = Config;

	return Super::CreateFactory(InContext, SamplerFactory);
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

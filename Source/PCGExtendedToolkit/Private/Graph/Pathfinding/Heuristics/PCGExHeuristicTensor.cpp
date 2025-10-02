﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicTensor.h"


#include "Transform/Tensors/PCGExTensor.h"
#include "Transform/Tensors/PCGExTensorHandler.h"
#include "Transform/Tensors/PCGExTensorFactoryProvider.h"


void FPCGExHeuristicTensor::PrepareForCluster(const TSharedPtr<const PCGExCluster::FCluster>& InCluster)
{
	FPCGExHeuristicOperation::PrepareForCluster(InCluster);
	TensorsHandler = MakeShared<PCGExTensor::FTensorsHandler>(TensorHandlerDetails);
	TensorsHandler->Init(Context, *TensorFactories, PrimaryDataFacade);
}

double FPCGExHeuristicTensor::GetGlobalScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal) const
{
	return GetScoreInternal(GetDot(From.PointIndex, Cluster->GetPos(From), Cluster->GetPos(Goal)));
}

double FPCGExHeuristicTensor::GetEdgeScore(
	const PCGExCluster::FNode& From,
	const PCGExCluster::FNode& To,
	const PCGExGraph::FEdge& Edge,
	const PCGExCluster::FNode& Seed,
	const PCGExCluster::FNode& Goal,
	const TSharedPtr<PCGEx::FHashLookup> TravelStack) const
{
	return GetScoreInternal(GetDot(From.PointIndex, Cluster->GetPos(From), Cluster->GetPos(To)));
}

double FPCGExHeuristicTensor::GetDot(const int32 InSeedIndex, const FVector& From, const FVector& To) const
{
	bool bSuccess = false;
	const PCGExTensor::FTensorSample Sample = TensorsHandler->Sample(InSeedIndex, FTransform(FRotationMatrix::MakeFromX((To - From).GetSafeNormal()).ToQuat(), From), bSuccess);
	if (!bSuccess) { return 0; }
	const double Dot = FVector::DotProduct((To - From).GetSafeNormal(), Sample.DirectionAndSize.GetSafeNormal());
	return bAbsoluteTensor ? 1 - FMath::Abs(Dot) : 1 - PCGExMath::Remap(Dot, -1, 1);
}

TSharedPtr<FPCGExHeuristicOperation> UPCGExHeuristicsFactoryTensor::CreateOperation(FPCGExContext* InContext) const
{
	PCGEX_FACTORY_NEW_OPERATION(HeuristicTensor)
	PCGEX_FORWARD_HEURISTIC_CONFIG
	NewOperation->bAbsoluteTensor = Config.bAbsolute;
	NewOperation->TensorHandlerDetails = Config.TensorHandlerDetails;
	NewOperation->TensorFactories = &TensorFactories;
	return NewOperation;
}

PCGEX_HEURISTIC_FACTORY_BOILERPLATE_IMPL(Tensor, {})

PCGExFactories::EPreparationResult UPCGExHeuristicsFactoryTensor::Prepare(FPCGExContext* InContext, const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
{
	PCGExFactories::EPreparationResult Result = Super::Prepare(InContext, AsyncManager);
	if (Result != PCGExFactories::EPreparationResult::Success) { return Result; }

	if (!PCGExFactories::GetInputFactories(InContext, PCGExTensor::SourceTensorsLabel, TensorFactories, {PCGExFactories::EType::Tensor}, true)) { return PCGExFactories::EPreparationResult::Fail; }
	if (TensorFactories.IsEmpty())
	{
		PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Missing tensors."));
		return PCGExFactories::EPreparationResult::Fail;
	}
	return Result;
}

TArray<FPCGPinProperties> UPCGExHeuristicsTensorProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExTensor::SourceTensorsLabel, "Tensors fields to influence search", Required, FPCGExDataTypeInfoTensor::AsId())
	return PinProperties;
}

UPCGExFactoryData* UPCGExHeuristicsTensorProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	UPCGExHeuristicsFactoryTensor* NewFactory = InContext->ManagedObjects->New<UPCGExHeuristicsFactoryTensor>();
	PCGEX_FORWARD_HEURISTIC_FACTORY
	return Super::CreateFactory(InContext, NewFactory);
}

#if WITH_EDITOR
FString UPCGExHeuristicsTensorProviderSettings::GetDisplayName() const
{
	return GetDefaultNodeTitle().ToString().Replace(TEXT("PCGEx | Heuristics"), TEXT("HX"))
		+ TEXT(" @ ")
		+ FString::Printf(TEXT("%.3f"), (static_cast<int32>(1000 * Config.WeightFactor) / 1000.0));
}
#endif

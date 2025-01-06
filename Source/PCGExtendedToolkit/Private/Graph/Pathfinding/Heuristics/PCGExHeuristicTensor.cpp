// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Graph/Pathfinding/Heuristics/PCGExHeuristicTensor.h"

#include "Transform/Tensors/PCGExTensor.h"


void UPCGExHeuristicTensor::PrepareForCluster(const TSharedPtr<const PCGExCluster::FCluster>& InCluster)
{
	UpwardVector = UpwardVector.GetSafeNormal();
	Super::PrepareForCluster(InCluster);
}

UPCGExHeuristicOperation* UPCGExHeuristicsFactoryTensor::CreateOperation(FPCGExContext* InContext) const
{
	UPCGExHeuristicTensor* NewOperation = InContext->ManagedObjects->New<UPCGExHeuristicTensor>();
	PCGEX_FORWARD_HEURISTIC_CONFIG
	return NewOperation;
}

PCGEX_HEURISTIC_FACTORY_BOILERPLATE_IMPL(Tensor, {})

TArray<FPCGPinProperties> UPCGExHeuristicsTensorProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties =  Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExTensor::SourceTensorsLabel, "Tensors fields to influence search", Required, {})
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

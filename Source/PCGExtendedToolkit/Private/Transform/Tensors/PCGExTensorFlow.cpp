// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensorFlow.h"




#define LOCTEXT_NAMESPACE "PCGExCreateTensorFlow"
#define PCGEX_NAMESPACE CreateTensorFlow

bool FPCGExTensorFlow::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!PCGExTensorPointOperation::Init(InContext, InFactory)) { return false; }
	return true;
}

PCGExTensor::FTensorSample FPCGExTensorFlow::Sample(const int32 InSeedIndex, const FTransform& InProbe) const
{
	const FVector& InPosition = InProbe.GetLocation();
	const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(InPosition, FVector::One());

	PCGExTensor::FEffectorSamples Samples = PCGExTensor::FEffectorSamples();

	auto ProcessNeighbor = [&](const FPCGPointRef& InEffector)
	{
		PCGExTensor::FEffectorMetrics Metrics;
		if (!ComputeFactor(InPosition, InEffector, Metrics)) { return; }

		Samples.Emplace_GetRef(
			InEffector.Point->Transform.GetRotation().RotateVector(Metrics.Guide),
			Metrics.Potency, Metrics.Weight);
	};

	Octree->FindElementsWithBoundsTest(BCAE, ProcessNeighbor);

	return Config.Mutations.Mutate(InProbe, Samples.Flatten(Config.TensorWeight));
}

PCGEX_TENSOR_BOILERPLATE(Flow, {}, {})

bool UPCGExTensorFlowFactory::InitInternalData(FPCGExContext* InContext)
{
	if (!Super::InitInternalData(InContext)) { return false; }
	DirectionBuffer.Reset();
	return true;
}

bool UPCGExTensorFlowFactory::InitInternalFacade(FPCGExContext* InContext)
{
	if (!Super::InitInternalFacade(InContext)) { return false; }

	if (Config.DirectionInput == EPCGExInputValueType::Attribute)
	{
		DirectionBuffer = InputDataFacade->GetBroadcaster<FVector>(Config.DirectionAttribute);
		if (!DirectionBuffer)
		{
			PCGEX_LOG_INVALID_SELECTOR_C(InContext, "Direction", Config.DirectionAttribute)
			return false;
		}
	}

	return true;
}

void UPCGExTensorFlowFactory::PrepareSinglePoint(const int32 Index, FPCGPoint& InPoint) const
{
	Super::PrepareSinglePoint(Index, InPoint);

	// Force forward-facing transform
	// As that's the direction we use during tensor sampling
	// This is so tensor transform is "cached" into the point at the time of tensor creation
	// instead of recomputing this every time the tensor is used

	// Sampling tensors is already rather expensive as-is

	if (DirectionBuffer)
	{
		if (Config.DirectionTransform == EPCGExTransformMode::Absolute)
		{
			InPoint.Transform.SetRotation(PCGExMath::MakeDirection(EPCGExAxis::Forward, DirectionBuffer->Read(Index)));
		}
		else
		{
			InPoint.Transform.SetRotation(PCGExMath::MakeDirection(EPCGExAxis::Forward, InPoint.Transform.TransformVectorNoScale(DirectionBuffer->Read(Index))));
		}
	}
	else if (Config.DirectionConstant != EPCGExAxis::Forward)
	{
		InPoint.Transform.SetRotation(PCGExMath::MakeDirection(EPCGExAxis::Forward, PCGExMath::GetDirection(InPoint.Transform.GetRotation(), Config.DirectionConstant)));
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

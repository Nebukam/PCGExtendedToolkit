// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensorSpin.h"

#define LOCTEXT_NAMESPACE "PCGExCreateTensorSpin"
#define PCGEX_NAMESPACE CreateTensorSpin

bool UPCGExTensorSpin::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!Super::Init(InContext, InFactory)) { return false; }
	return true;
}

PCGExTensor::FTensorSample UPCGExTensorSpin::Sample(const FTransform& InProbe) const
{
	const FVector& InPosition = InProbe.GetLocation();
	const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(InPosition, FVector::One());

	PCGExTensor::FEffectorSamples Samples = PCGExTensor::FEffectorSamples();

	auto ProcessNeighbor = [&](const FPCGPointRef& InEffector)
	{
		PCGExTensor::FEffectorMetrics Metrics;
		if (!ComputeFactor(InPosition, InEffector, Metrics)) { return; }

		Samples.Emplace_GetRef(
			FVector::CrossProduct(
				(InEffector.Point->Transform.GetLocation() - InPosition).GetSafeNormal(),
				InEffector.Point->Transform.GetRotation().RotateVector(Metrics.Guide)).GetSafeNormal(),
			InEffector.Point->Steepness * Config.PotencyFalloffCurveObj->Eval(Metrics.Factor),
			InEffector.Point->Density * Config.WeightFalloffCurveObj->Eval(Metrics.Factor));
	};

	Octree->FindElementsWithBoundsTest(BCAE, ProcessNeighbor);

	return Config.Mutations.Mutate(InProbe, Samples.Flatten(Config.TensorWeight));
}

PCGEX_TENSOR_BOILERPLATE(Spin, {}, {})

bool UPCGExTensorSpinFactory::InitInternalData(FPCGExContext* InContext)
{
	if (!Super::InitInternalData(InContext)) { return false; }
	AxisBuffer.Reset();
	return true;
}

bool UPCGExTensorSpinFactory::InitInternalFacade(FPCGExContext* InContext)
{
	if (!Super::InitInternalFacade(InContext)) { return false; }

	if (Config.AxisInput == EPCGExInputValueType::Attribute)
	{
		AxisBuffer = InputDataFacade->GetBroadcaster<FVector>(Config.AxisAttribute);
		if (!AxisBuffer)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid Direction attribute: \"{0}\"."), FText::FromName(Config.AxisAttribute.GetName())));
			return false;
		}
	}

	return true;
}

void UPCGExTensorSpinFactory::PrepareSinglePoint(const int32 Index, FPCGPoint& InPoint) const
{
	Super::PrepareSinglePoint(Index, InPoint);

	// Force forward-facing transform
	// As that's the direction we use during tensor sampling
	// This is so tensor transform is "cached" into the point at the time of tensor creation
	// instead of recomputing this every time the tensor is used

	// Sampling tensors is already rather expensive as-is

	if (AxisBuffer)
	{
		if (Config.AxisTransform == EPCGExTransformMode::Absolute)
		{
			InPoint.Transform.SetRotation(PCGExMath::MakeDirection(EPCGExAxis::Forward, AxisBuffer->Read(Index)));
		}
		else
		{
			InPoint.Transform.SetRotation(PCGExMath::MakeDirection(EPCGExAxis::Forward, InPoint.Transform.TransformVectorNoScale(AxisBuffer->Read(Index))));
		}
	}
	else if (Config.AxisConstant != EPCGExAxis::Forward)
	{
		InPoint.Transform.SetRotation(PCGExMath::MakeDirection(EPCGExAxis::Forward, PCGExMath::GetDirection(InPoint.Transform.GetRotation(), Config.AxisConstant)));
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

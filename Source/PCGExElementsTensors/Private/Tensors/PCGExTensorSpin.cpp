// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Tensors/PCGExTensorSpin.h"


#include "Containers/PCGExManagedObjects.h"
#include "Data/PCGExData.h"


#define LOCTEXT_NAMESPACE "PCGExCreateTensorSpin"
#define PCGEX_NAMESPACE CreateTensorSpin

namespace PCGExTensor
{
	bool FSpinEffectorsArray::Init(FPCGExContext* InContext, const UPCGExTensorPointFactoryData* InFactory)
	{
		const UPCGExTensorSpinFactory* SpinFactory = Cast<UPCGExTensorSpinFactory>(InFactory);
		Config = SpinFactory->Config;

		if (Config.AxisInput == EPCGExInputValueType::Attribute)
		{
			AxisBuffer = InFactory->InputDataFacade->GetBroadcaster<FVector>(Config.AxisAttribute);
			if (!AxisBuffer)
			{
				PCGEX_LOG_INVALID_SELECTOR_C(InContext, Axis, Config.AxisAttribute)
				return false;
			}
		}

		if (!FEffectorsArray::Init(InContext, InFactory)) { return false; }

		AxisBuffer.Reset();

		return true;
	}

	void FSpinEffectorsArray::PrepareSinglePoint(const int32 Index)
	{
		// Force forward-facing transform
		// As that's the direction we use during tensor sampling
		// This is so tensor transform is "cached" into the point at the time of tensor creation
		// instead of recomputing this every time the tensor is used

		// Sampling tensors is already rather expensive as-is

		if (AxisBuffer)
		{
			if (Config.AxisTransform == EPCGExTransformMode::Absolute)
			{
				Transforms[Index].SetRotation(PCGExMath::MakeDirection(EPCGExAxis::Forward, AxisBuffer->Read(Index)));
			}
			else
			{
				Transforms[Index].SetRotation(PCGExMath::MakeDirection(EPCGExAxis::Forward, Transforms[Index].TransformVectorNoScale(AxisBuffer->Read(Index))));
			}
		}
		else if (Config.AxisConstant != EPCGExAxis::Forward)
		{
			Transforms[Index].SetRotation(PCGExMath::MakeDirection(EPCGExAxis::Forward, PCGExMath::GetDirection(Transforms[Index].GetRotation(), Config.AxisConstant)));
		}
	}
}

bool FPCGExTensorSpin::Init(FPCGExContext* InContext, const UPCGExTensorFactoryData* InFactory)
{
	if (!PCGExTensorPointOperation::Init(InContext, InFactory)) { return false; }
	return true;
}

PCGExTensor::FTensorSample FPCGExTensorSpin::Sample(const int32 InSeedIndex, const FTransform& InProbe) const
{
	const FVector& InPosition = InProbe.GetLocation();
	const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(InPosition, FVector::One());

	PCGExTensor::FEffectorSamples Samples = PCGExTensor::FEffectorSamples();

	auto ProcessNeighbor = [&](const PCGExOctree::FItem& InItem)
	{
		PCGExTensor::FEffectorMetrics Metrics;
		if (!ComputeFactor(InPosition, InItem.Index, Metrics)) { return; }

		const FTransform& Transform = Effectors->ReadTransform(InItem.Index);
		Samples.Emplace_GetRef(
			FVector::CrossProduct((Transform.GetLocation() - InPosition).GetSafeNormal(), Transform.GetRotation().RotateVector(Metrics.Guide)).GetSafeNormal(),
			Effectors->ReadPotency(InItem.Index) * PotencyFalloffLUT->Eval(Metrics.Factor),
			Effectors->ReadWeight(InItem.Index) * WeightFalloffLUT->Eval(Metrics.Factor));
	};

	Effectors->GetOctree()->FindElementsWithBoundsTest(BCAE, ProcessNeighbor);

	return Config.Mutations.Mutate(InProbe, Samples.Flatten(Config.TensorWeight));
}

PCGEX_TENSOR_BOILERPLATE(Spin, {}, {})

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

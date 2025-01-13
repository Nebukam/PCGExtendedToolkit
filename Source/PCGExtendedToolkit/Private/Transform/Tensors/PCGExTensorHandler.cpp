// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensorHandler.h"

#include "Transform/Tensors/PCGExTensor.h"
#include "Transform/Tensors/PCGExTensorFactoryProvider.h"
#include "Transform/Tensors/PCGExTensorOperation.h"

namespace PCGExTensor
{
	FTensorsHandler::FTensorsHandler(const FPCGExTensorHandlerDetails& InConfig)
		: Config(InConfig)
	{
	}

	bool FTensorsHandler::Init(FPCGExContext* InContext, const TArray<TObjectPtr<const UPCGExTensorFactoryData>>& InFactories, const TSharedPtr<PCGExData::FFacade>& InDataFacade)
	{
		Tensors.Reserve(InFactories.Num());

		for (const UPCGExTensorFactoryData* Factory : InFactories)
		{
			UPCGExTensorOperation* Op = Factory->CreateOperation(InContext);
			Tensors.Add(Op);
		}

		if (Config.SamplerSettings.Sampler) { SamplerInstance = InContext->ManagedObjects->New<UPCGExTensorSampler>(GetTransientPackage(), Config.SamplerSettings.Sampler); }
		if (!SamplerInstance) { SamplerInstance = InContext->ManagedObjects->New<UPCGExTensorSampler>(); }

		if (!SamplerInstance) { return false; }

		SamplerInstance->BindContext(InContext);
		SamplerInstance->PrimaryDataFacade = InDataFacade;

		return SamplerInstance->PrepareForData(InContext);
	}

	bool FTensorsHandler::Init(FPCGExContext* InContext, const FName InPin, const TSharedPtr<PCGExData::FFacade>& InDataFacade)
	{
		TArray<TObjectPtr<const UPCGExTensorFactoryData>> InFactories;
		if (!PCGExFactories::GetInputFactories(InContext, InPin, InFactories, {PCGExFactories::EType::Tensor}, true)) { return false; }
		if (InFactories.IsEmpty())
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Missing tensors."));
			return false;
		}
		return Init(InContext, InFactories, InDataFacade);
	}

	FTensorSample FTensorsHandler::Sample(const FTransform& InProbe, bool& OutSuccess) const
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FTensorsHandler::Sample);

		check(SamplerInstance)

		FTensorSample Result = SamplerInstance->Sample(Tensors, InProbe, OutSuccess);

		if (Config.bNormalize)
		{
			Result.DirectionAndSize = Result.DirectionAndSize.GetSafeNormal() * Config.SizeConstant;
		}

		if (Config.bInvert)
		{
			Result.DirectionAndSize *= -1;
			Result.Rotation = FQuat(-Result.Rotation.X, -Result.Rotation.Y, -Result.Rotation.Y, Result.Rotation.W);
		}

		if (Config.bBidirectional)
		{
			if (FVector::DotProduct(
				PCGExMath::GetDirection(InProbe.GetRotation(), Config.BidirectionalAxisReference),
				Result.DirectionAndSize.GetSafeNormal()) < 0)
			{
				Result.DirectionAndSize = Result.DirectionAndSize * -1;
				Result.Rotation = FQuat(-Result.Rotation.X, -Result.Rotation.Y, -Result.Rotation.Y, Result.Rotation.W);
			}
		}

		Result.DirectionAndSize *= Config.UniformScale;

		return Result;
	}
}

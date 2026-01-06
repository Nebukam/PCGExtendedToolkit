// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Elements/PCGExUberNoise.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExProxyData.h"
#include "Data/PCGExProxyDataHelpers.h"
#include "PCGExVersion.h"
#include "Async/ParallelFor.h"
#include "Containers/PCGExScopedContainers.h"
#include "Core/PCGExNoise3DFactoryProvider.h"
#include "Data/PCGExSubSelectionOps.h"
#include "Helpers/PCGExArrayHelpers.h"
#include "Helpers/PCGExNoiseGenerator.h"


#define LOCTEXT_NAMESPACE "PCGExUberNoise"
#define PCGEX_NAMESPACE UberNoise

#if WITH_EDITOR
FString UPCGExUberNoiseSettings::GetDisplayName() const
{
	if (Attributes.WantsRemappedOutput())
	{
		return TEXT("UN : ") + Attributes.Source.ToString() + TEXT(" → ") + Attributes.Target.ToString();
	}

	return TEXT("Uber Noise : ") + Attributes.Source.ToString();
}
#endif

PCGEX_INITIALIZE_ELEMENT(UberNoise)

TArray<FPCGPinProperties> UPCGExUberNoiseSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExNoise3D::Labels::SourceNoise3DLabel, "Noises", Required, FPCGExDataTypeInfoNoise3D::AsId())
	return PinProperties;
}

PCGExData::EIOInit UPCGExUberNoiseSettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(UberNoise)

bool FPCGExUberNoiseElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(UberNoise)

	if (!Settings->Attributes.ValidateNamesOrProperties(Context)) { return false; }

	Context->NoiseGenerator = MakeShared<PCGExNoise3D::FNoiseGenerator>();
	if (!Context->NoiseGenerator->Init(Context)) { return false; }

	Context->NoiseGenerator->SetInPlaceMode(Settings->NoiseInPlaceMode, Settings->NoiseBlendMode);

	return true;
}

bool FPCGExUberNoiseElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExUberNoiseElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(UberNoise)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				NewBatch->bSkipCompletion = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any data to add noise to."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExUberNoise
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		if (Settings->Mode == EPCGExUberNoiseMode::New)
		{
			PCGExData::FProxyDescriptor OutputDescriptor;
			OutputDescriptor.DataFacade = PointDataFacade;
			OutputDescriptor.Role = PCGExData::EProxyRole::Write;


			// TODO : Just create a raw buffer with as many dimensions as needed
			switch (Settings->Dimensions)
			{
			case 1: OutputDescriptor.RealType = EPCGMetadataTypes::Double;
				break;
			case 2: OutputDescriptor.RealType = EPCGMetadataTypes::Vector2;
				break;
			case 3: OutputDescriptor.RealType = EPCGMetadataTypes::Vector;
				break;
			case 4: OutputDescriptor.RealType = EPCGMetadataTypes::Vector4;
				break;
			}

			if (!OutputDescriptor.Capture(Context, Settings->Attributes.GetTargetSelector(), PCGExData::EIOSide::Out))
			{
				// TODO : Log error
				return false;
			}

			NewOutProxy = PCGExData::GetProxyBuffer(Context, OutputDescriptor);
			if (!NewOutProxy)
			{
				// TODO : Log error
				return false;
			}
		}
		else
		{
			TArray<TSharedPtr<PCGExData::IBufferProxy>> UntypedInputProxies;
			TArray<TSharedPtr<PCGExData::IBufferProxy>> UntypedOutputProxies;

			PCGExData::FProxyDescriptor InputDescriptor;
			PCGExData::FProxyDescriptor OutputDescriptor;

			InputDescriptor.DataFacade = PointDataFacade;
			OutputDescriptor.DataFacade = PointDataFacade;
			OutputDescriptor.Role = PCGExData::EProxyRole::Write;

			if (!InputDescriptor.Capture(Context, Settings->Attributes.GetSourceSelector(), PCGExData::EIOSide::In)) { return false; }

			// Number of dimensions to be remapped
			UnderlyingType = InputDescriptor.WorkingType;
			Dimensions = FMath::Min(4, PCGExData::FSubSelectorRegistry::Get(UnderlyingType)->GetNumFields());

			// Get per-field proxies for input
			if (!GetPerFieldProxyBuffers(Context, InputDescriptor, Dimensions, UntypedInputProxies)) { return false; }

			if (!OutputDescriptor.CaptureStrict(Context, Settings->Attributes.GetTargetSelector(), PCGExData::EIOSide::Out, false))
			{
				// This might be expected if the destination does not exist

				if (Dimensions == 1 && Settings->Attributes.WantsRemappedOutput() && !OutputDescriptor.SubSelection.bIsValid)
				{
					// We're remapping a component to a single value with no subselection
					OutputDescriptor.RealType = InputDescriptor.WorkingType;
				}
				else
				{
					// We're remapping to a component within the same larger type
					OutputDescriptor.RealType = InputDescriptor.RealType;
				}

				if (Settings->bAutoCastIntegerToDouble && (OutputDescriptor.RealType == EPCGMetadataTypes::Integer32 || OutputDescriptor.RealType == EPCGMetadataTypes::Integer64))
				{
					OutputDescriptor.RealType = EPCGMetadataTypes::Double;
				}

				OutputDescriptor.WorkingType = InputDescriptor.WorkingType;
			}
			else
			{
				// TODO : Grab default type for attribute if it cannot be inferred
				// GetPerFieldProxyBuffers expect a valid RealType to work from
			}

			// Get per-field proxies for output
			if (!GetPerFieldProxyBuffers(Context, OutputDescriptor, Dimensions, UntypedOutputProxies)) { return false; }

			for (int i = 0; i < Dimensions; i++)
			{
				TSharedPtr<PCGExData::IBufferProxy> InProxy = UntypedInputProxies[i];
				TSharedPtr<PCGExData::IBufferProxy> OutProxy = UntypedOutputProxies[i];

				if (InProxy->WorkingType != EPCGMetadataTypes::Double)
				{
					// TODO : Some additional validation, just making sure we can safely cast those
				}

				if (OutProxy->WorkingType != EPCGMetadataTypes::Double)
				{
					// TODO : Some additional validation, just making sure we can safely cast those
				}

				InputProxies.Add(InProxy);
				OutputProxies.Add(OutProxy);
			}
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExUberNoise::Fetch);

		PointDataFacade->Fetch(Scope);

		TConstPCGValueRange<FTransform> InTransforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		const PCGExNoise3D::FNoiseGenerator* NoiseGenerator = Context->NoiseGenerator.Get();
		const int32 S = Scope.Start;

		if (Settings->Mode == EPCGExUberNoiseMode::New)
		{
			// TODO
			switch (Settings->Dimensions)
			{
			case 1: PCGEX_SCOPE_LOOP(Index) { NewOutProxy->Set(Index, NoiseGenerator->GetDouble(InTransforms[Index].GetLocation())); }
				break;
			case 2: PCGEX_SCOPE_LOOP(Index) { NewOutProxy->Set(Index, NoiseGenerator->GetVector2D(InTransforms[Index].GetLocation())); }
				break;
			case 3: PCGEX_SCOPE_LOOP(Index) { NewOutProxy->Set(Index, NoiseGenerator->GetVector(InTransforms[Index].GetLocation())); }
				break;
			case 4: PCGEX_SCOPE_LOOP(Index) { NewOutProxy->Set(Index, NoiseGenerator->GetVector4(InTransforms[Index].GetLocation())); }
				break;
			}
		}
		else
		{
			TArray<FVector> Positions;
			PCGExArrayHelpers::InitArray(Positions, Scope.Count);
			for (int i = 0; i < Scope.Count; i++) { Positions[i] = InTransforms[i + S].GetLocation(); }

			TArray<FVector4> Values;
			Values.Init(FVector::Zero(), Scope.Count);

			for (int d = 0; d < Dimensions; d++)
			{
				TSharedPtr<PCGExData::IBufferProxy> InProxy = InputProxies[d];
				PCGEX_SCOPE_LOOP(i) { Values[i - S][d] = InProxy->Get<double>(i); }
			}

			Context->NoiseGenerator->GenerateInPlace(Positions, Values);

			// TODO : Compute noise buffer for the current scope
			// Cache positions from transform

			for (int d = 0; d < Dimensions; d++)
			{
				TSharedPtr<PCGExData::IBufferProxy> OutProxy = OutputProxies[d];
				PCGEX_SCOPE_LOOP(i) { OutProxy->Set(i, Values[i - S][d]); }
			}
		}
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		PointDataFacade->WriteFastest(TaskManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

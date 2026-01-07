// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Elements/PCGExUberNoise.h"

#include "PCGExBlendingCommon.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExProxyData.h"
#include "Data/PCGExProxyDataHelpers.h"
#include "PCGExVersion.h"
#include "Async/ParallelFor.h"
#include "Containers/PCGExScopedContainers.h"
#include "Core/PCGExNoise3DFactoryProvider.h"
#include "Core/PCGExProxyDataBlending.h"
#include "Data/PCGExProxyDataImpl.h"
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

		EPCGExABBlendingType BlendMode = Settings->BlendMode;

		PCGExData::FProxyDescriptor OutDescriptor;
		OutDescriptor.DataFacade = PointDataFacade;
		OutDescriptor.Role = PCGExData::EProxyRole::Write;

		const bool bIsNewOutput = Settings->Mode == EPCGExUberNoiseMode::New;
		if (bIsNewOutput)
		{
			OutDescriptor.RealType = Settings->OutputType;
			BlendMode = EPCGExABBlendingType::CopySource;
		}

		if (!OutDescriptor.Capture(Context, Settings->Attributes.GetTargetSelector(), PCGExData::EIOSide::Out, !bIsNewOutput))
		{
			// If new, blender will initialize the buffer
			if (!bIsNewOutput) { return false; }

			// Fill-in working type
			OutDescriptor.RealType = Settings->OutputType;
			NumFields = PCGExData::FSubSelectorRegistry::Get(OutDescriptor.RealType)->GetNumFields();

			switch (NumFields)
			{
			default:
			case 1: OutDescriptor.WorkingType = EPCGMetadataTypes::Double;
				break;
			case 2: OutDescriptor.WorkingType = EPCGMetadataTypes::Vector2;
				break;
			case 3: OutDescriptor.WorkingType = EPCGMetadataTypes::Vector;
				break;
			case 4: OutDescriptor.WorkingType = EPCGMetadataTypes::Vector4;
				break;
			}
		}
		else
		{
			NumFields = PCGExData::FSubSelectorRegistry::Get(OutDescriptor.WorkingType)->GetNumFields();
		}

		if (NumFields < 1 || NumFields > 4) { PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Invalid number of fields.")); }

		PCGExData::FProxyDescriptor NoiseDescriptor;
		NoiseDescriptor.DataFacade = PointDataFacade;
		NoiseDescriptor.Role = PCGExData::EProxyRole::Read;

		switch (NumFields)
		{
		default:
		case 1: NoiseDescriptor.RealType = EPCGMetadataTypes::Double;
			break;
		case 2: NoiseDescriptor.RealType = EPCGMetadataTypes::Vector2;
			break;
		case 3: NoiseDescriptor.RealType = EPCGMetadataTypes::Vector;
			break;
		case 4: NoiseDescriptor.RealType = EPCGMetadataTypes::Vector4;
			break;
		}

		NoiseDescriptor.WorkingType = OutDescriptor.WorkingType;
		NoiseDescriptor.AddFlags(PCGExData::EProxyFlags::Raw);

		Blender = PCGExBlending::CreateProxyBlender(Context, EPCGExABBlendingType::Add, NoiseDescriptor, OutDescriptor);
		if (!Blender) { return false; }

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExUberNoise::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		TConstPCGValueRange<FTransform> InTransforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		TArray<FVector> Positions;
		PCGExArrayHelpers::InitArray(Positions, Scope.Count);
		for (int i = 0; i < Scope.Count; i++) { Positions[i] = InTransforms[Scope.Start + i].GetLocation(); }

#define PCGEX_LOOP_NOISE(_TYPE) \
	{\
		TArray<_TYPE>& NoiseBuffer = *StaticCastSharedPtr<PCGExData::TRawBufferProxy<_TYPE>>(Blender->A)->Buffer.Get(); \
		Context->NoiseGenerator->Generate(Positions, TArrayView<_TYPE>(NoiseBuffer.GetData() + Scope.Start, Scope.Count)); \
		Blender->BlendScope(Scope, Scope.GetConstView<int8>(PointFilterCache), 1); \
	}

		switch (Blender->A->RealType)
		{
		default: /* no-op */ break;
		case EPCGMetadataTypes::Double: PCGEX_LOOP_NOISE(double)
			break;
		case EPCGMetadataTypes::Vector2: PCGEX_LOOP_NOISE(FVector2D)
			break;
		case EPCGMetadataTypes::Vector: PCGEX_LOOP_NOISE(FVector)
			break;
		case EPCGMetadataTypes::Vector4: PCGEX_LOOP_NOISE(FVector4)
			break;
		}

#undef PCGEX_LOOP_NOISE
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		PointDataFacade->WriteFastest(TaskManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExBlendPath.h"


#include "Data/PCGExPointIO.h"
#include "Data/Blending/PCGExBlendOpsManager.h"
#include "Details/PCGExDetailsSettings.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExBlendPathElement"
#define PCGEX_NAMESPACE BlendPath

PCGEX_SETTING_VALUE_IMPL(UPCGExBlendPathSettings, Lerp, double, LerpInput, LerpAttribute, LerpConstant)

UPCGExBlendPathSettings::UPCGExBlendPathSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSupportClosedLoops = false;
}

TArray<FPCGPinProperties> UPCGExBlendPathSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGExDataBlending::DeclareBlendOpsInputs(PinProperties, EPCGPinStatus::Required);
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BlendPath)

PCGExData::EIOInit UPCGExBlendPathSettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(BlendPath)

bool FPCGExBlendPathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BlendPath)

	if (!PCGExFactories::GetInputFactories<UPCGExBlendOpFactory>(
		Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories,
		{PCGExFactories::EType::Blending}))
	{
		return false;
	}

	return true;
}

bool FPCGExBlendPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBlendPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BlendPath)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		// TODO : Skip completion

		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				PCGEX_SKIP_INVALID_PATH_ENTRY
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to blend."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	return Context->TryComplete();
}

namespace PCGExBlendPath
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBlendPath::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		if (Settings->BlendOver == EPCGExBlendOver::Fixed)
		{
			LerpGetter = Settings->GetValueSettingLerp();
			if (!LerpGetter->Init(PointDataFacade)) { return false; }
		}

		MaxIndex = PointDataFacade->GetNum() - 1;

		Start = 0;
		End = MaxIndex;

		BlendOpsManager = MakeShared<PCGExDataBlending::FBlendOpsManager>(PointDataFacade);
		BlendOpsManager->SetSources(PointDataFacade); // We want operands A & B to be the vtx here

		if (!BlendOpsManager->Init(Context, Context->BlendingFactories)) { return false; }

		if (Settings->BlendOver == EPCGExBlendOver::Distance)
		{
			Metrics = PCGExPaths::FPathMetrics(PointDataFacade->GetIn()->GetTransform(0).GetLocation());
			PCGEx::InitArray(Length, PointDataFacade->GetNum());

			const TConstPCGValueRange<FTransform> Transforms = PointDataFacade->GetIn()->GetConstTransformValueRange();
			for (int i = 0; i < PointDataFacade->GetNum(); i++) { Length[i] = Metrics.Add(Transforms[i].GetLocation()); }
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::BlendPath::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		PCGEX_SCOPE_LOOP(Index)
		{
			if ((Index == 0 && !Settings->bBlendFirstPoint) ||
				(Index == MaxIndex && !Settings->bBlendLastPoint))
			{
				continue;
			}

			double Alpha = 0.5;

			if (Settings->BlendOver == EPCGExBlendOver::Distance)
			{
				Alpha = Length[Index] / Metrics.Length;
			}
			else if (Settings->BlendOver == EPCGExBlendOver::Index)
			{
				Alpha = static_cast<double>(Index) / static_cast<double>(PointDataFacade->GetNum());
			}
			else
			{
				Alpha = LerpGetter->Read(Index);
			}

			BlendOpsManager->Blend(Start, End, Index, Alpha);
		}
	}

	void FProcessor::CompleteWork()
	{
		if (BlendOpsManager) { BlendOpsManager->Cleanup(Context); }
		PointDataFacade->WriteFastest(AsyncManager);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

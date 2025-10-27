﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExSmooth.h"

#include "PCGParamData.h"
#include "Data/Blending/PCGExBlendOpsManager.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Details/PCGExDetailsSettings.h"
#include "Paths/PCGExPaths.h"


#include "Paths/Smoothing/PCGExMovingAverageSmoothing.h"

#define LOCTEXT_NAMESPACE "PCGExSmoothElement"
#define PCGEX_NAMESPACE Smooth

PCGEX_SETTING_VALUE_IMPL(UPCGExSmoothSettings, Influence, double, InfluenceInput, InfluenceAttribute, InfluenceConstant)
PCGEX_SETTING_VALUE_IMPL(UPCGExSmoothSettings, SmoothingAmount, double, SmoothingAmountType, SmoothingAmountAttribute, SmoothingAmountConstant)

TArray<FPCGPinProperties> UPCGExSmoothSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGExDataBlending::DeclareBlendOpsInputs(PinProperties, EPCGPinStatus::Normal, BlendingInterface);
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExSmooth::SourceOverridesSmoothing)
	return PinProperties;
}

bool UPCGExSmoothSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if (InPin->Properties.Label == PCGExDataBlending::SourceBlendingLabel) { return BlendingInterface == EPCGExBlendingInterface::Individual; }
	return Super::IsPinUsedByNodeExecution(InPin);
}

#if WITH_EDITORONLY_DATA
void UPCGExSmoothSettings::PostInitProperties()
{
	if (!HasAnyFlags(RF_ClassDefaultObject) && IsInGameThread())
	{
		if (!SmoothingMethod) { SmoothingMethod = NewObject<UPCGExMovingAverageSmoothing>(this, TEXT("SmoothingMethod")); }
	}
	Super::PostInitProperties();
}
#endif

PCGEX_INITIALIZE_ELEMENT(Smooth)

PCGExData::EIOInit UPCGExSmoothSettings::GetMainDataInitializationPolicy() const{ return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(Smooth)

bool FPCGExSmoothElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Smooth)
	PCGEX_OPERATION_BIND(SmoothingMethod, UPCGExSmoothingInstancedFactory, PCGExSmooth::SourceOverridesSmoothing)

	if (Settings->BlendingInterface == EPCGExBlendingInterface::Individual)
	{
		PCGExFactories::GetInputFactories<UPCGExBlendOpFactory>(
			Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories,
			{PCGExFactories::EType::Blending}, false);
	}

	return true;
}

bool FPCGExSmoothElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSmoothElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(Smooth)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bHasInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to smooth."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	return Context->TryComplete();
}

namespace PCGExSmooth
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSmooth::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		bClosedLoop = PCGExPaths::GetClosedLoop(PointDataFacade->GetIn());
		NumPoints = PointDataFacade->GetNum();

		if (!Context->BlendingFactories.IsEmpty())
		{
			BlendOpsManager = MakeShared<PCGExDataBlending::FBlendOpsManager>(PointDataFacade);

			if (!BlendOpsManager->Init(Context, Context->BlendingFactories)) { return false; }

			DataBlender = BlendOpsManager;
		}
		else if (Settings->BlendingInterface == EPCGExBlendingInterface::Monolithic)
		{
			MetadataBlender = MakeShared<PCGExDataBlending::FMetadataBlender>();
			MetadataBlender->SetTargetData(PointDataFacade);
			MetadataBlender->SetSourceData(PointDataFacade, PCGExData::EIOSide::In, true);

			if (!MetadataBlender->Init(Context, Settings->BlendingSettings)) { return false; }

			DataBlender = MetadataBlender;
		}

		if (!DataBlender) { DataBlender = MakeShared<PCGExDataBlending::FDummyBlender>(); }

		Influence = Settings->GetValueSettingInfluence();
		if (!Influence->Init(PointDataFacade)) { return false; }

		Smoothing = Settings->GetValueSettingSmoothingAmount();
		if (!Smoothing->Init(PointDataFacade)) { return false; }

		SmoothingOperation = Context->SmoothingMethod->CreateOperation();
		SmoothingOperation->Path = PointDataFacade->Source;
		SmoothingOperation->Blender = DataBlender;
		SmoothingOperation->bClosedLoop = bClosedLoop;

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::Smooth::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		TArray<PCGEx::FOpStats> Trackers;
		DataBlender->InitTrackers(Trackers);

		PCGEX_SCOPE_LOOP(Index)
		{
			if (!PointFilterCache[Index]) { continue; }

			const double LocalSmoothing = FMath::Clamp(Smoothing->Read(Index), 0, MAX_dbl) * Settings->ScaleSmoothingAmountAttribute;

			if ((Settings->bPreserveEnd && Index == NumPoints - 1) ||
				(Settings->bPreserveStart && Index == 0))
			{
				SmoothingOperation->SmoothSingle(Index, LocalSmoothing, 0, Trackers);
				continue;
			}

			SmoothingOperation->SmoothSingle(Index, LocalSmoothing, Influence->Read(Index), Trackers);
		}
	}

	void FProcessor::CompleteWork()
	{
		if (BlendOpsManager) { BlendOpsManager->Cleanup(Context); }

		SmoothingOperation.Reset();
		PointDataFacade->WriteFastest(AsyncManager);

		BlendOpsManager.Reset();
	}
}
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

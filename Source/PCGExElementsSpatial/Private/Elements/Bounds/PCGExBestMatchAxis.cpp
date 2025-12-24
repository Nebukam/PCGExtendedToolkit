// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Bounds/PCGExBestMatchAxis.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Details/PCGExSettingsDetails.h"
#include "Helpers/PCGExDataMatcher.h"
#include "Helpers/PCGExTargetsHandler.h"

#define LOCTEXT_NAMESPACE "PCGExBestMatchAxisElement"
#define PCGEX_NAMESPACE BestMatchAxis

PCGEX_SETTING_VALUE_IMPL(UPCGExBestMatchAxisSettings, Match, FVector, MatchInput, MatchSource, MatchConstant)

TArray<FPCGPinProperties> UPCGExBestMatchAxisSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (Mode == EPCGExBestMatchAxisTargetMode::ClosestTarget) { PCGEX_PIN_POINTS(PCGExCommon::Labels::SourceTargetsLabel, TEXT("Target points"), Required) }
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BestMatchAxis)

PCGExData::EIOInit UPCGExBestMatchAxisSettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(BestMatchAxis)

bool FPCGExBestMatchAxisElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BestMatchAxis)

	if (Settings->Mode == EPCGExBestMatchAxisTargetMode::ClosestTarget)
	{
		Context->TargetsHandler = MakeShared<PCGExMatching::FTargetsHandler>();
		Context->TargetsHandler->Init(Context, PCGExCommon::Labels::SourceTargetsLabel);

		Context->NumMaxTargets = Context->TargetsHandler->GetMaxNumTargets();
		if (!Context->NumMaxTargets)
		{
			PCGEX_LOG_MISSING_INPUT(InContext, FTEXT("No targets (empty datasets)"))
			return false;
		}

		Context->TargetsHandler->SetDistances(Settings->DistanceDetails);
		Context->TargetsHandler->SetMatchingDetails(Context, &Settings->DataMatching);
	}

	return true;
}

bool FPCGExBestMatchAxisElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBestMatchAxisElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BestMatchAxis)
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
			return Context->CancelExecution(TEXT("No data."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExBestMatchAxis
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBestMatchAxis::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		if (Context->TargetsHandler)
		{
			IgnoreList.Add(PointDataFacade->GetIn());

			if (PCGExMatching::FScope MatchingScope(Context->InitialMainPointsNum, true); !Context->TargetsHandler->PopulateIgnoreList(PointDataFacade->Source, MatchingScope, IgnoreList))
			{
				(void)Context->TargetsHandler->HandleUnmatchedOutput(PointDataFacade, true);
				return false;
			}
		}
		else
		{
			MatchGetter = Settings->GetValueSettingMatch();
			if (!MatchGetter->Init(PointDataFacade)) { return false; }
		}

		EPCGPointNativeProperties AllocateFor = EPCGPointNativeProperties::None;
		AllocateFor |= EPCGPointNativeProperties::Transform;

		PointDataFacade->GetOut()->AllocateProperties(AllocateFor);

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::BestMatchAxis::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		UPCGBasePointData* OutPoints = PointDataFacade->GetOut();
		TPCGValueRange<FTransform> OutTransforms = OutPoints->GetTransformValueRange(false);

		const bool bUseTargets = Settings->Mode == EPCGExBestMatchAxisTargetMode::ClosestTarget;

		PCGEX_SCOPE_LOOP(Index)
		{
			FVector AxisToMatch = FVector::ForwardVector;

			if (bUseTargets)
			{
				PCGExData::FConstPoint TargetPoint;
				double Distance = MAX_dbl;
				Context->TargetsHandler->FindClosestTarget(PointDataFacade->GetInPoint(Index), TargetPoint, Distance, &IgnoreList);
				if (TargetPoint.Index == -1)
				{
				}
			}
			else if (Settings->Mode == EPCGExBestMatchAxisTargetMode::Direction)
			{
			}
			else if (Settings->Mode == EPCGExBestMatchAxisTargetMode::LookAtRelativePosition)
			{
			}
			else if (Settings->Mode == EPCGExBestMatchAxisTargetMode::LookAtWorldPosition)
			{
			}

			/*
			FVector Offset;
			OutTransforms[Index].SetLocation(UVW.GetPosition(Index, Offset));
			OutBoundsMin[Index] += Offset;
			OutBoundsMax[Index] += Offset;
			*/
		}
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

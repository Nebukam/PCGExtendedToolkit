// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExSmooth.h"

#include "Data/Blending/PCGExMetadataBlender.h"
#include "Paths/Smoothing/PCGExMovingAverageSmoothing.h"

#define LOCTEXT_NAMESPACE "PCGExSmoothElement"
#define PCGEX_NAMESPACE Smooth

PCGExData::EInit UPCGExSmoothSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(Smooth)

FPCGExSmoothContext::~FPCGExSmoothContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExSmoothElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Smooth)
	PCGEX_OPERATION_BIND(SmoothingMethod, UPCGExSmoothingOperation)

	return true;
}

bool FPCGExSmoothElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSmoothElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(Smooth)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSmooth::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExSmooth::FProcessor>* NewBatch)
			{
				NewBatch->PrimaryOperation = Context->SmoothingMethod;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any paths to smooth."));
			return true;
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExSmooth
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(MetadataBlender)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSmooth::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(Smooth)

		PointDataFacade->bSupportsDynamic = true;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalTypedContext = TypedContext;
		LocalSettings = Settings;

		bClosedPath = Settings->bClosedPath;
		NumPoints = PointIO->GetNum();

		MetadataBlender = new PCGExDataBlending::FMetadataBlender(&Settings->BlendingSettings);
		MetadataBlender->PrepareForData(PointDataFacade);

		if (Settings->InfluenceType == EPCGExFetchType::Attribute)
		{
			Influence = PointDataFacade->GetScopedBroadcaster<double>(Settings->InfluenceAttribute);
			if (!Influence)
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Input missing influence attribute: \"{0}\"."), FText::FromName(Settings->InfluenceAttribute.GetName())));
				return false;
			}
		}

		if (Settings->SmoothingAmountType == EPCGExFetchType::Attribute)
		{
			Smoothing = PointDataFacade->GetScopedBroadcaster<double>(Settings->SmoothingAmountAttribute);
			if (!Smoothing)
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Input missing smoothing amount attribute: \"{0}\"."), FText::FromName(Settings->InfluenceAttribute.GetName())));
				return false;
			}
		}

		TypedOperation = Cast<UPCGExSmoothingOperation>(PrimaryOperation);

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
		FilterScope(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		if (!PointFilterCache[Index]) { return; }

		PCGExData::FPointRef PtRef = PointIO->GetOutPointRef(Index);
		const double LocalSmoothing = Smoothing ? FMath::Clamp(Smoothing->Values[Index], 0, TNumericLimits<double>::Max()) * LocalSettings->ScaleSmoothingAmountAttribute : LocalSettings->SmoothingAmountConstant;

		if ((LocalSettings->bPreserveEnd && Index == NumPoints - 1) ||
			(LocalSettings->bPreserveStart && Index == 0))
		{
			TypedOperation->SmoothSingle(PointIO, PtRef, LocalSmoothing, 0, MetadataBlender, bClosedPath);
			return;
		}

		const double LocalInfluence = Influence ? Influence->Values[Index] : LocalSettings->InfluenceConstant;
		TypedOperation->SmoothSingle(PointIO, PtRef, LocalSmoothing, LocalInfluence, MetadataBlender, bClosedPath);
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

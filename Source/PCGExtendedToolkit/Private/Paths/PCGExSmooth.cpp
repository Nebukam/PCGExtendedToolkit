// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExSmooth.h"

#include "Data/Blending/PCGExMetadataBlender.h"
#include "Paths/Smoothing/PCGExMovingAverageSmoothing.h"

#define LOCTEXT_NAMESPACE "PCGExSmoothElement"
#define PCGEX_NAMESPACE Smooth

FName UPCGExSmoothSettings::GetPointFilterLabel() const { return FName("SmoothConditions"); }

PCGExData::EInit UPCGExSmoothSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(Smooth)

FPCGExSmoothContext::~FPCGExSmoothContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExSmoothElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Smooth)
	PCGEX_OPERATION_BIND(SmoothingMethod, UPCGExMovingAverageSmoothing)

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

	Context->OutputMainPoints();

	return Context->TryComplete();
}

namespace PCGExSmooth
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(MetadataBlender)
		Smoothing.Empty();
		Influence.Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSmooth::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(Smooth)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		bClosedPath = Settings->bClosedPath;
		NumPoints = PointIO->GetNum();

		MetadataBlender = new PCGExDataBlending::FMetadataBlender(&Settings->BlendingSettings);
		MetadataBlender->PrepareForData(PointDataFacade);

		Influence.SetNum(NumPoints);
		Smoothing.SetNum(NumPoints);

		double Min = 0;
		double Max = 0;

		if (Settings->InfluenceType == EPCGExFetchType::Attribute)
		{
			PCGEx::FLocalSingleFieldGetter* InfluenceGetter = new PCGEx::FLocalSingleFieldGetter();
			InfluenceGetter->Capture(Settings->InfluenceAttribute);

			if (!InfluenceGetter->GrabAndDump(PointIO, Influence, false, Min, Max))
			{
				PCGEX_DELETE(InfluenceGetter)
				PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Input missing influence attribute: \"{0}\"."), FText::FromName(Settings->InfluenceAttribute.GetName())));
				return false;
			}

			PCGEX_DELETE(InfluenceGetter)
		}
		else
		{
			for (int i = 0; i < NumPoints; i++) { Influence[i] = Settings->InfluenceConstant; }
		}

		if (Settings->SmoothingAmountType == EPCGExFetchType::Attribute)
		{
			PCGEx::FLocalSingleFieldGetter* SmoothingAmountGetter = new PCGEx::FLocalSingleFieldGetter();
			SmoothingAmountGetter->Capture(Settings->InfluenceAttribute);

			if (!SmoothingAmountGetter->GrabAndDump(PointIO, Smoothing, false, Min, Max))
			{
				PCGEX_DELETE(SmoothingAmountGetter)
				PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Input missing smoothing amount attribute: \"{0}\"."), FText::FromName(Settings->InfluenceAttribute.GetName())));
				return false;
			}

			PCGEX_DELETE(SmoothingAmountGetter)

			for (double& Amount : Smoothing) { Amount = FMath::Clamp(Amount, 0, TNumericLimits<double>::Max()) * Settings->ScaleSmoothingAmountAttribute; }
		}
		else
		{
			for (int i = 0; i < NumPoints; i++) { Smoothing[i] = Settings->SmoothingAmountConstant; }
		}

		if (Settings->bPreserveStart) { Influence[0] = 0; }
		if (Settings->bPreserveEnd) { Influence[Influence.Num() - 1] = 0; }

		TypedOperation = Cast<UPCGExSmoothingOperation>(PrimaryOperation);

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		if (!PointFilterCache[Index]) { return; }

		PCGExData::FPointRef PtRef = PointIO->GetOutPointRef(Index);
		TypedOperation->SmoothSingle(
			PointIO, PtRef,
			Smoothing[Index], Influence[Index],
			MetadataBlender, bClosedPath);
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

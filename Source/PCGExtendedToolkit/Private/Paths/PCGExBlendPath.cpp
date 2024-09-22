// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExBlendPath.h"

#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExBlendPathElement"
#define PCGEX_NAMESPACE BlendPath

UPCGExBlendPathSettings::UPCGExBlendPathSettings(
		const FObjectInitializer& ObjectInitializer)
		: Super(ObjectInitializer)
{
	bSupportClosedLoops = false;		
}

PCGExData::EInit UPCGExBlendPathSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(BlendPath)

FPCGExBlendPathContext::~FPCGExBlendPathContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExBlendPathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BlendPath)

	return true;
}

bool FPCGExBlendPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBlendPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BlendPath)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		// TODO : Skip completion

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBlendPath::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bInvalidInputs = true;
					Entry->InitializeOutput(PCGExData::EInit::Forward);
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExBlendPath::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any paths to fuse."));
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

namespace PCGExBlendPath
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(Start)
		PCGEX_DELETE(End)
		PCGEX_DELETE(MetadataBlender)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBlendPath::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BlendPath)

		PointDataFacade->bSupportsScopedGet = TypedContext->bScopedAttributeGet;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalSettings = Settings;
		LocalTypedContext = TypedContext;

		if (Settings->BlendOver == EPCGExBlendOver::Fixed && Settings->LerpSource == EPCGExFetchType::Attribute)
		{
			LerpCache = PointDataFacade->GetScopedBroadcaster<double>(Settings->LerpAttribute);

			if (!LerpCache)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Lerp attribute is invalid."));
				return false;
			}
		}

		TArray<FPCGPoint>& OutPoints = PointIO->GetOut()->GetMutablePoints();
		MaxIndex = OutPoints.Num() - 1;

		Start = new PCGExData::FPointRef(PointIO->GetInPoint(0), 0);
		End = new PCGExData::FPointRef(PointIO->GetInPoint(MaxIndex), MaxIndex);

		MetadataBlender = new PCGExDataBlending::FMetadataBlender(&Settings->BlendingSettings);
		MetadataBlender->PrepareForData(PointDataFacade);

		if (LocalSettings->BlendOver == EPCGExBlendOver::Distance)
		{
			Metrics = PCGExPaths::FPathMetrics(OutPoints[0].Transform.GetLocation());
			PCGEX_SET_NUM_UNINITIALIZED(Length, PointIO->GetNum())
			for (int i = 0; i < PointIO->GetNum(); ++i) { Length[i] = Metrics.Add(OutPoints[i].Transform.GetLocation()); }
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
		FilterScope(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		if (Index == 0 || Index == MaxIndex) { return; }

		double Alpha = 0.5;
		const PCGExData::FPointRef Current = PointIO->GetOutPointRef(Index);
		MetadataBlender->PrepareForBlending(Current);

		if (LocalSettings->BlendOver == EPCGExBlendOver::Distance)
		{
			Alpha = Length[Index] / Metrics.Length;
		}
		else if (LocalSettings->BlendOver == EPCGExBlendOver::Index)
		{
			Alpha = static_cast<double>(Index) / static_cast<double>(PointIO->GetNum());
		}
		else
		{
			Alpha = LerpCache ? LerpCache->Values[Index] : LocalSettings->LerpConstant;
		}

		MetadataBlender->Blend(*Start, *End, Current, Alpha);
		MetadataBlender->CompleteBlending(Current, 2, 1);
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

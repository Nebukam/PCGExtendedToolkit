// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExBlendAttributes.h"

#include "Data/Blending/PCGExAttributeBlendFactoryProvider.h"


#define LOCTEXT_NAMESPACE "PCGExBlendAttributesElement"
#define PCGEX_NAMESPACE BlendAttributes

TArray<FPCGPinProperties> UPCGExBlendAttributesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExDataBlending::SourceBlendingLabel, "Blending configurations.", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BlendAttributes)

bool FPCGExBlendAttributesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BlendAttributes)

	if (!PCGExFactories::GetInputFactories<UPCGExAttributeBlendFactory>(
		Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories,
		{PCGExFactories::EType::Blending}, true))
	{
		return false;
	}

	return true;
}

bool FPCGExBlendAttributesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBlendAttributesElement::Execute);

	PCGEX_CONTEXT(BlendAttributes)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBlendAttributes::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExBlendAttributes::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExBlendAttributes
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBlendAttributes::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		Operations.Reserve(Context->BlendingFactories.Num());
		for (const TObjectPtr<const UPCGExAttributeBlendFactory>& Factory : Context->BlendingFactories)
		{
			UPCGExAttributeBlendOperation* Op = Factory->CreateOperation(Context);
			if (!Op || !Op->PrepareForData(Context, PointDataFacade)) { continue; }
			Operations.Add(Op);
		}

		NumPoints = PointDataFacade->GetNum();

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, BlendScopeTask)

		BlendScopeTask->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				This->BlendScope(Scope);
			};

		BlendScopeTask->StartSubLoops(NumPoints, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

		return true;
	}

	void FProcessor::BlendScope(const PCGExMT::FScope& InScope)
	{
		PointDataFacade->Fetch(InScope);
		FilterScope(InScope);

		TArray<FPCGPoint>& Points = PointDataFacade->GetMutablePoints();

		for (int i = InScope.Start; i < InScope.End; i++)
		{
			if (!PointFilterCache[i]) { continue; }
			for (UPCGExAttributeBlendOperation* Op : Operations) { Op->Blend(i, Points[i]); }
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

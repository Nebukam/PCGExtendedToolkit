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

		Operations = MakeShared<TArray<TSharedPtr<PCGExAttributeBlendOperation>>>();
		Operations->Reserve(Context->BlendingFactories.Num());

		for (const TObjectPtr<const UPCGExAttributeBlendFactory>& Factory : Context->BlendingFactories)
		{
			TSharedPtr<PCGExAttributeBlendOperation> Op = Factory->CreateOperation(Context);
			if (!Op)
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("An operation could not be created."));
				return false;
			}

			Op->OpIdx = Operations->Add(Op);
			Op->SiblingOperations = Operations;

			if (!Op->PrepareForData(Context, PointDataFacade)) { return false; }
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
		TArray<TSharedPtr<PCGExAttributeBlendOperation>>& Ops = *Operations.Get();

		for (int i = InScope.Start; i < InScope.End; i++)
		{
			if (!PointFilterCache[i]) { continue; }
			for (const TSharedPtr<PCGExAttributeBlendOperation>& Op : Ops) { Op->Blend(i, Points[i]); }
		}
	}

	void FProcessor::CompleteWork()
	{
		TArray<TSharedPtr<PCGExAttributeBlendOperation>>& Ops = *Operations.Get();

		TSet<TSharedPtr<PCGExData::FBufferBase>> DisabledBuffers;
		for (const TSharedPtr<PCGExAttributeBlendOperation>& Op : Ops) { Op->CompleteWork(DisabledBuffers); }

		for (const TSharedPtr<PCGExData::FBufferBase>& Buffer : DisabledBuffers)
		{
			// If disabled buffer does not exist on input, delete it entierely
			if (!Buffer->OutAttribute) { continue; }
			if (!PointDataFacade->GetIn()->Metadata->HasAttribute(Buffer->OutAttribute->Name))
			{
				PointDataFacade->GetOut()->Metadata->DeleteAttribute(Buffer->OutAttribute->Name);
				// TODO : Check types and make sure we're not deleting something
			}

			if (Buffer->InAttribute)
			{
				// Log a warning that can be silenced that we may have removed a valid attribute
			}
		}

		PointDataFacade->Write(AsyncManager);
	}

	void FProcessor::Cleanup()
	{
		TPointsProcessor<FPCGExBlendAttributesContext, UPCGExBlendAttributesSettings>::Cleanup();
		Operations->Empty();
		Operations.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

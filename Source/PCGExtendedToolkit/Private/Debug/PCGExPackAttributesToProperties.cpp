// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Debug/PCGExPackAttributesToProperties.h"

#define LOCTEXT_NAMESPACE "PCGExEdgesToPaths"
#define PCGEX_NAMESPACE PackAttributesToProperties

PCGExData::EIOInit UPCGExPackAttributesToPropertiesSettings::GetMainOutputInitMode() const
{
	bool bDeletesAny = false;
	for (const FPCGExDebugAttributeToProperty& Attr : Remaps)
	{
		if (Attr.bDeleteAttribute)
		{
			bDeletesAny = true;
			break;
		}
	}
	return bDeletesAny ? PCGExData::EIOInit::Duplicate : PCGExData::EIOInit::Forward;
}

PCGEX_INITIALIZE_ELEMENT(PackAttributesToProperties)

bool FPCGExPackAttributesToPropertiesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }
	return true;
}

bool FPCGExPackAttributesToPropertiesElement::ExecuteInternal(
	FPCGContext* InContext) const
{
#if WITH_EDITOR

	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPackAttributesToPropertiesElement::Execute);

	PCGEX_CONTEXT(PackAttributesToProperties)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPackAttributesToProperties::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExPackAttributesToProperties::FProcessor>>& NewBatch)
			{
			}))
		{
			Context->MainPoints->StageOutputs();
			return Context->TryComplete();
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)
	Context->MainPoints->StageOutputs();

	return Context->TryComplete();

#else

	DisabledPassThroughData(InContext);
	return true;
	
#endif
}


namespace PCGExPackAttributesToProperties
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteIndex::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		for (int i = 0; i < Settings->Remaps.Num(); i++)
		{
			FPCGExDebugAttributeToProperty& NewRemap = Remaps.Add_GetRef(Settings->Remaps[i]);
			if (!NewRemap.Init(Context, PointDataFacade, !Settings->bQuietWarnings)) { Remaps.Pop(); }
		}

		if (Remaps.IsEmpty()) { return false; }

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		for (FPCGExDebugAttributeToProperty& Remap : Remaps) { Remap.ProcessSinglePoint(Point, Index); }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

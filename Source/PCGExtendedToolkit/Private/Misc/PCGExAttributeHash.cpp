﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExAttributeHash.h"

#include "Data/PCGExData.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointIO.h"


#define LOCTEXT_NAMESPACE "PCGExAttributeHashElement"
#define PCGEX_NAMESPACE AttributeHash

PCGEX_INITIALIZE_ELEMENT(AttributeHash)
PCGEX_ELEMENT_BATCH_POINT_IMPL(AttributeHash)

bool FPCGExAttributeHashElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(AttributeHash)

	PCGEX_VALIDATE_NAME(Settings->OutputName)
	return true;
}

bool FPCGExAttributeHashElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributeHashElement::Execute);

	PCGEX_CONTEXT(AttributeHash)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExAttributeHash
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExAttributeHash::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, Settings->bOutputToAttribute ? PCGExData::EIOInit::Duplicate : PCGExData::EIOInit::Forward)

		Hasher = MakeShared<PCGEx::FAttributeHasher>(Settings->HashConfig);
		if (!Hasher->Init(Context, PointDataFacade->Source)) { return false; }
		if (Hasher->RequiresCompilation()) { Hasher->Compile(AsyncManager, nullptr); }

		return true;
	}

	void FProcessor::CompleteWork()
	{
		if (Settings->bOutputToTags) { PointDataFacade->Source->Tags->Set<int32>(Settings->OutputName.ToString(), Hasher->GetHash()); }
		if (Settings->bOutputToAttribute) { PCGExData::WriteMark<int32>(PointDataFacade->Source, Settings->OutputName, Hasher->GetHash()); }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

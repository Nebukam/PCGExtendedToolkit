// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExWriteIndex.h"

#define LOCTEXT_NAMESPACE "PCGExWriteIndexElement"
#define PCGEX_NAMESPACE WriteIndex

void UPCGExWriteIndexSettings::TagPointIO(const TSharedPtr<PCGExData::FPointIO>& InPointIO, const double MaxNumEntries) const
{
	if (bOutputCollectionIndex && bOutputCollectionIndexToTags)
	{
		InPointIO->Tags->Set<int32>(CollectionIndexAttributeName.ToString(), InPointIO->IOIndex);
	}

	if (bOutputCollectionNumEntries && bOutputNumEntriesToTags)
	{
		if (bOutputNormalizedNumEntriesToTags)
		{
			InPointIO->Tags->Set<double>(NumEntriesAttributeName.ToString(), static_cast<double>(InPointIO->GetNum()) / MaxNumEntries);
		}
		else
		{
			InPointIO->Tags->Set<int32>(NumEntriesAttributeName.ToString(), InPointIO->GetNum());
		}
	}
}

PCGEX_INITIALIZE_ELEMENT(WriteIndex)

bool FPCGExWriteIndexElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteIndex)

	if (Settings->bOutputPointIndex)
	{
		PCGEX_VALIDATE_NAME(Settings->OutputAttributeName)
		Context->bTagsOnly = false;
	}

	if (Settings->bOutputCollectionIndex && Settings->bOutputCollectionIndexToPoints)
	{
		PCGEX_VALIDATE_NAME(Settings->CollectionIndexAttributeName)
		Context->bTagsOnly = false;
	}

	if (Settings->bOutputCollectionNumEntries)
	{
		if (Settings->bOutputNumEntriesToPoints)
		{
			PCGEX_VALIDATE_NAME(Settings->NumEntriesAttributeName)
			Context->bTagsOnly = false;
		}

		for (const TSharedPtr<PCGExData::FPointIO>& IO : Context->MainPoints->Pairs)
		{
			Context->MaxNumEntries = FMath::Max(Context->MaxNumEntries, IO->GetNum());
		}
	}


	return true;
}

bool FPCGExWriteIndexElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteIndexElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(WriteIndex)
	PCGEX_EXECUTION_CHECK

	if (!Context->bTagsOnly)
	{
		PCGEX_ON_INITIAL_EXECUTION
		{
			if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExWriteIndex::FProcessor>>(
				[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
				[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExWriteIndex::FProcessor>>& NewBatch)
				{
					NewBatch->bSkipCompletion = !Settings->bOutputPointIndex;
				}))
			{
				return Context->CancelExecution(TEXT("Could not find any points to process."));
			}
		}

		PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)
	}
	else
	{
		while (Context->AdvancePointsIO())
		{
			Context->CurrentIO->InitializeOutput(PCGExData::EIOInit::Forward);
			Settings->TagPointIO(Context->CurrentIO, Context->MaxNumEntries);
		}

		Context->Done();
	}

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExWriteIndex
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteIndex::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		NumPoints = PointDataFacade->GetNum();
		MaxIndex = NumPoints - 1;

		Settings->TagPointIO(PointDataFacade->Source, Context->MaxNumEntries);

		if (Settings->bOutputCollectionIndex && Settings->bOutputCollectionIndexToPoints)
		{
			WriteMark(PointDataFacade->Source, Settings->CollectionIndexAttributeName, BatchIndex);
		}

		if (Settings->bOutputCollectionNumEntries && Settings->bOutputNumEntriesToPoints)
		{
			if (Settings->bOutputNormalizedNumEntriesToPoints)
			{
				WriteMark(PointDataFacade->Source, Settings->NumEntriesAttributeName, static_cast<double>(PointDataFacade->GetNum()) / Context->MaxNumEntries);
			}
			else
			{
				WriteMark(PointDataFacade->Source, Settings->NumEntriesAttributeName, PointDataFacade->GetNum());
			}
		}

		if (Settings->bOutputPointIndex)
		{
			if (Settings->bOutputNormalizedIndex)
			{
				DoubleWriter = PointDataFacade->GetWritable<double>(Settings->OutputAttributeName, -1, Settings->bAllowInterpolation, PCGExData::EBufferInit::Inherit);
			}
			else
			{
				IntWriter = PointDataFacade->GetWritable<int32>(Settings->OutputAttributeName, -1, Settings->bAllowInterpolation, PCGExData::EBufferInit::Inherit);
			}

			StartParallelLoopForPoints();
		}

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		PCGEX_SCOPE_LOOP(Index)
		{
			if (Settings->bOneMinus)
			{
				if (DoubleWriter) { DoubleWriter->GetMutable(Index) = 1 - (static_cast<double>(Index) / MaxIndex); }
				else if (IntWriter) { IntWriter->GetMutable(Index) = MaxIndex - Index; }
			}
			else
			{
				if (DoubleWriter) { DoubleWriter->GetMutable(Index) = static_cast<double>(Index) / MaxIndex; }
				else if (IntWriter) { IntWriter->GetMutable(Index) = Index; }
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

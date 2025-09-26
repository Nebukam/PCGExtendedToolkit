﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExWriteIndex.h"

#include "PCGExBroadcast.h"
#include "PCGParamData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSplineData.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

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
		if (bNormalizeNumEntries)
		{
			InPointIO->Tags->Set<double>(NumEntriesAttributeName.ToString(), static_cast<double>(InPointIO->GetNum()) / MaxNumEntries);
		}
		else
		{
			InPointIO->Tags->Set<int32>(NumEntriesAttributeName.ToString(), InPointIO->GetNum());
		}
	}
}

void UPCGExWriteIndexSettings::TagData(const int32 Index, FPCGTaggedData& InTaggedData, double NumEntries, double MaxNumEntries) const
{
	if (bOutputCollectionIndex && bOutputCollectionIndexToTags)
	{
		const TSharedPtr<PCGExData::TDataValue<int32>> Tag = MakeShared<PCGExData::TDataValue<int32>>(Index);
		InTaggedData.Tags.Add(Tag->Flatten(CollectionIndexAttributeName.ToString()));
	}

	if (bOutputCollectionNumEntries && bOutputNumEntriesToTags)
	{
		if (bNormalizeNumEntries)
		{
			const TSharedPtr<PCGExData::TDataValue<double>> Tag = MakeShared<PCGExData::TDataValue<double>>(NumEntries / MaxNumEntries);
			InTaggedData.Tags.Add(Tag->Flatten(NumEntriesAttributeName.ToString()));
		}
		else
		{
			const TSharedPtr<PCGExData::TDataValue<int32>> Tag = MakeShared<PCGExData::TDataValue<int32>>(NumEntries);
			InTaggedData.Tags.Add(Tag->Flatten(NumEntriesAttributeName.ToString()));
		}
	}
}

bool UPCGExWriteIndexSettings::CollectionLevelOutputOnly() const
{
	return
		(!bOutputPointIndex) &&
		(!bOutputCollectionNumEntries || PCGExHelpers::IsDataDomainAttribute(NumEntriesAttributeName)) &&
		(!bOutputCollectionIndex || PCGExHelpers::IsDataDomainAttribute(CollectionIndexAttributeName));
}

bool UPCGExWriteIndexSettings::HasDynamicPins() const
{
	return IsInputless();
}

TArray<FPCGPinProperties> UPCGExWriteIndexSettings::InputPinProperties() const
{
	if (!IsInputless()) { return Super::InputPinProperties(); }
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_ANY(GetMainInputPin(), "Inputs", Required)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExWriteIndexSettings::OutputPinProperties() const
{
	if (!IsInputless()) { return Super::OutputPinProperties(); }
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_ANY(GetMainOutputPin(), "Output", Required)
	return PinProperties;
}

#if WITH_EDITOR
FString UPCGExWriteIndexSettings::GetDisplayName() const
{
	return bOutputPointIndex ? OutputAttributeName.ToString() : bOutputCollectionIndex ? CollectionIndexAttributeName.ToString() : bOutputCollectionNumEntries ? NumEntriesAttributeName.ToString() : TEXT("...");
}
#endif

PCGEX_INITIALIZE_ELEMENT(WriteIndex)
PCGEX_ELEMENT_BATCH_POINT_IMPL(WriteIndex)

bool FPCGExWriteIndexElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteIndex)

	Context->bCollectionLevelOutputOnly = Settings->CollectionLevelOutputOnly();

	bool bTagOnly = true;

	if (Settings->bOutputPointIndex)
	{
		PCGEX_VALIDATE_NAME(Settings->OutputAttributeName)
		Context->EntryIndexIdentifier = PCGEx::GetAttributeIdentifier(Settings->OutputAttributeName);
	}

	if (Settings->bOutputCollectionIndex && !Settings->bOutputCollectionIndexToTags)
	{
		PCGEX_VALIDATE_NAME(Settings->CollectionIndexAttributeName)
		Context->CollectionIndexIdentifier = PCGEx::GetAttributeIdentifier(Settings->CollectionIndexAttributeName);
		bTagOnly = false;
	}

	if (Settings->bOutputCollectionNumEntries)
	{
		if (!Settings->bNormalizeNumEntries) { PCGEX_VALIDATE_NAME(Settings->NumEntriesAttributeName) }
		Context->NumEntriesIdentifier = PCGEx::GetAttributeIdentifier(Settings->NumEntriesAttributeName);
		bTagOnly = false;
	}

	if (Context->bCollectionLevelOutputOnly)
	{
		Context->WorkingData = Context->InputData.GetInputsByPin(Settings->GetMainInputPin());
		Context->NumEntries.Reserve(Context->WorkingData.Num());

		for (FPCGTaggedData& TaggedData : Context->WorkingData)
		{
			if (!bTagOnly) { TaggedData.Data = TaggedData.Data->DuplicateData(Context); }

			if (const UPCGParamData* ParamData = Cast<UPCGParamData>(TaggedData.Data))
			{
				TUniquePtr<FPCGAttributeAccessorKeysEntries> Keys = MakeUnique<FPCGAttributeAccessorKeysEntries>(ParamData->Metadata);
				Context->MaxNumEntries = FMath::Max(Context->MaxNumEntries, Context->NumEntries.Emplace_GetRef(Keys->GetNum()));
				continue;
			}

			if (const UPCGSplineData* SplineData = Cast<UPCGSplineData>(TaggedData.Data))
			{
				Context->MaxNumEntries = FMath::Max(Context->MaxNumEntries, Context->NumEntries.Emplace_GetRef(SplineData->GetNumSegments()));
				continue;
			}

			if (const UPCGBasePointData* BasePointData = Cast<UPCGBasePointData>(TaggedData.Data))
			{
				Context->MaxNumEntries = FMath::Max(Context->MaxNumEntries, Context->NumEntries.Emplace_GetRef(BasePointData->GetNumPoints()));
				continue;
			}

			if (const UPCGPointData* PointData = Cast<UPCGPointData>(TaggedData.Data))
			{
				Context->MaxNumEntries = FMath::Max(Context->MaxNumEntries, Context->NumEntries.Emplace_GetRef(PointData->GetNumPoints()));
				continue;
			}

			Context->NumEntries.Emplace(0);
		}
	}
	else
	{
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

	if (!Context->bCollectionLevelOutputOnly)
	{
		PCGEX_ON_INITIAL_EXECUTION
		{
			if (!Context->StartBatchProcessingPoints(
				[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
				[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
				{
					NewBatch->bSkipCompletion = !Settings->bOutputPointIndex;
				}))
			{
				return Context->CancelExecution(TEXT("Could not find any points to process."));
			}
		}

		PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

		Context->MainPoints->StageOutputs();
		Context->Done();
	}
	else
	{
		if (Settings->bOutputCollectionIndex && !Settings->bOutputCollectionIndexToTags)
		{
			PCGEx::ExecuteWithRightType(
				PCGExDataHelpers::GetNumericType(Settings->CollectionIndexOutputType), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					for (int i = 0; i < Context->WorkingData.Num(); i++)
					{
						PCGExData::WriteMark<T>(const_cast<UPCGData*>(Context->WorkingData[i].Data.Get()), Context->CollectionIndexIdentifier, PCGEx::Convert<int32, T>(i));
					}
				});
		}

		if (Settings->bOutputCollectionNumEntries && !Settings->bOutputNumEntriesToTags)
		{
			PCGEx::ExecuteWithRightType(
				PCGExDataHelpers::GetNumericType(Settings->NumEntriesOutputType), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					for (int i = 0; i < Context->WorkingData.Num(); i++)
					{
						double NumEntries = Settings->bNormalizeNumEntries ? Context->NumEntries[i] / Context->MaxNumEntries : Context->NumEntries[i];
						PCGExData::WriteMark<T>(const_cast<UPCGData*>(Context->WorkingData[i].Data.Get()), Context->NumEntriesIdentifier, PCGEx::Convert<int32, T>(NumEntries));
					}
				});
		}

		for (int i = 0; i < Context->WorkingData.Num(); i++)
		{
			FPCGTaggedData& TaggedData = Context->WorkingData[i];
			Settings->TagData(i, TaggedData, Context->NumEntries[i], Context->MaxNumEntries);
			Context->StageOutput(const_cast<UPCGData*>(TaggedData.Data.Get()), Settings->GetMainOutputPin(), TaggedData.Tags, false, false, false);
		}

		Context->Done();
	}


	return Context->TryComplete();
}

namespace PCGExWriteIndex
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteIndex::Process);

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		NumPoints = PointDataFacade->GetNum();
		MaxIndex = NumPoints - 1;

		Settings->TagPointIO(PointDataFacade->Source, Context->MaxNumEntries);

		if (Settings->bOutputCollectionIndex && !Settings->bOutputCollectionIndexToTags)
		{
			PCGEx::ExecuteWithRightType(
				PCGExDataHelpers::GetNumericType(Settings->CollectionIndexOutputType), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					PCGExData::WriteMark<T>(PointDataFacade->GetOut(), Context->CollectionIndexIdentifier, PCGEx::Convert<int32, T>(BatchIndex));
				});
		}

		if (Settings->bOutputCollectionNumEntries && !Settings->bOutputNumEntriesToTags)
		{
			PCGEx::ExecuteWithRightType(
				PCGExDataHelpers::GetNumericType(Settings->NumEntriesOutputType), [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					if (Settings->bNormalizeNumEntries)
					{
						PCGExData::WriteMark<T>(PointDataFacade->GetOut(), Context->NumEntriesIdentifier, PCGEx::Convert<double, T>(static_cast<double>(PointDataFacade->GetNum()) / Context->MaxNumEntries));
					}
					else
					{
						PCGExData::WriteMark<T>(PointDataFacade->GetOut(), Context->NumEntriesIdentifier, PCGEx::Convert<int32, T>(PointDataFacade->GetNum()));
					}
				});
		}

		if (Settings->bOutputPointIndex)
		{
			if (Settings->bNormalizedEntryIndex)
			{
				DoubleWriter = PointDataFacade->GetWritable<double>(Context->EntryIndexIdentifier, -1, Settings->bAllowInterpolation, PCGExData::EBufferInit::Inherit);
			}
			else
			{
				IntWriter = PointDataFacade->GetWritable<int32>(Context->EntryIndexIdentifier, -1, Settings->bAllowInterpolation, PCGExData::EBufferInit::Inherit);
			}

			StartParallelLoopForPoints();
		}

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::WriteIndex::ProcessPoints);

		PCGEX_SCOPE_LOOP(Index)
		{
			if (Settings->bOneMinus)
			{
				if (DoubleWriter) { DoubleWriter->SetValue(Index, 1 - (static_cast<double>(Index) / MaxIndex)); }
				else if (IntWriter) { IntWriter->SetValue(Index, MaxIndex - Index); }
			}
			else
			{
				if (DoubleWriter) { DoubleWriter->SetValue(Index, static_cast<double>(Index) / MaxIndex); }
				else if (IntWriter) { IntWriter->SetValue(Index, Index); }
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->WriteFastest(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

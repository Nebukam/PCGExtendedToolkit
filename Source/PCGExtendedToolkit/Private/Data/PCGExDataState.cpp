// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExDataState.h"


PCGExDataFilter::TFilterHandler* UPCGExStateDefinitionBase::CreateHandler() const
{
	return new PCGExDataState::TStateHandler(this);
}

void UPCGExStateDefinitionBase::BeginDestroy()
{
	ValidStateAttributes.Empty();
	InvalidStateAttributes.Empty();

	PCGEX_DELETE_TARRAY(ValidStateAttributesInfos)
	PCGEX_DELETE_TARRAY(InvalidStateAttributesInfos)

	Super::BeginDestroy();
}

namespace PCGExDataState
{
	bool TStateHandler::Test(const int32 PointIndex) const
	{
		return false;
	}

	void TStateHandler::PrepareForWriting(PCGExData::FPointIO* PointIO)
	{
		const int32 NumPoints = Results.Num();

		OverlappingAttributes.Empty();

		bool bNeedIfs = Results[0];
		bool bNeedElses = !bNeedIfs;

		if (!bNeedIfs)
		{
			for (int p = 0; p < NumPoints; p++)
			{
				if (Results[p])
				{
					bNeedIfs = true;
					break;
				}
			}
		}

		if (!bNeedElses)
		{
			for (int p = 0; p < NumPoints; p++)
			{
				if (!Results[p])
				{
					bNeedElses = true;
					break;
				}
			}
		}

		InValidStateAttributes.Empty();
		InInvalidStateAttributes.Empty();

		OutValidStateAttributes.Empty();
		OutInvalidStateAttributes.Empty();

		UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;

		auto CreatePlaceholderAttributes = [&](
			const TArray<PCGEx::FAttributesInfos*>& InfosList,
			TArray<FPCGMetadataAttributeBase*>& InAttributes,
			TArray<FPCGMetadataAttributeBase*>& OutAttributes)
		{
			for (PCGEx::FAttributesInfos* Infos : InfosList)
			{
				for (FPCGMetadataAttributeBase* Att : Infos->Attributes)
				{
					InAttributes.Add(Att);

					PCGMetadataAttribute::CallbackWithRightType(
						Att->GetTypeId(),
						[&](auto DummyValue) -> void
						{
							using RawT = decltype(DummyValue);

							if (FPCGMetadataAttributeBase* OutAttribute = Metadata->GetMutableAttribute(Att->Name))
							{
								if (OutAttribute->GetTypeId() != Att->GetTypeId())
								{
									OverlappingAttributes.Add(Att->Name.ToString());
									OutAttributes.Add(nullptr);
								}
								else { OutAttributes.Add(OutAttribute); }
								return;
							}

							const FPCGMetadataAttribute<RawT>* TypedInAttribute = static_cast<FPCGMetadataAttribute<RawT>*>(Att);
							FPCGMetadataAttribute<RawT>* TypedOutAttribute = Metadata->FindOrCreateAttribute(
								Att->Name,
								TypedInAttribute->GetValue(PCGInvalidEntryKey),
								TypedInAttribute->AllowsInterpolation());

							OutAttributes.Add(TypedOutAttribute);
						});
				}
			}
		};

		if (bNeedIfs) { CreatePlaceholderAttributes(StateDefinition->ValidStateAttributesInfos, InValidStateAttributes, OutValidStateAttributes); }
		if (bNeedElses) { CreatePlaceholderAttributes(StateDefinition->InvalidStateAttributesInfos, InInvalidStateAttributes, OutInvalidStateAttributes); }
	}


	void TStatesManager::PrepareForTesting()
	{
		const int32 NumPoints = PointIO->GetNum();
		HighestState.SetNumUninitialized(NumPoints);
		for (int i = 0; i < NumPoints; i++) { HighestState[i] = -1; }

		TFilterManager::PrepareForTesting();
	}

	void TStatesManager::Test(const int32 PointIndex)
	{
		int32 HState = -1;

		for (PCGExDataFilter::TFilterHandler* Handler : Handlers)
		{
			const TStateHandler* StateHandler = static_cast<TStateHandler*>(Handler);
			const bool bValue = Handler->Test(PointIndex);
			Handler->Results[PointIndex] = bValue;
			if (bValue) { HState = StateHandler->Index; }
		}

		HighestState[PointIndex] = HState;
	}

	void TStatesManager::WriteStateNames(const FName AttributeName, const FName DefaultValue, const TArray<int32>& InIndices)
	{
		PCGEx::TFAttributeWriter<FName>* StateNameWriter = new PCGEx::TFAttributeWriter<FName>(AttributeName, DefaultValue, false);
		StateNameWriter->BindAndGet(*PointIO);

		for (const int32 i : InIndices)
		{
			if (const int32 HighestStateId = HighestState[i]; HighestStateId != -1)
			{
				const UPCGExStateDefinitionBase* State = static_cast<const UPCGExStateDefinitionBase*>(Handlers[HighestStateId]->Definition);
				StateNameWriter->Values[i] = State->StateName;
			}
			else { StateNameWriter->Values[i] = DefaultValue; }
		}

		StateNameWriter->Write();
		PCGEX_DELETE(StateNameWriter)
	}

	void TStatesManager::WriteStateValues(const FName AttributeName, const int32 DefaultValue, const TArray<int32>& InIndices)
	{
		PCGEx::TFAttributeWriter<int32>* StateValueWriter = new PCGEx::TFAttributeWriter<int32>(AttributeName, DefaultValue, false);
		StateValueWriter->BindAndGet(*PointIO);

		for (const int32 i : InIndices)
		{
			if (const int32 HighestStateId = HighestState[i]; HighestStateId != -1)
			{
				const UPCGExStateDefinitionBase* State = static_cast<const UPCGExStateDefinitionBase*>(Handlers[HighestStateId]->Definition);
				StateValueWriter->Values[i] = State->StateId;
			}
			else { StateValueWriter->Values[i] = DefaultValue; }
		}

		StateValueWriter->Write();
		PCGEX_DELETE(StateValueWriter)
	}

	void TStatesManager::WriteStateIndividualStates(FPCGExAsyncManager* AsyncManager, const TArray<int32>& InIndices)
	{
		for (PCGExDataFilter::TFilterHandler* Handler : Handlers)
		{
			AsyncManager->Start<PCGExDataStateTask::FWriteIndividualState>(
				Handler->Index, PointIO,
				static_cast<TStateHandler*>(Handler), &InIndices);
		}
	}

	void TStatesManager::WritePrepareForStateAttributes(const FPCGContext* InContext)
	{
		const int32 NumPoints = PointIO->GetNum();

		for (PCGExDataFilter::TFilterHandler* Handler : Handlers)
		{
			TStateHandler* StateHandler = static_cast<TStateHandler*>(Handler);
			StateHandler->PrepareForWriting(PointIO);

			if (!StateHandler->OverlappingAttributes.IsEmpty())
			{
				FString Names = FString::Join(StateHandler->OverlappingAttributes.Array(), TEXT(", "));
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Some If/Else attributes ({0}) have the same name but different types, this will have unexpected results."), FText::FromString(Names)));
			}
		}
	}

	void TStatesManager::WriteStateAttributes(const int32 PointIndex)
	{
		const PCGMetadataEntryKey Key = PointIO->GetOutPoint(PointIndex).MetadataEntry;

		auto ForwardValues = [&](
			TArray<FPCGMetadataAttributeBase*>& In,
			TArray<FPCGMetadataAttributeBase*>& Out)
		{
			for (int i = 0; i < Out.Num(); i++)
			{
				FPCGMetadataAttributeBase* OutAtt = Out[i];
				if (!OutAtt) { continue; }
				PCGMetadataAttribute::CallbackWithRightType(
					OutAtt->GetTypeId(),
					[&](auto DummyValue) -> void
					{
						using RawT = decltype(DummyValue);
						const FPCGMetadataAttribute<RawT>* TypedInAtt = static_cast<FPCGMetadataAttribute<RawT>*>(In[i]);
						FPCGMetadataAttribute<RawT>* TypedOutAtt = static_cast<FPCGMetadataAttribute<RawT>*>(OutAtt);
						TypedOutAtt->SetValue(Key, TypedInAtt->GetValueFromItemKey(PCGInvalidEntryKey));
					});
			}
		};

		for (PCGExDataFilter::TFilterHandler* Handler : Handlers)
		{
			TStateHandler* StateHandler = static_cast<TStateHandler*>(Handler);
			if (Handler->Results[PointIndex]) { ForwardValues(StateHandler->InValidStateAttributes, StateHandler->OutValidStateAttributes); }
			else { ForwardValues(StateHandler->InInvalidStateAttributes, StateHandler->OutInvalidStateAttributes); }
		}
	}

	void TStatesManager::PostProcessHandler(PCGExDataFilter::TFilterHandler* Handler)
	{
		const TStateHandler* StateHandler = static_cast<TStateHandler*>(Handler);
		if (StateHandler->bPartial) { bHasPartials = true; }
		TFilterManager::PostProcessHandler(Handler);
	}
}

namespace PCGExDataStateTask
{
	bool FWriteIndividualState::ExecuteTask()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FWriteIndividualState::ExecuteTask);

		PCGEx::TFAttributeWriter<bool>* StateWriter = new PCGEx::TFAttributeWriter<bool>(static_cast<const UPCGExStateDefinitionBase*>(Handler->Definition)->StateName);
		StateWriter->BindAndGet(*PointIO);

		for (const int32 i : (*InIndices)) { StateWriter->Values[i] = Handler->Results[i]; }
		StateWriter->Write();

		PCGEX_DELETE(StateWriter)
		return true;
	}
}

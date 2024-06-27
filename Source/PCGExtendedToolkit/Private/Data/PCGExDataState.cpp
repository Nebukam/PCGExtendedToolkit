// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExDataState.h"


PCGExPointFilter::TFilter* UPCGExDataStateFactoryBase::CreateFilter() const
{
	return new PCGExDataState::TDataState(this);
}

void UPCGExDataStateFactoryBase::BeginDestroy()
{
	ValidStateAttributes.Empty();
	InvalidStateAttributes.Empty();

	PCGEX_DELETE_TARRAY(ValidStateAttributesInfos)
	PCGEX_DELETE_TARRAY(InvalidStateAttributesInfos)

	Super::BeginDestroy();
}

namespace PCGExDataState
{
	bool TDataState::Test(const int32 PointIndex) const
	{
		return false;
	}

	void TDataState::PrepareForWriting()
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

		UPCGMetadata* Metadata = PointDataCache->Source->GetOut()->Metadata;

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

		if (bNeedIfs) { CreatePlaceholderAttributes(StateFactory->ValidStateAttributesInfos, InValidStateAttributes, OutValidStateAttributes); }
		if (bNeedElses) { CreatePlaceholderAttributes(StateFactory->InvalidStateAttributesInfos, InInvalidStateAttributes, OutInvalidStateAttributes); }
	}


	void TStatesManager::PrepareForTesting()
	{
		const int32 NumPoints = PointDataCache->Source->GetNum();
		HighestState.SetNumUninitialized(NumPoints);
		for (int32& State : HighestState) { State = -1; }

		return TManager::PrepareForTesting();
	}

	void TStatesManager::PrepareForTesting(const TArrayView<const int32>& PointIndices)
	{
		if (const int32 NumPoints = PointDataCache->Source->GetNum(); HighestState.Num() != NumPoints) { HighestState.SetNumUninitialized(NumPoints); }
		for (const int32 i : PointIndices) { HighestState[i] = -1; }

		return TManager::PrepareForTesting(PointIndices);
	}

	bool TStatesManager::TestPoint(const int32 Index)
	{
		int32 HState = -1;

		for (PCGExPointFilter::TFilter* Handler : PointFilters)
		{
			const TDataState* StateHandler = static_cast<TDataState*>(Handler);
			const bool bValue = Handler->Test(Index);
			Handler->Results[Index] = bValue;
			if (bValue) { HState = StateHandler->FilterIndex; }
		}

		HighestState[Index] = HState;
		return HState != -1;
	}

	void TStatesManager::WriteStateNames(PCGExMT::FTaskManager* AsyncManager, const FName AttributeName, const FName DefaultValue, const TArrayView<const int32>& InIndices)
	{
		PCGEx::TFAttributeWriter<FName>* StateNameWriter = new PCGEx::TFAttributeWriter<FName>(AttributeName, DefaultValue, false);
		StateNameWriter->BindAndSetNumUninitialized(const_cast<PCGExData::FPointIO*>(PointDataCache->Source)); // TODO: Parallellllll

		for (const int32 i : InIndices)
		{
			if (const int32 HighestStateId = HighestState[i]; HighestStateId != -1)
			{
				const UPCGExDataStateFactoryBase* State = static_cast<const UPCGExDataStateFactoryBase*>(PointFilters[HighestStateId]->Factory);
				StateNameWriter->Values[i] = State->StateName;
			}
			else { StateNameWriter->Values[i] = DefaultValue; }
		}

		//PCGEX_ASYNC_WRITE_DELETE(AsyncManager, StateNameWriter)
		StateNameWriter->Write();
		PCGEX_DELETE(StateNameWriter)
	}

	void TStatesManager::WriteStateValues(PCGExMT::FTaskManager* AsyncManager, const FName AttributeName, const int32 DefaultValue, const TArrayView<const int32>& InIndices)
	{
		PCGEx::TFAttributeWriter<int32>* StateValueWriter = new PCGEx::TFAttributeWriter<int32>(AttributeName, DefaultValue, false);
		StateValueWriter->BindAndSetNumUninitialized(const_cast<PCGExData::FPointIO*>(PointDataCache->Source)); // TODO: Parallellllll

		for (const int32 i : InIndices)
		{
			if (const int32 HighestStateId = HighestState[i]; HighestStateId != -1)
			{
				const UPCGExDataStateFactoryBase* State = static_cast<const UPCGExDataStateFactoryBase*>(PointFilters[HighestStateId]->Factory);
				StateValueWriter->Values[i] = State->StateId;
			}
			else { StateValueWriter->Values[i] = DefaultValue; }
		}

		//PCGEX_ASYNC_WRITE_DELETE(AsyncManager, StateValueWriter)
		StateValueWriter->Write();
		PCGEX_DELETE(StateValueWriter)
	}

	void TStatesManager::WriteStateIndividualStates(PCGExMT::FTaskManager* AsyncManager, const TArrayView<const int32>& InIndices)
	{
		for (PCGExPointFilter::TFilter* Handler : PointFilters)
		{
			AsyncManager->Start<PCGExDataStateTask::FWriteIndividualState>(
				Handler->FilterIndex, const_cast<PCGExData::FPointIO*>(PointDataCache->Source),
				static_cast<TDataState*>(Handler), InIndices);
		}
	}

	void TStatesManager::WritePrepareForStateAttributes(const FPCGContext* InContext)
	{
		const int32 NumPoints = PointDataCache->Source->GetNum();

		for (PCGExPointFilter::TFilter* Handler : PointFilters)
		{
			TDataState* StateHandler = static_cast<TDataState*>(Handler);
			StateHandler->PrepareForWriting();

			if (!StateHandler->OverlappingAttributes.IsEmpty())
			{
				FString Names = FString::Join(StateHandler->OverlappingAttributes.Array(), TEXT(", "));
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Some If/Else attributes ({0}) have the same name but different types, this will have unexpected results."), FText::FromString(Names)));
			}
		}
	}

	void TStatesManager::WriteStateAttributes(const int32 PointIndex)
	{
		const PCGMetadataEntryKey Key = PointDataCache->Source->GetOutPoint(PointIndex).MetadataEntry;

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

		for (PCGExPointFilter::TFilter* Handler : PointFilters)
		{
			TDataState* StateHandler = static_cast<TDataState*>(Handler);
			if (Handler->Results[PointIndex]) { ForwardValues(StateHandler->InValidStateAttributes, StateHandler->OutValidStateAttributes); }
			else { ForwardValues(StateHandler->InInvalidStateAttributes, StateHandler->OutInvalidStateAttributes); }
		}
	}
}

namespace PCGExDataStateTask
{
	bool FWriteIndividualState::ExecuteTask()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FWriteIndividualState::ExecuteTask);

		PCGEx::TFAttributeWriter<bool>* StateWriter = new PCGEx::TFAttributeWriter<bool>(static_cast<const UPCGExDataStateFactoryBase*>(Handler->Factory)->StateName);
		StateWriter->BindAndSetNumUninitialized(PointIO);

		for (const int32 i : Indices) { StateWriter->Values[i] = Handler->Results[i]; }
		StateWriter->Write();

		PCGEX_DELETE(StateWriter)
		return true;
	}
}

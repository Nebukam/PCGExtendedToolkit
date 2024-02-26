// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExDataState.h"


void UPCGExStateDefinitionBase::BeginDestroy()
{
	IfAttributes.Empty();
	ElseAttributes.Empty();

	PCGEX_DELETE_TARRAY(IfInfos)
	PCGEX_DELETE_TARRAY(ElseInfos)

	Super::BeginDestroy();
}

namespace PCGExDataState
{
	bool AStateHandler::Test(const int32 PointIndex) const { return true; }

	void AStateHandler::PrepareForTesting(PCGExData::FPointIO* PointIO)
	{
		const int32 NumPoints = PointIO->GetNum();
		Results.SetNum(NumPoints);
		for (int i = 0; i < NumPoints; i++) { Results[i] = false; }
	}

	void AStateHandler::PrepareForWriting(PCGExData::FPointIO* PointIO)
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

		InIfAttributes.Empty();
		InElseAttributes.Empty();

		OutIfAttributes.Empty();
		OutElseAttributes.Empty();

		UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;

		auto CreatePlaceholderAttributes = [&](
			TArray<PCGEx::FAttributesInfos*>& InfosList,
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

							FPCGMetadataAttributeBase* OutAttribute = Metadata->GetMutableAttribute(Att->Name);

							if (OutAttribute)
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

		if (bNeedIfs) { CreatePlaceholderAttributes(ADefinition->IfInfos, InIfAttributes, OutIfAttributes); }
		if (bNeedElses) { CreatePlaceholderAttributes(ADefinition->ElseInfos, InElseAttributes, OutElseAttributes); }
	}

	AStatesManager::AStatesManager(PCGExData::FPointIO* InPointIO)
		: PointIO(InPointIO)
	{
	}

	void AStatesManager::PrepareForTesting()
	{
		const int32 NumPoints = PointIO->GetNum();
		HighestState.SetNum(NumPoints);
		for (int i = 0; i < NumPoints; i++) { HighestState[i] = -1; }

		for (AStateHandler* Handler : Handlers) { Handler->PrepareForTesting(PointIO); }
	}

	void AStatesManager::Test(const int32 PointIndex)
	{
		int32 HState = -1;

		for (AStateHandler* Handler : Handlers)
		{
			const bool bValue = Handler->Test(PointIndex);
			Handler->Results[PointIndex] = bValue;
			if (bValue) { HState = Handler->Index; }
		}

		HighestState[PointIndex] = HState;
	}

	void AStatesManager::WriteStateNames(const FName AttributeName, const FName DefaultValue)
	{
		const int32 NumPoints = PointIO->GetOutNum();

		PCGEx::TFAttributeWriter<FName>* StateNameWriter = new PCGEx::TFAttributeWriter<FName>(AttributeName, DefaultValue, false);
		StateNameWriter->BindAndGet(*PointIO);
		
		for (int i = 0; i < NumPoints; i++)
		{
			if (const int32 HighestStateId = HighestState[i]; HighestStateId != -1) { StateNameWriter->Values[i] = Handlers[HighestStateId]->ADefinition->StateName; }
			else { StateNameWriter->Values[i] = DefaultValue; }
		}
		
		StateNameWriter->Write();		
		PCGEX_DELETE(StateNameWriter)
		
	}

	void AStatesManager::WriteStateValues(const FName AttributeName, const int32 DefaultValue)
	{
		const int32 NumPoints = PointIO->GetOutNum();

		PCGEx::TFAttributeWriter<int32>* StateValueWriter = new PCGEx::TFAttributeWriter<int32>(AttributeName, DefaultValue, false);
		StateValueWriter->BindAndGet(*PointIO);
		
		for (int i = 0; i < NumPoints; i++)
		{
			if (const int32 HighestStateId = HighestState[i]; HighestStateId != -1) { StateValueWriter->Values[i] = Handlers[HighestStateId]->ADefinition->StateId; }
			else { StateValueWriter->Values[i] = DefaultValue; }
		}
		
		StateValueWriter->Write();		
		PCGEX_DELETE(StateValueWriter)
		
	}

	void AStatesManager::WriteStateIndividualStates(FPCGExAsyncManager* AsyncManager)
	{
		for (AStateHandler* Handler : Handlers) { AsyncManager->Start<PCGExDataStateTask::FWriteIndividualState>(Handler->Index, PointIO, Handler); }
	}

	void AStatesManager::WriteStateAttributes(FPCGExAsyncManager* AsyncManager)
	{
		const int32 NumPoints = PointIO->GetNum();

		for (AStateHandler* Handler : Handlers)
		{
			Handler->PrepareForWriting(PointIO);

			if (!Handler->OverlappingAttributes.IsEmpty())
			{
				FString Names = FString::Join(Handler->OverlappingAttributes.Array(), TEXT(", "));
				PCGE_LOG_C(Warning, GraphAndLog, AsyncManager->Context, FText::Format(FTEXT("Some If/Else attributes ({0}) have the same name but different types, this will have unexpected results."), FText::FromString(Names)));
			}
		}

		for (int i = 0; i < NumPoints; i++) { AsyncManager->Start<PCGExDataStateTask::FWriteStateAttribute>(i, PointIO, this); }
	}
}

namespace PCGExDataStateTask
{
	bool FWriteStateAttribute::ExecuteTask()
	{
		const PCGMetadataEntryKey Key = PointIO->GetOutPoint(TaskIndex).MetadataEntry;

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

		for (PCGExDataState::AStateHandler* Handler : StateManager->Handlers)
		{
			if (Handler->Results[TaskIndex]) { ForwardValues(Handler->InIfAttributes, Handler->OutIfAttributes); }
			else { ForwardValues(Handler->InElseAttributes, Handler->OutElseAttributes); }
		}

		return true;
	}

	bool FWriteIndividualState::ExecuteTask()
	{
		PCGEx::TFAttributeWriter<bool>* StateWriter = new PCGEx::TFAttributeWriter<bool>(Handler->ADefinition->StateName);
		StateWriter->BindAndGet(*PointIO);

		const int32 NumPoints = PointIO->GetOutNum();
		for (int i = 0; i < NumPoints; i++) { StateWriter->Values[i] = Handler->Results[i]; }
		StateWriter->Write();

		PCGEX_DELETE(StateWriter)
		return true;
	}
}

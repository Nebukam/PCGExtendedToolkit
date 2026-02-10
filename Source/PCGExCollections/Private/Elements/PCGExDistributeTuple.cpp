// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExDistributeTuple.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGExRandomHelpers.h"

#if WITH_EDITOR
#include "UObject/UObjectGlobals.h"
#include "Editor.h"
#endif

#define LOCTEXT_NAMESPACE "PCGExDistributeTupleElement"
#define PCGEX_NAMESPACE DistributeTuple

#pragma region UPCGExDistributeTupleSettings

#if WITH_EDITOR
void UPCGExDistributeTupleSettings::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExDistributeTupleSettings::PostEditChangeProperty);

	bool bNeedsSync = false;
	bool bNeedsUIRefresh = false;

	if (PropertyChangedEvent.MemberProperty)
	{
		FName PropName = PropertyChangedEvent.MemberProperty->GetFName();
		EPropertyChangeType::Type ChangeType = PropertyChangedEvent.ChangeType;

		if (PropName == GET_MEMBER_NAME_CHECKED(UPCGExDistributeTupleSettings, Composition))
		{
			bNeedsSync = true;
			bNeedsUIRefresh = true;
		}
		else if (PropertyChangedEvent.MemberProperty->GetOwnerStruct() == FPCGExPropertySchema::StaticStruct())
		{
			bNeedsSync = true;
			bNeedsUIRefresh = true;
		}
		else if (PropName == GET_MEMBER_NAME_CHECKED(UPCGExDistributeTupleSettings, Values) && (ChangeType == EPropertyChangeType::ArrayAdd || ChangeType == EPropertyChangeType::ArrayRemove || ChangeType == EPropertyChangeType::ArrayClear || ChangeType == EPropertyChangeType::ArrayMove))
		{
			bNeedsSync = true;
		}
	}

	if (!bNeedsSync && !bNeedsUIRefresh)
	{
		Super::PostEditChangeProperty(PropertyChangedEvent);
		return;
	}

	if (bNeedsSync)
	{
		Composition.SyncAllSchemas();
		TArray<FInstancedStruct> Schema = Composition.BuildSchema();
		for (FPCGExWeightedPropertyOverrides& Row : Values) { Row.SyncToSchema(Schema); }
	}

	(void)MarkPackageDirty();

	if (bNeedsUIRefresh)
	{
		FProperty* ValuesProperty = FindFProperty<FProperty>(GetClass(), TEXT("Values"));
		if (ValuesProperty)
		{
			FPropertyChangedEvent RefreshEvent(ValuesProperty, EPropertyChangeType::ArrayClear);
			FCoreUObjectDelegates::OnObjectPropertyChanged.Broadcast(this, RefreshEvent);
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EIOInit UPCGExDistributeTupleSettings::GetMainDataInitializationPolicy() const
{
	return StealData == EPCGExOptionState::Enabled ? PCGExData::EIOInit::Forward : PCGExData::EIOInit::Duplicate;
}

PCGEX_INITIALIZE_ELEMENT(DistributeTuple)
PCGEX_ELEMENT_BATCH_POINT_IMPL(DistributeTuple)

#pragma endregion

#pragma region FPCGExDistributeTupleElement

bool FPCGExDistributeTupleElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(DistributeTuple)

	if (!Settings->Composition.IsEmpty() && !Settings->Values.IsEmpty())
	{
		TArray<FName> Duplicates;
		if (!Settings->Composition.ValidateUniqueNames(Duplicates))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Composition has duplicate column names."));
			return false;
		}

		if (Settings->bOutputRowIndex)
		{
			PCGEX_VALIDATE_NAME(Settings->RowIndexAttributeName)
		}

		if (Settings->bOutputWeight)
		{
			PCGEX_VALIDATE_NAME(Settings->WeightAttributeName)
		}
	}

	return true;
}

bool FPCGExDistributeTupleElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDistributeTupleElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(DistributeTuple)
	PCGEX_EXECUTION_CHECK

	if (Settings->Composition.IsEmpty() || Settings->Values.IsEmpty())
	{
		DisabledPassThroughData(InContext);
		Context->Done();
		return Context->TryComplete();
	}

	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				NewBatch->bSkipCompletion = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->MainPoints->StageOutputs();
	Context->Done();

	return Context->TryComplete();
}

#pragma endregion

#pragma region PCGExDistributeTuple::FProcessor

namespace PCGExDistributeTuple
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExDistributeTuple::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, Settings->GetMainDataInitializationPolicy())

		NumRows = Settings->Values.Num();
		if (NumRows == 0) { return false; }

		// Build cumulative weight table
		CumulativeWeights.SetNum(NumRows);
		TotalWeight = 0;
		for (int32 i = 0; i < NumRows; ++i)
		{
			TotalWeight += FMath::Max(0, Settings->Values[i].Weight);
			CumulativeWeights[i] = TotalWeight;
		}

		if (TotalWeight == 0 && Settings->Distribution == EPCGExDistribution::WeightedRandom)
		{
			// All weights are zero - fall back to uniform random
			TotalWeight = NumRows;
			for (int32 i = 0; i < NumRows; ++i) { CumulativeWeights[i] = i + 1; }
		}

		// Initialize per-column output buffers
		const int32 NumColumns = Settings->Composition.Num();
		Columns.SetNum(NumColumns);

		for (int32 ColIdx = 0; ColIdx < NumColumns; ++ColIdx)
		{
			const FPCGExPropertySchema& Schema = Settings->Composition.Schemas[ColIdx];
			const FPCGExProperty* SchemaProperty = Schema.GetProperty();

			if (!SchemaProperty || !SchemaProperty->SupportsOutput())
			{
				continue;
			}

			FColumnOutput& Col = Columns[ColIdx];

			// Deep-copy the schema property so we own the output buffer
			Col.OwnedProperty = Schema.Property;

			FPCGExProperty* OutputProperty = Col.OwnedProperty.GetMutablePtr<FPCGExProperty>();
			if (!OutputProperty || !OutputProperty->InitializeOutput(PointDataFacade, Schema.Name))
			{
				Col.OwnedProperty.Reset();
				continue;
			}

			Col.WriterPtr = OutputProperty;

			// Build per-row source lookup
			Col.RowSources.SetNum(NumRows);
			for (int32 RowIdx = 0; RowIdx < NumRows; ++RowIdx)
			{
				const FPCGExWeightedPropertyOverrides& Row = Settings->Values[RowIdx];
				if (Row.IsOverrideEnabled(ColIdx))
				{
					Col.RowSources[RowIdx] = Row.Overrides[ColIdx].GetProperty();
				}
				else
				{
					Col.RowSources[RowIdx] = nullptr;
				}
			}
		}

		// Optional output writers
		if (Settings->bOutputRowIndex)
		{
			RowIndexWriter = PointDataFacade->GetWritable<int32>(Settings->RowIndexAttributeName, PCGExData::EBufferInit::New);
		}

		if (Settings->bOutputWeight)
		{
			WeightWriter = PointDataFacade->GetWritable<int32>(Settings->WeightAttributeName, PCGExData::EBufferInit::New);
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExDistributeTuple::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		const UPCGBasePointData* OutPointData = PointDataFacade->GetOut();
		const TConstPCGValueRange<int32> Seeds = OutPointData->GetConstSeedValueRange();
		const UPCGComponent* Component = Context->GetComponent();
		const int32 MaxRowIndex = NumRows - 1;

		PCGEX_SCOPE_LOOP(Index)
		{
			int32 PickedRow = -1;

			switch (Settings->Distribution)
			{
			case EPCGExDistribution::Index:
				{
					PickedRow = PCGExMath::SanitizeIndex(Index, MaxRowIndex, Settings->IndexSafety);
					if (PickedRow < 0) { continue; }
				}
				break;

			case EPCGExDistribution::Random:
				{
					const int32 Seed = PCGExRandomHelpers::GetSeed(Seeds[Index], Settings->SeedComponents, Settings->LocalSeed, Settings, Component);
					FRandomStream RandomStream(Seed);
					PickedRow = RandomStream.RandRange(0, MaxRowIndex);
				}
				break;

			case EPCGExDistribution::WeightedRandom:
				{
					const int32 Seed = PCGExRandomHelpers::GetSeed(Seeds[Index], Settings->SeedComponents, Settings->LocalSeed, Settings, Component);
					FRandomStream RandomStream(Seed);
					const int32 Roll = RandomStream.RandRange(1, TotalWeight);

					// Binary search through cumulative weights
					int32 Lo = 0, Hi = MaxRowIndex;
					while (Lo < Hi)
					{
						const int32 Mid = (Lo + Hi) >> 1;
						if (CumulativeWeights[Mid] < Roll) { Lo = Mid + 1; }
						else { Hi = Mid; }
					}
					PickedRow = Lo;
				}
				break;
			}

			if (PickedRow < 0 || PickedRow > MaxRowIndex) { continue; }

			// Write optional outputs
			if (RowIndexWriter) { RowIndexWriter->SetValue(Index, PickedRow); }
			if (WeightWriter) { WeightWriter->SetValue(Index, Settings->Values[PickedRow].Weight); }

			// Write column values from the picked row
			for (const FColumnOutput& Col : Columns)
			{
				if (!Col.WriterPtr) { continue; }

				const FPCGExProperty* RowSource = Col.RowSources[PickedRow];
				if (!RowSource) { continue; }

				Col.WriterPtr->WriteOutputFrom(Index, RowSource);
			}
		}
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		PointDataFacade->WriteFastest(TaskManager);
	}
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

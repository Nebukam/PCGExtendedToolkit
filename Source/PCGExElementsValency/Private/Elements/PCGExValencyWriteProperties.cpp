// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExValencyWriteProperties.h"

#include "PCGParamData.h"
#include "Core/PCGExValencyBondingRules.h"
#include "Data/PCGExData.h"

#define LOCTEXT_NAMESPACE "PCGExValencyWriteProperties"
#define PCGEX_NAMESPACE ValencyWriteProperties

PCGEX_INITIALIZE_ELEMENT(ValencyWriteProperties)
PCGEX_ELEMENT_BATCH_POINT_IMPL(ValencyWriteProperties)

#pragma region UPCGExValencyWritePropertiesSettings

PCGExData::EIOInit UPCGExValencyWritePropertiesSettings::GetMainDataInitializationPolicy() const
{
	return PCGExData::EIOInit::Duplicate;
}

TArray<FPCGPinProperties> UPCGExValencyWritePropertiesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAM(PCGExValency::Labels::SourceValencyMapLabel, "Valency map from Solve or Generative nodes.", Required)
	return PinProperties;
}

#if WITH_EDITOR
void UPCGExValencyWritePropertiesSettings::ImportBondingRulesPropertyOutputConfigs()
{
	// This is a convenience function to auto-populate property configs from the first
	// BondingRules found. At edit time, we don't have a Valency Map yet, so the user
	// would manually set a BondingRules reference. For now, log guidance.
	UE_LOG(LogTemp, Log, TEXT("ImportBondingRulesPropertyOutputConfigs: Use AutoPopulateFromRules() at runtime with compiled bonding rules."));
}
#endif

#pragma endregion

#pragma region FPCGExValencyWritePropertiesElement

bool FPCGExValencyWritePropertiesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(ValencyWriteProperties)

	// Create and unpack Valency Map
	Context->ValencyUnpacker = MakeShared<PCGExValency::FValencyUnpacker>();
	Context->ValencyUnpacker->UnpackPin(InContext, PCGExValency::Labels::SourceValencyMapLabel);

	if (!Context->ValencyUnpacker->HasValidMapping())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Could not rebuild a valid Valency Map from the provided input."));
		return false;
	}

	if (!Settings->PropertiesOutput.HasOutputs() && !Settings->bOutputModuleName)
	{
		PCGE_LOG(Warning, GraphAndLog, FTEXT("No property or module name outputs configured."));
	}

	// Ensure all BondingRules are compiled
	for (const auto& Pair : Context->ValencyUnpacker->GetBondingRules())
	{
		UPCGExValencyBondingRules* Rules = Pair.Value;
		if (Rules && !Rules->IsCompiled())
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(
				FTEXT("BondingRules '{0}' from Valency Map is not compiled - properties may be unavailable."),
				FText::FromString(Rules->GetName())));
		}
	}

	return true;
}

bool FPCGExValencyWritePropertiesElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	PCGEX_CONTEXT_AND_SETTINGS(ValencyWriteProperties)
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

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->MainPoints->StageOutputs();
	return Context->TryComplete();
}

#pragma endregion

#pragma region PCGExValencyWriteProperties::FProcessor

namespace PCGExValencyWriteProperties
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExValencyWriteProperties::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, Settings->GetMainDataInitializationPolicy())

		// Read ValencyEntry hashes
		const FName EntryAttrName = PCGExValency::EntryData::GetEntryAttributeName(Settings->EntrySuffix);
		ValencyEntryReader = PointDataFacade->GetReadable<int64>(EntryAttrName, PCGExData::EIOSide::In, true);
		if (!ValencyEntryReader)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(
				FTEXT("ValencyEntry attribute '{0}' not found. Run Valency : Solve first."),
				FText::FromName(EntryAttrName)));
			return false;
		}

		// Create module name writer if configured
		if (Settings->bOutputModuleName)
		{
			ModuleNameWriter = PointDataFacade->GetWritable<FName>(
				Settings->ModuleNameAttributeName, NAME_None, true, PCGExData::EBufferInit::Inherit);
		}

		// Initialize property writers for each BondingRules in the Valency Map
		if (Settings->PropertiesOutput.HasOutputs())
		{
			for (const auto& Pair : Context->ValencyUnpacker->GetBondingRules())
			{
				UPCGExValencyBondingRules* Rules = Pair.Value;
				if (!Rules || !Rules->IsCompiled()) { continue; }

				const FPCGExValencyBondingRulesCompiled* CompiledRules = Rules->GetCompiledData();
				if (!CompiledRules) { continue; }

				TSharedPtr<FPCGExValencyPropertyWriter> Writer = MakeShared<FPCGExValencyPropertyWriter>();
				if (Writer->Initialize(Rules, CompiledRules, PointDataFacade, Settings->PropertiesOutput))
				{
					PropertyWriters.Add(Rules, Writer);
				}
			}
		}

		if (!ModuleNameWriter && PropertyWriters.IsEmpty())
		{
			// Nothing to write
			return false;
		}

		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExValencyWriteProperties::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		PCGEX_SCOPE_LOOP(Index)
		{
			const uint64 ValencyHash = ValencyEntryReader->Read(Index);
			if (ValencyHash == PCGExValency::EntryData::INVALID_ENTRY) { continue; }

			// Resolve ValencyEntry -> BondingRules + ModuleIndex
			uint16 ModuleIndex = 0;
			uint16 PatternFlags = 0;
			UPCGExValencyBondingRules* Rules = Context->ValencyUnpacker->ResolveEntry(ValencyHash, ModuleIndex, PatternFlags);
			if (!Rules || !Rules->IsCompiled()) { continue; }

			const FPCGExValencyBondingRulesCompiled* CompiledRules = Rules->GetCompiledData();
			if (ModuleIndex >= CompiledRules->ModuleCount) { continue; }

			// Write module name
			if (ModuleNameWriter)
			{
				ModuleNameWriter->SetValue(Index, CompiledRules->ModuleNames[ModuleIndex]);
			}

			// Write properties via the appropriate writer
			if (const TSharedPtr<FPCGExValencyPropertyWriter>* WriterPtr = PropertyWriters.Find(Rules))
			{
				(*WriterPtr)->WriteModuleProperties(Index, ModuleIndex);
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->WriteFastest(TaskManager);
	}
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

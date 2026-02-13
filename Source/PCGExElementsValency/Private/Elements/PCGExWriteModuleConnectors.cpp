// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExWriteModuleConnectors.h"

#include "Core/PCGExValencyConnectorSet.h"
#include "Core/PCGExValencyBondingRules.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Data/Utils/PCGExDataPreloader.h"

#define LOCTEXT_NAMESPACE "PCGExWriteModuleConnectors"
#define PCGEX_NAMESPACE WriteModuleConnectors

TArray<FPCGPinProperties> UPCGExWriteModuleConnectorsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(TEXT("Connectors"), "Connector points for chaining to next solve", Required)
	return PinProperties;
}

PCGExData::EIOInit UPCGExWriteModuleConnectorsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Forward; }
PCGExData::EIOInit UPCGExWriteModuleConnectorsSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Forward; }

void FPCGExWriteModuleConnectorsContext::RegisterAssetDependencies()
{
	FPCGExValencyProcessorContext::RegisterAssetDependencies();

	const UPCGExWriteModuleConnectorsSettings* Settings = GetInputSettings<UPCGExWriteModuleConnectorsSettings>();
	if (Settings && !Settings->ConnectorSet.IsNull())
	{
		AddAssetDependency(Settings->ConnectorSet.ToSoftObjectPath());
	}
}

PCGEX_INITIALIZE_ELEMENT(WriteModuleConnectors)
PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(WriteModuleConnectors)

bool FPCGExWriteModuleConnectorsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExValencyProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteModuleConnectors)

	if (Settings->ConnectorSet.IsNull())
	{
		if (!Settings->bQuietMissingConnectorSet) { PCGE_LOG(Error, GraphAndLog, FTEXT("No Connector Set provided.")); }
		return false;
	}

	// Create output collection for connector points
	Context->ConnectorOutputCollection = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->ConnectorOutputCollection->OutputPin = FName("Connectors");

	return true;
}

void FPCGExWriteModuleConnectorsElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	FPCGExValencyProcessorElement::PostLoadAssetsDependencies(InContext);

	PCGEX_CONTEXT_AND_SETTINGS(WriteModuleConnectors)

	if (!Context->ConnectorSet && !Settings->ConnectorSet.IsNull())
	{
		Context->ConnectorSet = Settings->ConnectorSet.Get();
	}
}

bool FPCGExWriteModuleConnectorsElement::PostBoot(FPCGExContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(WriteModuleConnectors)

	if (!FPCGExValencyProcessorElement::PostBoot(InContext)) { return false; }

	if (!Context->ConnectorSet)
	{
		if (!Settings->bQuietMissingConnectorSet) { PCGE_LOG(Error, GraphAndLog, FTEXT("No Connector Set provided.")); }
		return false;
	}

	// Validate connector set
	TArray<FText> ValidationErrors;
	if (!Context->ConnectorSet->Validate(ValidationErrors))
	{
		for (const FText& Error : ValidationErrors) { PCGE_LOG(Error, GraphAndLog, Error); }
		return false;
	}

	// Compile connector set
	Context->ConnectorSet->Compile();

	// Validate bonding rules (required for module connector lookup)
	if (!Context->BondingRules)
	{
		if (!Settings->bQuietMissingBondingRules) { PCGE_LOG(Error, GraphAndLog, FTEXT("No Bonding Rules provided.")); }
		return false;
	}

	if (!Context->BondingRules->IsCompiled())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Bonding Rules are not compiled."));
		return false;
	}

	return true;
}

bool FPCGExWriteModuleConnectorsElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	PCGEX_CONTEXT_AND_SETTINGS(WriteModuleConnectors)

	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartProcessingClusters(
			[](const TSharedPtr<PCGExData::FPointIOTaggedEntries>& Entries) { return true; },
			[&](const TSharedPtr<PCGExClusterMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not build any clusters."));
		}
	}

	PCGEX_CLUSTER_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	// Output the connector points
	Context->ConnectorOutputCollection->StageOutputs();
	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExWriteModuleConnectors
{
#pragma region FProcessor

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteModuleConnectors::Process);

		if (!PCGExValencyMT::IProcessor::Process(InTaskManager)) { return false; }

		// Get the module data reader
		ModuleDataReader = VtxDataFacade->GetReadable<int64>(Settings->ModuleDataAttributeName);
		if (!ModuleDataReader)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(
				FTEXT("Module data attribute '{0}' not found on vertices."),
				FText::FromName(Settings->ModuleDataAttributeName)));
			return false;
		}

		// Count total output connectors to pre-allocate
		const FPCGExValencyBondingRulesCompiled* CompiledRules = Context->BondingRules->GetCompiledData();
		if (!CompiledRules)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Bonding rules not compiled."));
			return false;
		}

		// First pass: count connectors
		const int32 NumVertices = VtxDataFacade->GetNum(PCGExData::EIOSide::In);
		int32 TotalConnectorCount = 0;

		for (int32 i = 0; i < NumVertices; ++i)
		{
			const int64 ModuleData = ModuleDataReader->Read(i);
			const int32 ModuleIndex = static_cast<int32>(ModuleData & 0xFFFFFFFF);

			if (ModuleIndex >= 0 && ModuleIndex < Context->BondingRules->Modules.Num())
			{
				const FPCGExValencyModuleDefinition& Module = Context->BondingRules->Modules[ModuleIndex];
				for (const FPCGExValencyModuleConnector& Connector : Module.Connectors)
				{
					if (Connector.Polarity == EPCGExConnectorPolarity::Plug)
					{
						TotalConnectorCount++;
					}
				}
			}
		}

		if (TotalConnectorCount == 0)
		{
			// No output connectors to write
			return true;
		}

		// Create output point data
		ConnectorOutput = Context->ConnectorOutputCollection->Emplace_GetRef(PCGExData::EIOInit::New);
		if (!ConnectorOutput)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Failed to create connector output."));
			return false;
		}

		// Pre-allocate points
		ConnectorOutput->GetOut()->SetNumPoints(TotalConnectorCount);

		// Create output attributes
		FPCGMetadataAttribute<int64>* ConnectorRefAttr = ConnectorOutput->CreateAttribute<int64>(
			Settings->ConnectorOutputAttributeName, PCGExValencyConnector::INVALID_CONNECTOR, true, true);

		FPCGMetadataAttribute<int32>* SourceIndexAttr = nullptr;
		if (Settings->bOutputSourceIndex)
		{
			SourceIndexAttr = ConnectorOutput->CreateAttribute<int32>(
				Settings->SourceIndexAttributeName, -1, false, true);
		}

		FPCGMetadataAttribute<FName>* ConnectorNameAttr = nullptr;
		if (Settings->bOutputConnectorName)
		{
			ConnectorNameAttr = ConnectorOutput->CreateAttribute<FName>(
				Settings->ConnectorNameAttributeName, NAME_None, false, true);
		}

		FPCGMetadataAttribute<FName>* ConnectorTypeAttr = nullptr;
		if (Settings->bOutputConnectorType)
		{
			ConnectorTypeAttr = ConnectorOutput->CreateAttribute<FName>(
				Settings->ConnectorTypeAttributeName, NAME_None, false, true);
		}

		// Get transform ranges
		TConstPCGValueRange<FTransform> InTransforms = VtxDataFacade->GetIn()->GetConstTransformValueRange();
		TPCGValueRange<FTransform> OutTransforms = ConnectorOutput->GetOut()->GetTransformValueRange();

		// Second pass: write connector points
		int32 ConnectorIndex = 0;
		for (int32 VertexIdx = 0; VertexIdx < NumVertices; ++VertexIdx)
		{
			const int64 ModuleData = ModuleDataReader->Read(VertexIdx);
			const int32 ModuleIndex = static_cast<int32>(ModuleData & 0xFFFFFFFF);

			if (ModuleIndex < 0 || ModuleIndex >= Context->BondingRules->Modules.Num())
			{
				continue;
			}

			const FPCGExValencyModuleDefinition& Module = Context->BondingRules->Modules[ModuleIndex];
			const FTransform& VertexTransform = InTransforms[VertexIdx];

			for (int32 ConnectorDefIdx = 0; ConnectorDefIdx < Module.Connectors.Num(); ++ConnectorDefIdx)
			{
				const FPCGExValencyModuleConnector& Connector = Module.Connectors[ConnectorDefIdx];
				if (Connector.Polarity != EPCGExConnectorPolarity::Plug)
				{
					continue;
				}

				// Compute connector transform
				const FTransform ConnectorOffset = Connector.GetEffectiveOffset(Context->ConnectorSet);
				const FTransform ConnectorTransform = ConnectorOffset * VertexTransform;

				// Write transform
				OutTransforms[ConnectorIndex] = ConnectorTransform;

				// Write connector reference
				const int32 ConnectorTypeIndex = Context->ConnectorSet->FindConnectorTypeIndex(Connector.ConnectorType);
				const int64 PackedRef = (ConnectorTypeIndex >= 0) ? PCGExValencyConnector::Pack(0, static_cast<uint16>(ConnectorTypeIndex)) : PCGExValencyConnector::INVALID_CONNECTOR;
				ConnectorRefAttr->SetValue(ConnectorIndex, PackedRef);

				// Write optional attributes
				if (SourceIndexAttr)
				{
					SourceIndexAttr->SetValue(ConnectorIndex, VertexIdx);
				}
				if (ConnectorNameAttr)
				{
					ConnectorNameAttr->SetValue(ConnectorIndex, Connector.ConnectorName);
				}
				if (ConnectorTypeAttr)
				{
					ConnectorTypeAttr->SetValue(ConnectorIndex, Connector.ConnectorType);
				}

				ConnectorIndex++;
			}
		}

		ConnectorCount = ConnectorIndex;
		return true;
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		// Processing is done in Process() since we need sequential connector index assignment
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		if (ConnectorCount > 0)
		{
			PCGE_LOG_C(Verbose, GraphAndLog, Context, FText::Format(
				FTEXT("WriteModuleConnectors: Wrote {0} connector points."),
				FText::AsNumber(ConnectorCount)));
		}
	}

#pragma endregion

#pragma region FBatch

	FBatch::FBatch(FPCGExContext* InContext, const TSharedRef<PCGExData::FPointIO>& InVtx, TArrayView<TSharedRef<PCGExData::FPointIO>> InEdges)
		: TBatch(InContext, InVtx, InEdges)
	{
	}

	FBatch::~FBatch()
	{
	}

	void FBatch::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		PCGExValencyMT::IBatch::RegisterBuffersDependencies(FacadePreloader);

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteModuleConnectors)

		// Register module data attribute for reading
		FacadePreloader.Register<int64>(Context, Settings->ModuleDataAttributeName);
	}

	void FBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteModuleConnectors)

		// Create module data reader
		ModuleDataReader = VtxDataFacade->GetReadable<int64>(Settings->ModuleDataAttributeName);

		PCGExValencyMT::IBatch::OnProcessingPreparationComplete();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<PCGExClusterMT::IProcessor>& InProcessor)
	{
		if (!PCGExValencyMT::IBatch::PrepareSingle(InProcessor)) { return false; }

		FProcessor* TypedProcessor = static_cast<FProcessor*>(InProcessor.Get());
		TypedProcessor->ModuleDataReader = ModuleDataReader;

		return true;
	}

	void FBatch::CompleteWork()
	{
		PCGExValencyMT::IBatch::CompleteWork();
	}

#pragma endregion
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

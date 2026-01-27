// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExWriteModuleSockets.h"

#include "Core/PCGExValencySocketRules.h"
#include "Core/PCGExValencyBondingRules.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Data/Utils/PCGExDataPreloader.h"

#define LOCTEXT_NAMESPACE "PCGExWriteModuleSockets"
#define PCGEX_NAMESPACE WriteModuleSockets

TArray<FPCGPinProperties> UPCGExWriteModuleSocketsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(TEXT("Sockets"), "Socket points for chaining to next solve", Required)
	return PinProperties;
}

PCGExData::EIOInit UPCGExWriteModuleSocketsSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Forward; }
PCGExData::EIOInit UPCGExWriteModuleSocketsSettings::GetEdgeOutputInitMode() const { return PCGExData::EIOInit::Forward; }

void FPCGExWriteModuleSocketsContext::RegisterAssetDependencies()
{
	FPCGExValencyProcessorContext::RegisterAssetDependencies();

	const UPCGExWriteModuleSocketsSettings* Settings = GetInputSettings<UPCGExWriteModuleSocketsSettings>();
	if (Settings && !Settings->SocketRules.IsNull())
	{
		AddAssetDependency(Settings->SocketRules.ToSoftObjectPath());
	}
}

PCGEX_INITIALIZE_ELEMENT(WriteModuleSockets)
PCGEX_ELEMENT_BATCH_EDGE_IMPL_ADV(WriteModuleSockets)

bool FPCGExWriteModuleSocketsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExValencyProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteModuleSockets)

	if (Settings->SocketRules.IsNull())
	{
		if (!Settings->bQuietMissingSocketRules) { PCGE_LOG(Error, GraphAndLog, FTEXT("No Socket Rules provided.")); }
		return false;
	}

	// Create output collection for socket points
	Context->SocketOutputCollection = MakeShared<PCGExData::FPointIOCollection>(Context);
	Context->SocketOutputCollection->OutputPin = FName("Sockets");

	return true;
}

void FPCGExWriteModuleSocketsElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	FPCGExValencyProcessorElement::PostLoadAssetsDependencies(InContext);

	PCGEX_CONTEXT_AND_SETTINGS(WriteModuleSockets)

	if (!Context->SocketRules && !Settings->SocketRules.IsNull())
	{
		Context->SocketRules = Settings->SocketRules.Get();
	}
}

bool FPCGExWriteModuleSocketsElement::PostBoot(FPCGExContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(WriteModuleSockets)

	if (!FPCGExValencyProcessorElement::PostBoot(InContext)) { return false; }

	if (!Context->SocketRules)
	{
		if (!Settings->bQuietMissingSocketRules) { PCGE_LOG(Error, GraphAndLog, FTEXT("No Socket Rules provided.")); }
		return false;
	}

	// Validate socket rules
	TArray<FText> ValidationErrors;
	if (!Context->SocketRules->Validate(ValidationErrors))
	{
		for (const FText& Error : ValidationErrors) { PCGE_LOG(Error, GraphAndLog, Error); }
		return false;
	}

	// Compile socket rules
	Context->SocketRules->Compile();

	// Validate bonding rules (required for module socket lookup)
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

bool FPCGExWriteModuleSocketsElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	PCGEX_CONTEXT_AND_SETTINGS(WriteModuleSockets)

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

	// Output the socket points
	Context->SocketOutputCollection->StageOutputs();
	Context->OutputPointsAndEdges();

	return Context->TryComplete();
}

namespace PCGExWriteModuleSockets
{
#pragma region FProcessor

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteModuleSockets::Process);

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

		// Count total output sockets to pre-allocate
		const FPCGExValencyBondingRulesCompiled* CompiledRules = Context->BondingRules->GetCompiledData();
		if (!CompiledRules)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Bonding rules not compiled."));
			return false;
		}

		// First pass: count sockets
		const int32 NumVertices = VtxDataFacade->GetNum(PCGExData::EIOSide::In);
		int32 TotalSocketCount = 0;

		for (int32 i = 0; i < NumVertices; ++i)
		{
			const int64 ModuleData = ModuleDataReader->Read(i);
			const int32 ModuleIndex = static_cast<int32>(ModuleData & 0xFFFFFFFF);

			if (ModuleIndex >= 0 && ModuleIndex < Context->BondingRules->Modules.Num())
			{
				const FPCGExValencyModuleDefinition& Module = Context->BondingRules->Modules[ModuleIndex];
				for (const FPCGExValencyModuleSocket& Socket : Module.Sockets)
				{
					if (Socket.bIsOutputSocket)
					{
						TotalSocketCount++;
					}
				}
			}
		}

		if (TotalSocketCount == 0)
		{
			// No output sockets to write
			return true;
		}

		// Create output point data
		SocketOutput = Context->SocketOutputCollection->Emplace_GetRef(PCGExData::EIOInit::New);
		if (!SocketOutput)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Failed to create socket output."));
			return false;
		}

		// Pre-allocate points
		SocketOutput->GetOut()->SetNumPoints(TotalSocketCount);

		// Create output attributes
		FPCGMetadataAttribute<int64>* SocketRefAttr = SocketOutput->CreateAttribute<int64>(
			Settings->SocketOutputAttributeName, PCGExValencySocket::INVALID_SOCKET, true, true);

		FPCGMetadataAttribute<int32>* SourceIndexAttr = nullptr;
		if (Settings->bOutputSourceIndex)
		{
			SourceIndexAttr = SocketOutput->CreateAttribute<int32>(
				Settings->SourceIndexAttributeName, -1, false, true);
		}

		FPCGMetadataAttribute<FName>* SocketNameAttr = nullptr;
		if (Settings->bOutputSocketName)
		{
			SocketNameAttr = SocketOutput->CreateAttribute<FName>(
				Settings->SocketNameAttributeName, NAME_None, false, true);
		}

		FPCGMetadataAttribute<FName>* SocketTypeAttr = nullptr;
		if (Settings->bOutputSocketType)
		{
			SocketTypeAttr = SocketOutput->CreateAttribute<FName>(
				Settings->SocketTypeAttributeName, NAME_None, false, true);
		}

		// Get transform ranges
		TConstPCGValueRange<FTransform> InTransforms = VtxDataFacade->GetIn()->GetConstTransformValueRange();
		TPCGValueRange<FTransform> OutTransforms = SocketOutput->GetOut()->GetTransformValueRange();

		// Second pass: write socket points
		int32 SocketIndex = 0;
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

			for (int32 SocketDefIdx = 0; SocketDefIdx < Module.Sockets.Num(); ++SocketDefIdx)
			{
				const FPCGExValencyModuleSocket& Socket = Module.Sockets[SocketDefIdx];
				if (!Socket.bIsOutputSocket)
				{
					continue;
				}

				// Compute socket transform
				const FTransform SocketOffset = Socket.GetEffectiveOffset(Context->SocketRules);
				const FTransform SocketTransform = SocketOffset * VertexTransform;

				// Write transform
				OutTransforms[SocketIndex] = SocketTransform;

				// Write socket reference
				const int32 SocketTypeIndex = Context->SocketRules->FindSocketTypeIndex(Socket.SocketType);
				const int64 PackedRef = (SocketTypeIndex >= 0) ? PCGExValencySocket::Pack(0, static_cast<uint16>(SocketTypeIndex)) : PCGExValencySocket::INVALID_SOCKET;
				SocketRefAttr->SetValue(SocketIndex, PackedRef);

				// Write optional attributes
				if (SourceIndexAttr)
				{
					SourceIndexAttr->SetValue(SocketIndex, VertexIdx);
				}
				if (SocketNameAttr)
				{
					SocketNameAttr->SetValue(SocketIndex, Socket.SocketName);
				}
				if (SocketTypeAttr)
				{
					SocketTypeAttr->SetValue(SocketIndex, Socket.SocketType);
				}

				SocketIndex++;
			}
		}

		SocketCount = SocketIndex;
		return true;
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		// Processing is done in Process() since we need sequential socket index assignment
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		if (SocketCount > 0)
		{
			PCGE_LOG_C(Verbose, GraphAndLog, Context, FText::Format(
				FTEXT("WriteModuleSockets: Wrote {0} socket points."),
				FText::AsNumber(SocketCount)));
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

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteModuleSockets)

		// Register module data attribute for reading
		FacadePreloader.Register<int64>(Context, Settings->ModuleDataAttributeName);
	}

	void FBatch::OnProcessingPreparationComplete()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(WriteModuleSockets)

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

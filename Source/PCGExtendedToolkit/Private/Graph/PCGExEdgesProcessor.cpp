// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExEdgesProcessor.h"

#include "IPCGExDebug.h"
#include "Data/PCGExGraphParamsData.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

#pragma region UPCGSettings interface


PCGExData::EInit UPCGExEdgesProcessorSettings::GetMainOutputInitMode() const { return PCGExData::EInit::Forward; }

PCGExData::EInit UPCGExEdgesProcessorSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::Forward; }

bool UPCGExEdgesProcessorSettings::GetMainAcceptMultipleData() const { return true; }

TArray<FPCGPinProperties> UPCGExEdgesProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	FPCGPinProperties& PinEdgesInput = PinProperties.Emplace_GetRef(PCGExGraph::SourceEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinEdgesInput.Tooltip = FTEXT("Edges associated with the main input points");
#endif // WITH_EDITOR

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExEdgesProcessorSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	FPCGPinProperties& PinEdgesOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputEdgesLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinEdgesOutput.Tooltip = FTEXT("Edges associated with the main output points");
#endif // WITH_EDITOR

	return PinProperties;
}

bool UPCGExEdgesProcessorSettings::GetCacheAllMeshes() const { return false; }

#pragma endregion

FPCGExEdgesProcessorContext::~FPCGExEdgesProcessorContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(Edges)
	PCGEX_DELETE(BoundEdges)

	if (bCacheAllMeshes)
	{
		CurrentMesh = nullptr;
		PCGEX_DELETE_TARRAY(Meshes)
	}
	else { PCGEX_DELETE(CurrentMesh) }
}


bool FPCGExEdgesProcessorContext::AdvanceAndBindPointsIO()
{
	PCGEX_DELETE_TARRAY(Meshes)
	PCGEX_DELETE(BoundEdges)
	CurrentEdgesIndex = -1;

	if (!AdvancePointsIO()) { return false; }

	BoundEdges = new PCGExData::FKPointIOMarkedBindings<int32>(CurrentIO, PCGExGraph::PUIDAttributeName);
	Edges->ForEach(
		[&](PCGExData::FPointIO& InEdgesData, int32)
		{
			if (BoundEdges->IsMatching(InEdgesData)) { BoundEdges->Add(InEdgesData); }
		});

	return true;
}

bool FPCGExEdgesProcessorContext::AdvanceEdges()
{
	if (!bCacheAllMeshes) { PCGEX_DELETE(CurrentMesh) }

	if (CurrentEdges) { CurrentEdges->Cleanup(); }

	if (Edges->Pairs.IsValidIndex(++CurrentEdgesIndex))
	{
		CurrentEdges = Edges->Pairs[CurrentEdgesIndex];

		CurrentMesh = new PCGExMesh::FMesh();
		CurrentIO->CreateInKeys();
		CurrentEdges->CreateInKeys();
		CurrentMesh->BuildFrom(*CurrentIO, *CurrentEdges);

		if (bCacheAllMeshes) { Meshes.Add(CurrentMesh); }

		return true;
	}

	CurrentEdges = nullptr;
	return false;
}

void FPCGExEdgesProcessorContext::OutputPointsAndEdges()
{
	MainPoints->OutputTo(this);
	Edges->OutputTo(this);
}

PCGEX_INITIALIZE_CONTEXT(EdgesProcessor)

bool FPCGExEdgesProcessorElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(EdgesProcessor)

	if (Context->Edges->IsEmpty())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing Edges."));
		return false;
	}

	return true;
}

FPCGContext* FPCGExEdgesProcessorElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExPointsProcessorElementBase::InitializeContext(InContext, InputData, SourceComponent, Node);

	PCGEX_CONTEXT_AND_SETTINGS(EdgesProcessor)

	Context->Edges = new PCGExData::FPointIOGroup();
	Context->Edges->DefaultOutputLabel = PCGExGraph::OutputEdgesLabel;
	TArray<FPCGTaggedData> Sources = Context->InputData.GetInputsByPin(PCGExGraph::SourceEdgesLabel);
	Context->Edges->Initialize(Context, Sources, Settings->GetEdgeOutputInitMode());

	Context->bCacheAllMeshes = Settings->GetCacheAllMeshes();

	return Context;
}

#undef LOCTEXT_NAMESPACE

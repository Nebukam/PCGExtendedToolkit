// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Graph/PCGExPackClusters.h"

#include "Data/PCGExPointIOMerger.h"
#include "Geometry/PCGExGeoDelaunay.h"

#define LOCTEXT_NAMESPACE "PCGExPackClusters"
#define PCGEX_NAMESPACE PackClusters

UPCGExPackClustersSettings::UPCGExPackClustersSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

PCGExData::EInit UPCGExPackClustersSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGExData::EInit UPCGExPackClustersSettings::GetEdgeOutputInitMode() const { return PCGExData::EInit::NoOutput; }

TArray<FPCGPinProperties> UPCGExPackClustersSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& PinEdgesOutput = PinProperties.Emplace_GetRef(PCGExGraph::OutputPackedClustersLabel, EPCGDataType::Point);

#if WITH_EDITOR
	PinEdgesOutput.Tooltip = FTEXT("Individually packed clusters");
#endif

	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(PackClusters)


FPCGExPackClustersContext::~FPCGExPackClustersContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(PackedClusters)
	PCGEX_DELETE(VtxAttributes)
}


bool FPCGExPackClustersElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExEdgesProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PackClusters)

	Context->PackedClusters = new PCGExData::FPointIOGroup();
	Context->PackedClusters->DefaultOutputLabel = PCGExGraph::OutputPackedClustersLabel;

	return true;
}

bool FPCGExPackClustersElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPackClustersElement::Execute);

	PCGEX_CONTEXT(PackClusters)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		PCGEX_DELETE(Context->VtxAttributes)

		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (!Context->TaggedEdges)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Some input points have no associated edges."));
				Context->SetState(PCGExMT::State_ReadyForNextPoints);
				return false;
			}

			Context->VtxAttributes = PCGEx::FAttributesInfos::Get(Context->CurrentIO->GetIn());

			for (PCGExData::FPointIO* EdgeIO : Context->TaggedEdges->Entries)
			{
				EdgeIO->CreateInKeys();
				PCGExData::FPointIO& PackedIO = Context->PackedClusters->Emplace_GetRef(*EdgeIO, PCGExData::EInit::DuplicateInput); // Pack from edges
				Context->GetAsyncManager()->Start<FPCGExPackClusterTask>(PackedIO.IOIndex, &PackedIO, Context->CurrentIO, EdgeIO);
			}

			Context->SetAsyncState(PCGExMT::State_WaitingOnAsyncWork);
		}
	}

	if (Context->IsState(PCGExMT::State_WaitingOnAsyncWork))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsDone())
	{
		Context->PackedClusters->OutputTo(Context);
	}

	return Context->IsDone();
}

bool FPCGExPackClusterTask::ExecuteTask()
{
	FPCGExPackClustersContext* Context = Manager->GetContext<FPCGExPackClustersContext>();

	TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();

	PCGEx::TFAttributeReader<int32>* StartIndexReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgeStart);
	PCGEx::TFAttributeReader<int32>* EndIndexReader = new PCGEx::TFAttributeReader<int32>(PCGExGraph::Tag_EdgeEnd);
	StartIndexReader->Bind(*InEdges);
	EndIndexReader->Bind(*InEdges);

	const int32 NumEdges = StartIndexReader->Values.Num();

	TArray<int32> SourceVtx;
	SourceVtx.Reserve(NumEdges * 2);

	for (int i = 0; i < NumEdges; i++)
	{
		SourceVtx.AddUnique(*Context->NodeIndicesMap.Find(StartIndexReader->Values[i]));
		SourceVtx.AddUnique(*Context->NodeIndicesMap.Find(EndIndexReader->Values[i]));
	}

	PCGEX_DELETE(StartIndexReader)
	PCGEX_DELETE(EndIndexReader)

	const int32 VtxStartIndex = MutablePoints.Num();
	MutablePoints.SetNum(VtxStartIndex + SourceVtx.Num());

	for (int i = 0; i < SourceVtx.Num(); i++)
	{
		int32 WriteIndex = VtxStartIndex + i;

		FPCGPoint OriginalVtx = InVtx->GetInPoint(SourceVtx[i]);
		FPCGPoint PackedVtx = MutablePoints[WriteIndex] = OriginalVtx;
		PackedVtx.MetadataEntry = PCGInvalidEntryKey;
		PointIO->GetOut()->Metadata->InitializeOnSet(PackedVtx.MetadataEntry);
	}

	for (const PCGEx::FAttributeIdentity& Identity : Context->VtxAttributes->Identities)
	{
		PCGMetadataAttribute::CallbackWithRightType(
			static_cast<uint16>(Identity.UnderlyingType),
			[&](auto DummyValue) -> void
			{
				using T = decltype(DummyValue);
				TArray<T> RawValues;

				const FPCGMetadataAttribute<T>* Source = InVtx->GetIn()->Metadata->GetConstTypedAttribute<T>(Identity.Name);
				PCGEx::TFAttributeWriter<T>* Writer = new PCGEx::TFAttributeWriter<T>(Identity.Name, Source->GetValueFromItemKey(PCGInvalidEntryKey), Source->AllowsInterpolation());
				Writer->BindAndGet(*PointIO);

				for (int i = 0; i < SourceVtx.Num(); i++)
				{
					Writer->Values[VtxStartIndex + i] = Source->GetValueFromItemKey(InVtx->GetInPoint(SourceVtx[i]).MetadataEntry);
				}

				Writer->Write();
				PCGEX_DELETE(Writer)
			});
	}

	PCGExData::WriteMark(*PointIO, PCGExGraph::Tag_PackedClusterEdgeCount, NumEdges);

	FString OutPairId;
	PointIO->Tags->Set(PCGExGraph::TagStr_ClusterPair, InEdges->GetIn()->UID, OutPairId);

	InEdges->Cleanup();

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

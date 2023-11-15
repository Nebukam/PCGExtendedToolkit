// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Relational/PCGExBuildRelations.h"

#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "PCGContext.h"
#include "DrawDebugHelpers.h"
#include "Editor.h"
#include "Relational/PCGExRelationsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExBuildRelations"

int32 UPCGExBuildRelationsSettings::GetPreferredChunkSize() const { return 32; }

PCGEx::EIOInit UPCGExBuildRelationsSettings::GetPointOutputInitMode() const { return PCGEx::EIOInit::DuplicateInput; }

FPCGElementPtr UPCGExBuildRelationsSettings::CreateElement() const
{
	return MakeShared<FPCGExBuildRelationsElement>();
}

FPCGContext* FPCGExBuildRelationsElement::Initialize(
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExBuildRelationsContext* Context = new FPCGExBuildRelationsContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	return Context;
}

void FPCGExBuildRelationsElement::InitializeContext(
	FPCGExPointsProcessorContext* InContext,
	const FPCGDataCollection& InputData,
	TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node) const
{
	FPCGExRelationsProcessorElement::InitializeContext(InContext, InputData, SourceComponent, Node);
	//FPCGExBuildRelationsContext* Context = static_cast<FPCGExBuildRelationsContext*>(InContext);
	// ...
}

bool FPCGExBuildRelationsElement::ExecuteInternal(
	FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildRelationsElement::Execute);

	FPCGExBuildRelationsContext* Context = static_cast<FPCGExBuildRelationsContext*>(InContext);

	const UPCGExBuildRelationsSettings* Settings = Context->GetInputSettings<UPCGExBuildRelationsSettings>();
	check(Settings);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExMT::EState::ReadyForNextPoints);
	}

	// Prep point for param loops

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		if (Context->CurrentIO)
		{
			//Cleanup current IO, indices won't be needed anymore.
			Context->CurrentIO->Flush();
		}

		if (!Context->AdvancePointsIO(true))
		{
			Context->SetState(PCGExMT::EState::Done); //No more points
		}
		else
		{
			Context->CurrentIO->BuildMetadataEntriesAndIndices();
			Context->Octree = const_cast<UPCGPointData::PointOctree*>(&(Context->CurrentIO->Out->GetOctree())); // Not sure this really saves perf
			Context->SetState(PCGExMT::EState::ReadyForNextParams);
		}
	}

	// Process params for current points

	auto ProcessPoint = [&Context](
		const FPCGPoint& Point, int32 ReadIndex, UPCGExPointIO* IO)
	{
		Context->CachedIndex->SetValue(Point.MetadataEntry, ReadIndex); // Cache index

		TArray<PCGExRelational::FSocketSampler> Samplers;
		const double MaxDistance = Context->PrepareSamplersForPoint(Point, Samplers);

		auto ProcessPointNeighbor = [&ReadIndex, &Samplers, &IO](const FPCGPointRef& OtherPointRef)
		{
			const FPCGPoint* OtherPoint = OtherPointRef.Point;
			const int32 Index = IO->GetIndex(OtherPoint->MetadataEntry);

			if (Index == ReadIndex) { return; }

			for (PCGExRelational::FSocketSampler& SocketSampler : Samplers)
			{
				if (SocketSampler.ProcessPoint(OtherPoint))
				{
					SocketSampler.Index = Index;
					SocketSampler.EntryKey = OtherPoint->MetadataEntry;
				}
			}
		};

		const FBoxCenterAndExtent Box = FBoxCenterAndExtent(Point.Transform.GetLocation(), FVector(MaxDistance));
		Context->Octree->FindElementsWithBoundsTest(Box, ProcessPointNeighbor);

		//Write results
		const PCGMetadataEntryKey Key = Point.MetadataEntry;
		for (PCGExRelational::FSocketSampler Sampler : Samplers) { Sampler.OutputTo(Key); }
	};

	if (Context->IsState(PCGExMT::EState::ReadyForNextParams))
	{
		if (!Context->AdvanceParams())
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
			return false;
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingParams);
		}
	}

	auto Initialize = [&Context](const UPCGExPointIO* IO)
	{
		Context->CurrentParams->PrepareForPointData(Context, IO->Out);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingParams))
	{
		if (Context->CurrentIO->OutputParallelProcessing(Context, Initialize, ProcessPoint, Context->ChunkSize))
		{
			if (Settings->bComputeRelationsType)
			{
				Context->SetState(PCGExMT::EState::ProcessingParams2ndPass);
			}
			else
			{
				Context->SetState(PCGExMT::EState::ReadyForNextParams);
			}
		}
	}

	// Process params again for relation types

	auto InitializeForRelations = [&Context](UPCGExPointIO* IO)
	{
	};

	auto ProcessPointForRelations = [&Context](
		const FPCGPoint& Point, int32 ReadIndex, UPCGExPointIO* IO)
	{
		Context->ComputeRelationsType(Point, ReadIndex, IO);
	};

	if (Context->IsState(PCGExMT::EState::ProcessingParams2ndPass))
	{
		if (Context->CurrentIO->OutputParallelProcessing(Context, InitializeForRelations, ProcessPointForRelations, Context->ChunkSize))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextParams);
		}
	}

	if (Context->IsState(PCGExMT::EState::Done))
	{
		Context->OutputPointsAndParams();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE

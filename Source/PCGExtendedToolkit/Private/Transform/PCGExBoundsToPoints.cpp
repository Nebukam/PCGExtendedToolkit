// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExBoundsToPoints.h"

#include "Data/PCGExData.h"

#define LOCTEXT_NAMESPACE "PCGExBoundsToPointsElement"
#define PCGEX_NAMESPACE BoundsToPoints

PCGExData::EInit UPCGExBoundsToPointsSettings::GetMainOutputInitMode() const { return bGeneratePerPointData ? PCGExData::EInit::NoOutput : PCGExData::EInit::DuplicateInput; }

FPCGExBoundsToPointsContext::~FPCGExBoundsToPointsContext()
{
	PCGEX_TERMINATE_ASYNC
}

PCGEX_INITIALIZE_ELEMENT(BoundsToPoints)

bool FPCGExBoundsToPointsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BoundsToPoints)

	return true;
}

bool FPCGExBoundsToPointsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBoundsToPointsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BoundsToPoints)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }


		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBoundsToPoints::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExBoundsToPoints::FProcessor>* NewBatch)
			{
				//NewBatch->bRequiresWriteStep = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any paths to subdivide."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExBoundsToPoints
{
	FProcessor::~FProcessor()
	{
		NewOutputs.Empty();
		PointAttributesToOutputTags.Cleanup();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBoundsToPoints::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BoundsToPoints)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		bSetExtents = Settings->bSetExtents;
		Extents = Settings->Extents;

		bSetScale = Settings->bSetScale;
		Scale = Settings->Scale;

		Axis = Settings->SymmetryAxis;
		UVW = Settings->UVW;
		if (!UVW.Init(Context, PointDataFacade)) { return false; }

		PointAttributesToOutputTags = Settings->PointAttributesToOutputTags;
		if (!PointAttributesToOutputTags.Init(Context, PointDataFacade)) { return false; }

		NumPoints = PointIO->GetNum();
		bGeneratePerPointData = Settings->bGeneratePerPointData;
		bSymmetry = Settings->SymmetryAxis != EPCGExMinimalAxis::None;

		if (bGeneratePerPointData)
		{
			NewOutputs.SetNumUninitialized(PointIO->GetNum());
			for (int i = 0; i < NewOutputs.Num(); i++)
			{
				NewOutputs[i] = TypedContext->MainPoints->Emplace_GetRef(PointIO, PCGExData::EInit::NewOutput);
			}

			if (bSymmetry)
			{
			}
			else
			{
			}
		}
		else
		{
			if (bSymmetry)
			{
				PointIO->GetOut()->GetMutablePoints().SetNumUninitialized(NumPoints * 2);
			}
			else
			{
			}
		}

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		if (bGeneratePerPointData)
		{
			int32 OutIndex;
			PCGExData::FPointIO* NewOutput = NewOutputs[Index];

			FPCGPoint& A = NewOutput->CopyPoint(Point, OutIndex);
			if (bSetExtents)
			{
				A.BoundsMin = -Extents;
				A.BoundsMax = Extents;
			}

			A.Transform.SetLocation(UVW.GetPosition(PointIO->GetInPointRef(Index)));
			if (bSetScale) { A.Transform.SetScale3D(Scale); }

			if (bSymmetry)
			{
				FPCGPoint& B = NewOutput->CopyPoint(Point, OutIndex);
				if (bSetExtents)
				{
					B.BoundsMin = -Extents;
					B.BoundsMax = Extents;
				}

				B.Transform.SetLocation(UVW.GetPosition(PointIO->GetInPointRef(Index), Axis, true));
				if (bSetScale) { B.Transform.SetScale3D(Scale); }
			}

			PointAttributesToOutputTags.Tag(Index, NewOutput);
		}
		else
		{
			TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();

			FPCGPoint& A = MutablePoints[Index];
			if (bSetExtents)
			{
				A.BoundsMin = -Extents;
				A.BoundsMax = Extents;
			}

			A.Transform.SetLocation(UVW.GetPosition(PointIO->GetInPointRef(Index)));
			if (bSetScale) { A.Transform.SetScale3D(Scale); }

			if (bSymmetry)
			{
				MutablePoints[NumPoints + Index] = Point;
				FPCGPoint& B = MutablePoints[NumPoints + Index];
				if (bSetExtents)
				{
					B.BoundsMin = -Extents;
					B.BoundsMax = Extents;
				}

				B.Transform.SetLocation(UVW.GetPosition(PointIO->GetInPointRef(Index), Axis, true));
				if (bSetScale) { B.Transform.SetScale3D(Scale); }
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BoundsToPoints)

		if (!bGeneratePerPointData && bSymmetry)
		{
			TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
			UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;
			for (int i = NumPoints; i < MutablePoints.Num(); i++) { Metadata->InitializeOnSet(MutablePoints[i].MetadataEntry); }
		}
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

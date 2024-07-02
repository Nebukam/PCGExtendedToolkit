// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/PCGExBoundsToPoints.h"

#include "Data/PCGExData.h"

#define LOCTEXT_NAMESPACE "PCGExBoundsToPointsElement"
#define PCGEX_NAMESPACE BoundsToPoints

PCGExData::EInit UPCGExBoundsToPointsSettings::GetMainOutputInitMode() const { return bGeneratePerPointData ? PCGExData::EInit::NoOutput : SymmetryAxis == EPCGExMinimalAxis::None ? PCGExData::EInit::DuplicateInput : PCGExData::EInit::NewOutput; }

FPCGExBoundsToPointsContext::~FPCGExBoundsToPointsContext()
{
	PCGEX_TERMINATE_ASYNC
}

PCGEX_INITIALIZE_ELEMENT(BoundsToPoints)

bool FPCGExBoundsToPointsElement::Boot(FPCGContext* InContext) const
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

	Context->OutputMainPoints();
	Context->Done();

	return Context->TryComplete();
}

namespace PCGExBoundsToPoints
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE_TARRAY(NewOutputs)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BoundsToPoints)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		UVW = Settings->UVW;
		if (!UVW.Init(Context, PointDataFacade)) { return false; }

		NumPoints = PointIO->GetNum();
		bGeneratePerPointData = Settings->bGeneratePerPointData;
		bSymmetry = Settings->SymmetryAxis != EPCGExMinimalAxis::None;

		if (bGeneratePerPointData)
		{
			NewOutputs.SetNumUninitialized(PointIO->GetNum());
			for (int i = 0; i < NewOutputs.Num(); i++)
			{
				PCGExData::FPointIO* NewIO = new PCGExData::FPointIO(PointIO->GetIn());
				NewIO->InitializeOutput(PCGExData::EInit::NewOutput);
				NewOutputs[i] = NewIO;
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
			A.SetLocalCenter(FVector::Zero());
			A.SetExtents(FVector(0.5));

			const FVector Position = UVW.GetPosition(PointIO->GetInPointRef(Index));
			A.Transform.SetLocation(Position);
			
			if (bSymmetry)
			{
				const FVector OriginalPos = Point.Transform.GetLocation();
				const FVector Dir = (OriginalPos - Position).GetSafeNormal();
				const double Dist = FVector::Dist(OriginalPos, Position);

				FPCGPoint& B = NewOutput->CopyPoint(Point, OutIndex);
				B.SetLocalCenter(FVector::Zero());
				B.SetExtents(FVector(0.5));

				B.Transform.SetLocation(OriginalPos + (Dir * -Dist));
			}
		}
		else
		{
			TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
			FPCGPoint& A = MutablePoints[Index];
			A.SetLocalCenter(FVector::Zero());
			A.SetExtents(FVector(0.5));

			const FVector Position = UVW.GetPosition(PointIO->GetInPointRef(Index));
			A.Transform.SetLocation(Position);

			if (bSymmetry)
			{
				const FVector OriginalPos = Point.Transform.GetLocation();
				const FVector Dir = (OriginalPos - Position).GetSafeNormal();
				const double Dist = FVector::Dist(OriginalPos, Position);

				MutablePoints[NumPoints + Index] = Point;
				FPCGPoint& B = MutablePoints[NumPoints + Index];
				B.SetLocalCenter(FVector::Zero());
				B.SetExtents(FVector(0.5));

				B.Transform.SetLocation(OriginalPos + (Dir * -Dist));
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

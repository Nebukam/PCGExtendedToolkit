// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExPointsToBounds.h"

#include "Data/PCGExData.h"

#define LOCTEXT_NAMESPACE "PCGExPointsToBoundsElement"
#define PCGEX_NAMESPACE PointsToBounds

PCGExData::EInit UPCGExPointsToBoundsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGExPointsToBoundsContext::~FPCGExPointsToBoundsContext()
{
	PCGEX_TERMINATE_ASYNC
}

PCGEX_INITIALIZE_ELEMENT(PointsToBounds)

bool FPCGExPointsToBoundsElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PointsToBounds)

	if (Settings->bWritePointsCount) { PCGEX_VALIDATE_NAME(Settings->PointsCountAttributeName) }

	return true;
}

bool FPCGExPointsToBoundsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPointsToBoundsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PointsToBounds)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPointsToBounds::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExPointsToBounds::FProcessor>* NewBatch)
			{
				//NewBatch->bRequiresWriteStep = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any paths to subdivide."));
			return true;
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->OutputMainPoints();

	return Context->TryComplete();
}

namespace PCGExPointsToBounds
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(MetadataBlender)
		PCGEX_DELETE(Bounds)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPointsToBounds::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PointsToBounds)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		Bounds = new FBounds(PointIO);
		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();

		switch (Settings->BoundsSource)
		{
		default: ;
		case EPCGExPointBoundsSource::DensityBounds:
			for (const FPCGPoint& Pt : InPoints)
			{
				const FBox Box = Pt.GetDensityBounds().GetBox();
				Bounds->Bounds += Box;
				Bounds->FastVolume += Box.GetExtent().Length();
			}
			break;
		case EPCGExPointBoundsSource::ScaledBounds:
			for (const FPCGPoint& Pt : InPoints)
			{
				const FBox Box = FBoxCenterAndExtent(Pt.Transform.GetLocation(), Pt.GetScaledExtents()).GetBox();
				Bounds->Bounds += Box;
				Bounds->FastVolume += Box.GetExtent().Length();
			}
			break;
		case EPCGExPointBoundsSource::Bounds:
			for (const FPCGPoint& Pt : InPoints)
			{
				const FBox Box = FBoxCenterAndExtent(Pt.Transform.GetLocation(), Pt.GetExtents()).GetBox();
				Bounds->Bounds += Box;
				Bounds->FastVolume += Box.GetExtent().Length();
			}
			break;
		}

		return true;
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PointsToBounds)

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		UPCGPointData* OutData = PointIO->GetOut();

		TArray<FPCGPoint>& MutablePoints = OutData->GetMutablePoints();
		MutablePoints.Emplace();

		const double NumPoints = InPoints.Num();

		const FBox& Box = Bounds->Bounds;
		const FVector Center = Box.GetCenter();
		const double SqrDist = Box.GetExtent().SquaredLength();

		MutablePoints[0].Transform.SetLocation(Center);
		MutablePoints[0].BoundsMin = Box.Min - Center;
		MutablePoints[0].BoundsMax = Box.Max - Center;

		if (Settings->bBlendProperties)
		{
			MetadataBlender = new PCGExDataBlending::FMetadataBlender(&Settings->BlendingSettings);
			MetadataBlender->PrepareForData(PointDataFacade);

			const PCGExData::FPointRef Target = PointIO->GetOutPointRef(0);
			MetadataBlender->PrepareForBlending(Target);

			double TotalWeight = 0;

			for (int i = 0; i < NumPoints; i++)
			{
				FVector Location = InPoints[i].Transform.GetLocation();
				const double Weight = FVector::DistSquared(Center, Location) / SqrDist;
				MetadataBlender->Blend(Target, PointIO->GetInPointRef(i), Target, Weight);
				TotalWeight += Weight;
			}

			MetadataBlender->CompleteBlending(Target, NumPoints, TotalWeight);

			MutablePoints[0].Transform.SetLocation(Center);
			MutablePoints[0].BoundsMin = Box.Min - Center;
			MutablePoints[0].BoundsMax = Box.Max - Center;
		}

		if (Settings->bWritePointsCount) { PCGExData::WriteMark(OutData->Metadata, Settings->PointsCountAttributeName, NumPoints); }

		PointDataFacade->Write(AsyncManagerPtr, true);
	}

	bool FComputeIOBoundsTask::ExecuteTask()
	{
		const TArray<FPCGPoint>& InPoints = Bounds->PointIO->GetIn()->GetPoints();

		switch (BoundsSource)
		{
		default: ;
		case EPCGExPointBoundsSource::DensityBounds:
			for (const FPCGPoint& Pt : InPoints)
			{
				const FBox Box = Pt.GetDensityBounds().GetBox();
				Bounds->Bounds += Box;
				Bounds->FastVolume += Box.GetExtent().Length();
			}
			break;
		case EPCGExPointBoundsSource::ScaledBounds:
			for (const FPCGPoint& Pt : InPoints)
			{
				const FBox Box = FBoxCenterAndExtent(Pt.Transform.GetLocation(), Pt.GetScaledExtents()).GetBox();
				Bounds->Bounds += Box;
				Bounds->FastVolume += Box.GetExtent().Length();
			}
			break;
		case EPCGExPointBoundsSource::Bounds:
			for (const FPCGPoint& Pt : InPoints)
			{
				const FBox Box = FBoxCenterAndExtent(Pt.Transform.GetLocation(), Pt.GetExtents()).GetBox();
				Bounds->Bounds += Box;
				Bounds->FastVolume += Box.GetExtent().Length();
			}
			break;
		}

		return true;
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

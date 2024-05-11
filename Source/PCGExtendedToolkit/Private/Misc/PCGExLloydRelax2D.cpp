// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExLloydRelax2D.h"

#include "Geometry/PCGExGeoDelaunay.h"

#define LOCTEXT_NAMESPACE "PCGExLloydRelax2DElement"
#define PCGEX_NAMESPACE LloydRelax2D

PCGExData::EInit UPCGExLloydRelax2DSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(LloydRelax2D)

FPCGExLloydRelax2DContext::~FPCGExLloydRelax2DContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(InfluenceGetter)
	ProjectionSettings.Cleanup();

	ActivePositions.Empty();
}

bool FPCGExLloydRelax2DElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(LloydRelax2D)

	Context->InfluenceGetter = new PCGEx::FLocalSingleFieldGetter();
	Context->InfluenceGetter->Capture(Settings->InfluenceSettings.LocalInfluence);

	PCGEX_FWD(ProjectionSettings)

	return true;
}

bool FPCGExLloydRelax2DElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExLloydRelax2DElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(LloydRelax2D)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else
		{
			if (Context->CurrentIO->GetNum() <= 3)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("(0) Some inputs have too few points to be processed (<= 3)."));
				return false;
			}

			Context->CurrentIO->CreateInKeys();
			
			if (Settings->InfluenceSettings.bUseLocalInfluence) { Context->InfluenceGetter->Grab(*Context->CurrentIO); }

			PCGExGeo::PointsToPositions(Context->CurrentIO->GetIn()->GetPoints(), Context->ActivePositions);

			Context->ProjectionSettings.Init(Context->CurrentIO);

			Context->GetAsyncManager()->Start<FPCGExLloydRelax2Task>(
				0, nullptr, &Context->ActivePositions,
				&Settings->InfluenceSettings, Settings->Iterations, Context->InfluenceGetter, &Context->ProjectionSettings);


			Context->SetAsyncState(PCGExMT::State_ProcessingPoints);
		}
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		PCGEX_WAIT_ASYNC

		TArray<FPCGPoint>& MutablePoints = Context->CurrentIO->GetOut()->GetMutablePoints();

		TArray<FVector> TargetPositions;
		TargetPositions.SetNum(MutablePoints.Num());

		for (int i = 0; i < MutablePoints.Num(); i++)
		{
			FVector Pos = MutablePoints[i].Transform.GetLocation();
			Pos.X = Context->ActivePositions[i].X;
			Pos.Y = Context->ActivePositions[i].Y;
			TargetPositions[i] = Pos;
		}

		if (Settings->InfluenceSettings.bProgressiveInfluence)
		{
			for (int i = 0; i < MutablePoints.Num(); i++) { MutablePoints[i].Transform.SetLocation(TargetPositions[i]); }
		}
		else
		{
			for (int i = 0; i < MutablePoints.Num(); i++)
			{
				MutablePoints[i].Transform.SetLocation(
					FMath::Lerp(
						MutablePoints[i].Transform.GetLocation(),
						TargetPositions[i],
						Context->InfluenceGetter->SafeGet(i, Settings->InfluenceSettings.Influence)));
			}
		}

		Context->SetState(PCGExMT::State_ReadyForNextPoints);
		return false;
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
	}

	return Context->IsDone();
}

bool FPCGExLloydRelax2Task::ExecuteTask()
{
	NumIterations--;

	PCGExGeo::TDelaunay2* Delaunay = new PCGExGeo::TDelaunay2();
	TArray<FVector>& Positions = *ActivePositions;

	const TArrayView<FVector> View = MakeArrayView(Positions);
	if (!Delaunay->Process(View, *ProjectionSettings)) { return false; }

	const int32 NumPoints = Positions.Num();

	TArray<FVector> Sum;
	TArray<double> Counts;
	Sum.Append(*ActivePositions);
	Counts.SetNum(NumPoints);
	for (int i = 0; i < NumPoints; i++) { Counts[i] = 1; }

	FVector Centroid;
	for (const PCGExGeo::FDelaunaySite2& Site : Delaunay->Sites)
	{
		PCGExGeo::GetCentroid(Positions, Site.Vtx, Centroid);
		for (const int32 PtIndex : Site.Vtx)
		{
			Counts[PtIndex] += 1;
			Sum[PtIndex] += Centroid;
		}
	}

	if (InfluenceSettings->bProgressiveInfluence && InfluenceGetter)
	{
		for (int i = 0; i < NumPoints; i++) { Positions[i] = FMath::Lerp(Positions[i], Sum[i] / Counts[i], InfluenceGetter->SafeGet(i, InfluenceSettings->Influence)); }
	}
	else
	{
		for (int i = 0; i < NumPoints; i++) { Positions[i] = FMath::Lerp(Positions[i], Sum[i] / Counts[i], InfluenceSettings->Influence); }
	}

	PCGEX_DELETE(Delaunay)

	if (NumIterations > 0)
	{
		Manager->Start<FPCGExLloydRelax2Task>(TaskIndex + 1, PointIO, ActivePositions, InfluenceSettings, NumIterations, InfluenceGetter, ProjectionSettings);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExLloydRelax.h"

#include "Geometry/PCGExVoronoiLloyd.h"

#define LOCTEXT_NAMESPACE "PCGExLloydRelaxElement"
#define PCGEX_NAMESPACE LloydRelax

PCGExData::EInit UPCGExLloydRelaxSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(LloydRelax)

FPCGExLloydRelaxContext::~FPCGExLloydRelaxContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(InfluenceGetter)

	ActivePositions.Empty();
}

bool FPCGExLloydRelaxElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(LloydRelax)

	Context->InfluenceGetter = new PCGEx::FLocalSingleFieldGetter();
	Context->InfluenceGetter->Capture(Settings->InfluenceSettings.LocalInfluence);

	return true;
}

bool FPCGExLloydRelaxElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExLloydRelaxElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(LloydRelax)

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
			if (Context->CurrentIO->GetNum() <= 4)
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("(0) Some inputs have too few points to be processed (<= 3)."));
				return false;
			}

			Context->CurrentIO->CreateInKeys();
			Context->InfluenceGetter->Grab(*Context->CurrentIO);
			PCGExGeo::PointsToPositions(Context->CurrentIO->GetIn()->GetPoints(), Context->ActivePositions);

			Context->GetAsyncManager()->Start<PCGExGeoTask::FLloydRelax3>(
				0, nullptr, &Context->ActivePositions,
				Settings->InfluenceSettings, Settings->Iterations);


			Context->SetAsyncState(PCGExMT::State_ProcessingPoints);
		}
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		if (!Context->IsAsyncWorkComplete()) { return false; }

		TArray<FPCGPoint>& MutablePoints = Context->CurrentIO->GetOut()->GetMutablePoints();

		if (Settings->InfluenceSettings.bProgressiveInfluence)
		{
			for (int i = 0; i < MutablePoints.Num(); i++) { MutablePoints[i].Transform.SetLocation(Context->ActivePositions[i]); }
		}
		else
		{
			for (int i = 0; i < MutablePoints.Num(); i++)
			{
				MutablePoints[i].Transform.SetLocation(
					FMath::Lerp(
						MutablePoints[i].Transform.GetLocation(),
						Context->ActivePositions[i],
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

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

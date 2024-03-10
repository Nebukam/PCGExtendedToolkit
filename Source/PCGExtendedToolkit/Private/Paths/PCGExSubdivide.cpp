// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExSubdivide.h"

#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExSubdivideElement"
#define PCGEX_NAMESPACE Subdivide

#if WITH_EDITOR
void UPCGExSubdivideSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

PCGExData::EInit UPCGExSubdivideSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

PCGEX_INITIALIZE_ELEMENT(Subdivide)

void UPCGExSubdivideSettings::PostInitProperties()
{
	Super::PostInitProperties();
	PCGEX_OPERATION_DEFAULT(Blending, UPCGExSubPointsBlendInterpolate)
}

FPCGExSubdivideContext::~FPCGExSubdivideContext()
{
	Milestones.Empty();
	MilestonesMetrics.Empty();
}

bool FPCGExSubdivideElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Subdivide)

	PCGEX_FWD(SubdivideMethod)
	PCGEX_FWD(Distance)
	PCGEX_FWD(Count)
	PCGEX_FWD(bFlagSubPoints)
	PCGEX_FWD(FlagName)

	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInterpolate)

	Context->Blending->bClosedPath = Settings->bClosedPath;

	if (Context->bFlagSubPoints) { PCGEX_VALIDATE_NAME(Context->FlagName) }

	return true;
}


bool FPCGExSubdivideElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSubdivideElement::Execute);

	PCGEX_CONTEXT(Subdivide)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else { Context->SetState(PCGExMT::State_ProcessingPoints); }
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto Initialize = [&](const PCGExData::FPointIO& PointIO)
		{
			Context->Milestones.Empty();
			Context->Milestones.Add(0);
			Context->MilestonesMetrics.Empty();
			Context->MilestonesMetrics.Add(PCGExMath::FPathMetricsSquared{});
			if (Context->bFlagSubPoints) { Context->FlagAttribute = PointIO.GetOut()->Metadata->FindOrCreateAttribute(Context->FlagName, false, false); }
		};

		auto ProcessPoint = [&](const int32 Index, const PCGExData::FPointIO& PointIO)
		{
			int32 LastIndex;

			const FPCGPoint& StartPoint = PointIO.GetInPoint(Index);
			const FPCGPoint* EndPtr = PointIO.TryGetInPoint(Index + 1);
			PointIO.CopyPoint(StartPoint, LastIndex);

			if (!EndPtr) { return; }

			const FVector StartPos = StartPoint.Transform.GetLocation();
			const FVector EndPos = EndPtr->Transform.GetLocation();
			const FVector Dir = (EndPos - StartPos).GetSafeNormal();
			PCGExMath::FPathMetricsSquared& Metrics = Context->MilestonesMetrics.Last();

			const double Distance = FVector::Distance(StartPos, EndPos);
			const int32 NumSubdivisions = Context->SubdivideMethod == EPCGExSubdivideMode::Count ?
				                              Context->Count :
				                              FMath::Floor(FVector::Distance(StartPos, EndPos) / Context->Distance);

			const double StepSize = Distance / static_cast<double>(NumSubdivisions);
			const double StartOffset = (Distance - StepSize * NumSubdivisions) * 0.5;

			Metrics.Reset(StartPos);

			for (int i = 0; i < NumSubdivisions; i++)
			{
				FPCGPoint& NewPoint = PointIO.CopyPoint(StartPoint, LastIndex);
				FVector SubLocation = StartPos + Dir * (StartOffset + i * StepSize);
				NewPoint.Transform.SetLocation(SubLocation);
				Metrics.Add(SubLocation);

				if (Context->FlagAttribute) { Context->FlagAttribute->SetValue(NewPoint.MetadataEntry, true); }
			}

			Metrics.Add(EndPos);

			Context->Milestones.Add(LastIndex);
			Context->MilestonesMetrics.Emplace_GetRef();
		};

		if (!Context->ProcessCurrentPoints(Initialize, ProcessPoint, true)) { return false; }
		Context->SetState(PCGExSubdivide::State_BlendingPoints);
	}

	if (Context->IsState(PCGExSubdivide::State_BlendingPoints))
	{
		auto Initialize = [&]()
		{
			Context->Blending->PrepareForData(*Context->CurrentIO, *Context->CurrentIO, false);
		};

		auto ProcessMilestone = [&](const int32 Index)
		{
			if (!Context->Milestones.IsValidIndex(Index + 1)) { return; } // Ignore last point

			const PCGExData::FPointIO& PointIO = *Context->CurrentIO;

			const int32 StartIndex = Context->Milestones[Index];
			const int32 Range = Context->Milestones[Index + 1] - StartIndex;
			const int32 EndIndex = StartIndex + Range + 1;

			TArray<FPCGPoint>& MutablePoints = PointIO.GetOut()->GetMutablePoints();
			TArrayView<FPCGPoint> View = MakeArrayView(MutablePoints.GetData() + StartIndex + 1, Range);

			const FPCGPoint& StartPoint = PointIO.GetOutPoint(StartIndex);
			const FPCGPoint* EndPtr = PointIO.TryGetOutPoint(EndIndex);
			if (!EndPtr) { return; }

			Context->Blending->ProcessSubPoints(
				PCGEx::FPointRef(StartPoint, StartIndex), PCGEx::FPointRef(*EndPtr, EndIndex),
				View, Context->MilestonesMetrics[Index]);

			for (FPCGPoint& Pt : View) { PCGExMath::RandomizeSeed(Pt); }
		};

		if (!Context->Process(Initialize, ProcessMilestone, Context->Milestones.Num())) { return false; }

		Context->Blending->Write();
		Context->CurrentIO->OutputTo(Context);
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

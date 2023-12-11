// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Splines/PCGExSubdivide.h"

#include "Splines/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExSubdivideElement"

UPCGExSubdivideSettings::UPCGExSubdivideSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Blending = EnsureOperation<UPCGExSubPointsBlendInterpolate>(Blending);
}

void UPCGExSubdivideSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Blending = EnsureOperation<UPCGExSubPointsBlendInterpolate>(Blending);
	if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

PCGExData::EInit UPCGExSubdivideSettings::GetPointOutputInitMode() const { return PCGExData::EInit::NewOutput; }

FPCGElementPtr UPCGExSubdivideSettings::CreateElement() const { return MakeShared<FPCGExSubdivideElement>(); }

FPCGExSubdivideContext::~FPCGExSubdivideContext()
{
	Milestones.Empty();
	MilestonesMetrics.Empty();
}

FPCGContext* FPCGExSubdivideElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExSubdivideContext* Context = new FPCGExSubdivideContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	const UPCGExSubdivideSettings* Settings = Context->GetInputSettings<UPCGExSubdivideSettings>();
	check(Settings);

	Context->Method = Settings->SubdivideMethod;
	Context->Distance = Settings->Distance;
	Context->Count = Settings->Count;
	Context->bFlagSubPoints = Settings->bFlagSubPoints;
	Context->FlagName = Settings->FlagName;

	PCGEX_BIND_OPERATION(Blending, UPCGExSubPointsBlendInterpolate)

	return Context;
}


bool FPCGExSubdivideElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSubdivideElement::Execute);

	FPCGExSubdivideContext* Context = static_cast<FPCGExSubdivideContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExMT::State_ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::State_ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO()) { Context->Done(); }
		else { Context->SetState(PCGExMT::State_ProcessingPoints); }
	}

	if (Context->IsState(PCGExMT::State_ProcessingPoints))
	{
		auto Initialize = [&](PCGExData::FPointIO& PointIO)
		{
			Context->Milestones.Empty();
			Context->Milestones.Add(0);
			Context->MilestonesMetrics.Empty();
			Context->MilestonesMetrics.Add(PCGExMath::FPathMetrics{});
			if (Context->bFlagSubPoints) { Context->FlagAttribute = PointIO.GetOut()->Metadata->FindOrCreateAttribute(Context->FlagName, false); }
			Context->Blending->PrepareForData(PointIO);
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
			PCGExMath::FPathMetrics& Metrics = Context->MilestonesMetrics.Last();

			const double Distance = FVector::Distance(StartPos, EndPos);
			const int32 NumSubdivisions = Context->Method == EPCGExSubdivideMode::Count ?
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

				if (Context->bFlagSubPoints) { Context->FlagAttribute->SetValue(NewPoint.MetadataEntry, true); }
			}

			Metrics.Add(EndPos);

			Context->Milestones.Add(LastIndex);
			Context->MilestonesMetrics.Emplace_GetRef();
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint, true))
		{
			Context->SetState(PCGExSubdivide::State_BlendingPoints);
		}
	}

	if (Context->IsState(PCGExSubdivide::State_BlendingPoints))
	{
		auto Initialize = [&]()
		{
			Context->Blending->PrepareForData(*Context->CurrentIO);
		};

		auto ProcessMilestone = [&](const int32 Index)
		{
			if (!Context->Milestones.IsValidIndex(Index + 1)) { return; } // Ignore last point

			const PCGExData::FPointIO& PointIO = *Context->CurrentIO;

			const int32 StartIndex = Context->Milestones[Index];
			const int32 Range = Context->Milestones[Index + 1] - StartIndex;
			const int32 EndIndex = StartIndex + Range + 1;

			TArray<FPCGPoint>& MutablePoints = PointIO.GetOut()->GetMutablePoints();
			TArrayView<FPCGPoint> Path = MakeArrayView(MutablePoints.GetData() + StartIndex + 1, Range);

			const FPCGPoint& StartPoint = PointIO.GetOutPoint(StartIndex);
			const FPCGPoint* EndPtr = PointIO.TryGetOutPoint(EndIndex);
			if (!EndPtr) { return; }

			Context->Blending->ProcessSubPoints(
				PCGEx::FPointRef(StartPoint, StartIndex), PCGEx::FPointRef(*EndPtr, EndIndex),
				Path, Context->MilestonesMetrics[Index]);
		};

		if (Context->Process(Initialize, ProcessMilestone, Context->Milestones.Num()))
		{
			Context->CurrentIO->OutputTo(Context);
			Context->CurrentIO->Cleanup();
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	return Context->IsDone();
}

#undef LOCTEXT_NAMESPACE

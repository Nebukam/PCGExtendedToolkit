// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Splines/PCGExSubdivide.h"

#include "Splines/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExSubdivideElement"

UPCGExSubdivideSettings::UPCGExSubdivideSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Blending = EnsureInstruction<UPCGExSubPointsBlendInterpolate>(Blending);
}

void UPCGExSubdivideSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Blending = EnsureInstruction<UPCGExSubPointsBlendInterpolate>(Blending);
	if (Blending) { Blending->UpdateUserFacingInfos(); }
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

PCGExPointIO::EInit UPCGExSubdivideSettings::GetPointOutputInitMode() const { return PCGExPointIO::EInit::NewOutput; }

FPCGElementPtr UPCGExSubdivideSettings::CreateElement() const { return MakeShared<FPCGExSubdivideElement>(); }

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

	Context->Blending = Settings->EnsureInstruction<UPCGExSubPointsBlendInterpolate>(Settings->Blending, Context);

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
		auto Initialize = [&](FPCGExPointIO& PointIO)
		{
			Context->Milestones.Empty();
			Context->Milestones.Add(0);
			Context->MilestonesPathInfos.Empty();
			Context->MilestonesPathInfos.Add(PCGExMath::FPathInfos{});
			if (Context->bFlagSubPoints) { Context->FlagAttribute = PointIO.GetOut()->Metadata->FindOrCreateAttribute(Context->FlagName, false); }
			Context->Blending->PrepareForData(PointIO);
		};

		auto ProcessPoint = [&](const int32 Index, const FPCGExPointIO& PointIO)
		{
			int32 LastIndex;

			const FPCGPoint& StartPoint = PointIO.GetInPoint(Index);
			const FPCGPoint* EndPtr = PointIO.TryGetInPoint(Index + 1);
			PointIO.CopyPoint(StartPoint, LastIndex);

			if (!EndPtr) { return; }

			const FVector StartPos = StartPoint.Transform.GetLocation();
			const FVector EndPos = EndPtr->Transform.GetLocation();
			const FVector Dir = (EndPos - StartPos).GetSafeNormal();
			PCGExMath::FPathInfos PathInfos = PCGExMath::FPathInfos(StartPos);

			const double Distance = FVector::Distance(StartPos, EndPos);
			const int32 NumSubdivisions = Context->Method == EPCGExSubdivideMode::Count ?
				                              Context->Count :
				                              FMath::Floor(FVector::Distance(StartPos, EndPos) / Context->Distance);

			const double StepSize = Distance / static_cast<double>(NumSubdivisions);
			const double StartOffset = (Distance - StepSize * NumSubdivisions) * 0.5;

			for (int i = 0; i < NumSubdivisions; i++)
			{
				FPCGPoint& NewPoint = PointIO.CopyPoint(StartPoint, LastIndex);
				FVector SubLocation = StartPos + Dir * (StartOffset + i * StepSize);
				NewPoint.Transform.SetLocation(SubLocation);
				PathInfos.Add(SubLocation);

				if (Context->bFlagSubPoints) { Context->FlagAttribute->SetValue(NewPoint.MetadataEntry, true); }
			}

			PathInfos.Add(EndPos);

			Context->Milestones.Add(LastIndex);
			Context->MilestonesPathInfos.Add(PathInfos);
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

			if(!Context->Milestones.IsValidIndex(Index + 1)){return;} // Ignore last point
			
			FPCGExPointIO& PointIO = *Context->CurrentIO;

			const int32 StartIndex = Context->Milestones[Index];
			const int32 Range = Context->Milestones[Index + 1] - StartIndex;
			const int32 EndIndex = StartIndex + Range + 1;

			TArray<FPCGPoint>& MutablePoints = PointIO.GetOut()->GetMutablePoints();
			TArrayView<FPCGPoint> Path = MakeArrayView(MutablePoints.GetData() + StartIndex + 1, Range);

			const FPCGPoint& StartPoint = PointIO.GetOutPoint(StartIndex);
			const FPCGPoint* EndPtr = PointIO.TryGetOutPoint(EndIndex);
			if (!EndPtr) { return; }

			Context->Blending->ProcessSubPoints(PCGEx::FPointRef(StartPoint, StartIndex), PCGEx::FPointRef(*EndPtr, EndIndex), Path, Context->MilestonesPathInfos[Index]);
		};

		if (Context->Process(Initialize, ProcessMilestone, Context->Milestones.Num()))
		{
			Context->SetState(PCGExMT::State_ReadyForNextPoints);
		}
	}

	if (Context->IsDone())
	{
		Context->OutputPoints();
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE

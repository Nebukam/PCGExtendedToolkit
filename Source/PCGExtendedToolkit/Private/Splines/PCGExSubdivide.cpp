// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Splines/PCGExSubdivide.h"

#include "Splines/SubPoints/DataBlending/PCGExSubPointsDataBlendLerp.h"

#define LOCTEXT_NAMESPACE "PCGExSubdivideElement"

UPCGExSubdivideSettings::UPCGExSubdivideSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Blending = EnsureInstruction<UPCGExSubPointsDataBlendLerp>(Blending);
}

PCGExIO::EInitMode UPCGExSubdivideSettings::GetPointOutputInitMode() const { return PCGExIO::EInitMode::NewOutput; }

FName UPCGExSubdivideSettings::GetMainPointsInputLabel() const { return PCGExGraph::SourcePathsLabel; }
FName UPCGExSubdivideSettings::GetMainPointsOutputLabel() const { return PCGExGraph::OutputPathsLabel; }

FPCGElementPtr UPCGExSubdivideSettings::CreateElement() const { return MakeShared<FPCGExSubdivideElement>(); }

FPCGContext* FPCGExSubdivideElement::Initialize(const FPCGDataCollection& InputData, TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExSubdivideContext* Context = new FPCGExSubdivideContext();
	InitializeContext(Context, InputData, SourceComponent, Node);
	const UPCGExSubdivideSettings* Settings = Context->GetInputSettings<UPCGExSubdivideSettings>();
	check(Settings);

	Context->Method = Settings->Method;
	Context->Distance = Settings->Distance;
	Context->Count = Settings->Count;
	Context->bFlagSubPoints = Settings->bFlagSubPoints;
	Context->FlagName = Settings->FlagName;

	Context->SubPointsProcessor = Settings->EnsureInstruction<UPCGExSubPointsDataBlendLerp>(Settings->Blending);

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
		auto Initialize = [&](UPCGExPointIO* PointIO)
		{
			Context->InputAttributeMap.PrepareForPoints(PointIO->In);
			if (Context->bFlagSubPoints) { Context->FlagAttribute = PointIO->Out->Metadata->FindOrCreateAttribute(Context->FlagName, false); }
			Context->SubPointsProcessor->PrepareForData(PointIO, &Context->InputAttributeMap);
		};

		auto ProcessPoint = [&](const int32 Index, const UPCGExPointIO* PointIO)
		{
			const FPCGPoint& StartPoint = PointIO->GetInPoint(Index);
			const FPCGPoint* EndPtr = PointIO->TryGetInPoint(Index + 1);
			FPCGPoint& StartCopy = PointIO->NewPoint(StartPoint);
			if (!EndPtr) { return; }

			const FVector StartPos = StartPoint.Transform.GetLocation();
			const FVector EndPos = EndPtr->Transform.GetLocation();
			const FVector Dir = (EndPos - StartPos).GetSafeNormal();

			const double Distance = FVector::Distance(StartPos, EndPos);
			int32 NumSubdivisions = Context->Count;
			if (Context->Method == EPCGExSubdivideMode::Distance) { NumSubdivisions = FMath::Floor(FVector::Distance(StartPos, EndPos) / Context->Distance); }

			const double StepSize = Distance / static_cast<double>(NumSubdivisions);
			const double StartOffset = (Distance - StepSize * NumSubdivisions) * 0.5;

			TArray<FPCGPoint>& Points = PointIO->Out->GetMutablePoints();
			const int32 StartIndex = Points.Num();

			for (int i = 0; i < NumSubdivisions; i++)
			{
				FPCGPoint& NewPoint = PointIO->NewPoint(StartPoint);
				if (Context->bFlagSubPoints) { Context->FlagAttribute->SetValue(NewPoint.MetadataEntry, true); }

				NewPoint.Transform.SetLocation(StartPos + Dir * (StartOffset + i * StepSize));
			}

			TArrayView<FPCGPoint> Path = MakeArrayView(Points.GetData() + StartIndex, NumSubdivisions);
			Context->SubPointsProcessor->ProcessSubPoints(StartPoint, *EndPtr, Path, Distance);
		};

		if (Context->ProcessCurrentPoints(Initialize, ProcessPoint, true))
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

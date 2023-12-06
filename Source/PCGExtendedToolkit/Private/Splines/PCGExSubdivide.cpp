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
		auto Initialize = [&](UPCGExPointIO* PointIO)
		{
			if (Context->bFlagSubPoints) { Context->FlagAttribute = PointIO->Out->Metadata->FindOrCreateAttribute(Context->FlagName, false); }
			Context->Blending->PrepareForData(PointIO);
		};

		auto ProcessPoint = [&](const int32 Index, const UPCGExPointIO* PointIO)
		{
			const FPCGPoint& StartPoint = PointIO->GetInPoint(Index);
			const FPCGPoint* EndPtr = PointIO->TryGetInPoint(Index + 1);
			FPCGPoint& StartCopy = PointIO->CopyPoint(StartPoint);
			if (!EndPtr) { return; }

			const FVector StartPos = StartPoint.Transform.GetLocation();
			const FVector EndPos = EndPtr->Transform.GetLocation();
			const FVector Dir = (EndPos - StartPos).GetSafeNormal();

			const double Distance = FVector::Distance(StartPos, EndPos);
			int32 NumSubdivisions = Context->Count;
			if (Context->Method == EPCGExSubdivideMode::Distance) { NumSubdivisions = FMath::Floor(FVector::Distance(StartPos, EndPos) / Context->Distance); }

			const double StepSize = Distance / static_cast<double>(NumSubdivisions);
			const double StartOffset = (Distance - StepSize * NumSubdivisions) * 0.5;

			TArray<FPCGPoint>& MutablePoints = PointIO->Out->GetMutablePoints();
			const int32 ViewOffset = MutablePoints.Num();

			PCGExMath::FPathInfos PathInfos = PCGExMath::FPathInfos(StartPos);

			for (int i = 0; i < NumSubdivisions; i++)
			{
				FPCGPoint& NewPoint = PointIO->CopyPoint(StartPoint);
				if (Context->bFlagSubPoints) { Context->FlagAttribute->SetValue(NewPoint.MetadataEntry, true); }

				FVector SubLocation = StartPos + Dir * (StartOffset + i * StepSize);
				PathInfos.Add(SubLocation);

				NewPoint.Transform.SetLocation(SubLocation);
			}

			TArrayView<FPCGPoint> Path = MakeArrayView(MutablePoints.GetData() + ViewOffset, NumSubdivisions);
			Context->Blending->ProcessSubPoints(StartPoint, *EndPtr, Path, PathInfos);
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

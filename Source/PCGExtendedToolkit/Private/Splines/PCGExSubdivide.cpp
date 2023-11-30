// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#include "Splines/PCGExSubdivide.h"

#define LOCTEXT_NAMESPACE "PCGExSubdivideElement"

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
	Context->SubdivideBlend = Settings->SubdivideBlend;
	Context->FlagName = Settings->FlagName;

	return Context;
}


bool FPCGExSubdivideElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSubdivideElement::Execute);

	FPCGExSubdivideContext* Context = static_cast<FPCGExSubdivideContext*>(InContext);

	if (Context->IsSetup())
	{
		if (!Validate(Context)) { return true; }
		Context->SetState(PCGExMT::EState::ReadyForNextPoints);
	}

	if (Context->IsState(PCGExMT::EState::ReadyForNextPoints))
	{
		if (!Context->AdvancePointsIO())
		{
			Context->SetState(PCGExMT::EState::Done);
		}
		else
		{
			Context->SetState(PCGExMT::EState::ProcessingPoints);
		}
	}

	auto Initialize = [&](UPCGExPointIO* PointIO)
	{
		if (Context->bFlagSubPoints) { Context->FlagAttribute = PointIO->Out->Metadata->FindOrCreateAttribute(Context->FlagName, false); }
		if (Context->SubdivideBlend == EPCGExSubdivideBlendMode::Lerp) { Context->InputAttributeMap.PrepareForPoints(PointIO->In); }
	};

	auto ProcessPoint = [&](const int32 Index, const UPCGExPointIO* PointIO)
	{
		const FPCGPoint& StartPoint = PointIO->GetInPoint(Index);
		const FPCGPoint* EndPtr = PointIO->TryGetInPoint(Index + 1);
		FPCGPoint& StartCopy = PointIO->NewPoint(StartPoint);
		if (!EndPtr) { return; }

		const FVector StartPos = StartPoint.Transform.GetLocation();
		const FVector EndPos = EndPtr->Transform.GetLocation();

		const double Distance = FVector::Distance(StartPos, EndPos);
		int32 NumSubdivisions = Context->Count;
		if (Context->Method == EPCGExSubdivideMode::Distance) { NumSubdivisions = FMath::Floor(FVector::Distance(StartPos, EndPos) / Context->Distance); }

		const double StepSize = Distance / static_cast<double>(NumSubdivisions);
		const double StartOffset = (Distance - StepSize * NumSubdivisions) * 0.5;

		if (Context->SubdivideBlend == EPCGExSubdivideBlendMode::Lerp)
		{
			for (int i = 0; i < NumSubdivisions; i++)
			{
				const double Lerp = (StartOffset + StepSize * i) / Distance;

				FPCGPoint& NewPoint = PointIO->NewPoint(StartPoint);
				if (Context->bFlagSubPoints) { Context->FlagAttribute->SetValue(NewPoint.MetadataEntry, true); }

				PCGExMath::Lerp(StartPoint, *EndPtr, NewPoint, Lerp);

				Context->InputAttributeMap.SetLerp(
					StartPoint.MetadataEntry,
					EndPtr->MetadataEntry,
					NewPoint.MetadataEntry,
					Lerp);
			}
		}
		else
		{
			const FPCGPoint& RefPoint = Context->SubdivideBlend == EPCGExSubdivideBlendMode::InheritStart ? StartPoint : *EndPtr;
			for (int i = 0; i < NumSubdivisions; i++)
			{
				double Lerp = (StartOffset + StepSize * i) / Distance;
				FPCGPoint& NewPoint = PointIO->NewPoint(RefPoint);
				NewPoint.Transform.SetLocation(FMath::Lerp(StartPos, EndPos, Lerp));
				if (Context->bFlagSubPoints) { Context->FlagAttribute->SetValue(NewPoint.MetadataEntry, true); }
			}
		}
	};

	if (Context->IsState(PCGExMT::EState::ProcessingPoints))
	{
		if (Context->ChunkProcessingCurrentPoints(Initialize, ProcessPoint))
		{
			Context->SetState(PCGExMT::EState::ReadyForNextPoints);
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

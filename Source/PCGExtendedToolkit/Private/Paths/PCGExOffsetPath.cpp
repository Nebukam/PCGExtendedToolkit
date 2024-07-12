// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExOffsetPath.h"

#define LOCTEXT_NAMESPACE "PCGExOffsetPathElement"
#define PCGEX_NAMESPACE OffsetPath

PCGExData::EInit UPCGExOffsetPathSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

PCGEX_INITIALIZE_ELEMENT(OffsetPath)

FPCGExOffsetPathContext::~FPCGExOffsetPathContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExOffsetPathElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(OffsetPath)

	return true;
}

bool FPCGExOffsetPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExOffsetPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(OffsetPath)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExOffsetPath::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExOffsetPath::FProcessor>* NewBatch)
			{
				//NewBatch->SetPointsFilterData(&Context->FilterFactories);
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any paths to shrink."));
			return true;
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be affected."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
	}

	return Context->TryComplete();
}

namespace PCGExOffsetPath
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(OffsetGetter)
		PCGEX_DELETE(UpGetter)

		Positions.Empty();
		Normals.Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExOffsetPath::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(OffsetPath)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		UpVector = Settings->UpVectorConstant;
		OffsetConstant = Settings->OffsetConstant;

		OffsetGetter = new PCGEx::FLocalSingleFieldGetter();

		if (Settings->OffsetType == EPCGExFetchType::Attribute)
		{
			OffsetGetter->Capture(Settings->OffsetAttribute);
			if (!OffsetGetter->Grab(PointIO))
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Input missing offset size attribute: {0}."), FText::FromName(Settings->OffsetAttribute.GetName())));
				return false;
			}
		}

		UpGetter = new PCGEx::FLocalVectorGetter();

		if (Settings->UpVectorType == EPCGExFetchType::Attribute)
		{
			UpGetter->Capture(Settings->UpVectorAttribute);
			if (!UpGetter->Grab(PointIO))
			{
				PCGEX_DELETE(UpGetter)
				PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("Input missing UpVector attribute: {0}."), FText::FromName(Settings->UpVectorAttribute.GetName())));
				return false;
			}
		}

		const TArray<FPCGPoint>& Points = PointIO->GetIn()->GetPoints();
		NumPoints = Points.Num();

		Positions.SetNumUninitialized(NumPoints);
		Normals.SetNumUninitialized(NumPoints);

		for (int i = 0; i < NumPoints; i++) { Positions[i] = Points[i].Transform.GetLocation(); }

		StartParallelLoopForRange(NumPoints - 2); // Compute all normals

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		Point.Transform.SetLocation(Positions[Index] + (Normals[Index].GetSafeNormal() * OffsetGetter->SafeGet(Index, OffsetConstant)));
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		Normals[Iteration + 1] = NRM(Iteration, Iteration + 1, Iteration + 2); // Offset by 1 because loop should be -1 / 0 / +1
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(OffsetPath)
		const int32 LastIndex = NumPoints - 1;

		// Update first & last Normals
		if (Settings->bClosedPath)
		{
			Normals[0] = NRM(LastIndex, 0, 1);
			Normals[LastIndex] = NRM(NumPoints - 2, LastIndex, 0);
		}
		else
		{
			Normals[0] = NRM(0, 0, 1);
			Normals[LastIndex] = NRM(NumPoints - 2, LastIndex, LastIndex);
		}

		StartParallelLoopForPoints();
	}

	FVector FProcessor::NRM(const int32 A, const int32 B, const int32 C) const
	{
		const FVector VA = Positions[A];
		const FVector VB = Positions[B];
		const FVector VC = Positions[C];
		const FVector UpAverage = ((UpGetter->SafeGet(A, UpVector) + UpGetter->SafeGet(B, UpVector) + UpGetter->SafeGet(C, UpVector)) / 3).GetSafeNormal();
		return FMath::Lerp(PCGExMath::GetNormal(VA, VB, VB + UpAverage), PCGExMath::GetNormal(VB, VC, VC + UpAverage), 0.5).GetSafeNormal();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

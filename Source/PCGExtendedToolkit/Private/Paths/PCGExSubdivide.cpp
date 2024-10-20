// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExSubdivide.h"

#include "PCGExRandom.h"


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExSubdivideElement"
#define PCGEX_NAMESPACE Subdivide

PCGExData::EInit UPCGExSubdivideSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

PCGEX_INITIALIZE_ELEMENT(Subdivide)

bool FPCGExSubdivideElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Subdivide)

	if (Settings->bFlagSubPoints) { PCGEX_VALIDATE_NAME(Settings->SubPointFlagName) }
	if (Settings->bWriteAlpha) { PCGEX_VALIDATE_NAME(Settings->AlphaAttributeName) }

	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendOperation)

	return true;
}


bool FPCGExSubdivideElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSubdivideElement::Execute);

	PCGEX_CONTEXT(Subdivide)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSubdivide::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bInvalidInputs = true;
					Entry->InitializeOutput(Context, PCGExData::EInit::Forward);
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSubdivide::FProcessor>>& NewBatch)
			{
				NewBatch->PrimaryOperation = Context->Blending;
				NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to subdivide."));
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSubdivide
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSubdivide::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }


		bClosedLoop = Context->ClosedLoop.IsClosedLoop(PointDataFacade->Source);

		if (Settings->AmountInput == EPCGExInputValueType::Attribute)
		{
			AmountGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->SubdivisionAmount);
			if (!AmountGetter)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Subdivision Amount attribute is invalid."));
				return false;
			}
		}
		else
		{
			ConstantAmount = Settings->SubdivideMethod == EPCGExSubdivideMode::Count ? Settings->Count : Settings->Distance;
		}

		bUseCount = Settings->SubdivideMethod == EPCGExSubdivideMode::Count;

		Blending = Cast<UPCGExSubPointsBlendOperation>(PrimaryOperation);
		Blending->bClosedLoop = bClosedLoop;

		PCGEx::InitArray(Subdivisions, PointDataFacade->GetNum());

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
		FilterScope(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		FSubdivision& Sub = Subdivisions[Index];

		Sub.NumSubdivisions = 0;
		Sub.Start = PointIO->GetInPoint(Index).Transform.GetLocation();
		Sub.End = PointIO->GetInPoint(Index + 1 == PointIO->GetNum() ? 0 : Index + 1).Transform.GetLocation();
		Sub.Dist = FVector::Distance(Sub.Start, Sub.End);

		if (!PointFilterCache[Index]) { return; }

		const double Amount = AmountGetter ? AmountGetter->Read(Index) : ConstantAmount;

		if (bUseCount)
		{
			Sub.NumSubdivisions = FMath::Floor(Amount);
			Sub.StepSize = Sub.Dist / static_cast<double>(Sub.NumSubdivisions + 1);
			Sub.StartOffset = Sub.StepSize;
		}
		else
		{
			Sub.NumSubdivisions = FMath::Floor(Sub.Dist / Amount);
			Sub.StepSize = Amount;
			Sub.StartOffset = (Sub.Dist - (Sub.StepSize * Sub.NumSubdivisions)) * 0.5;
		}

		Sub.Dir = (Sub.End - Sub.Start).GetSafeNormal();
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		const FSubdivision& Sub = Subdivisions[Iteration];

		if (FlagWriter) { FlagWriter->GetMutable(Sub.OutStart) = false; }
		if (AlphaWriter) { AlphaWriter->GetMutable(Sub.OutStart) = Settings->DefaultAlpha; }

		if (Sub.NumSubdivisions == 0) { return; }

		TArray<FPCGPoint>& MutablePoints = PointDataFacade->GetOut()->GetMutablePoints();

		PCGExPaths::FPathMetrics Metrics = PCGExPaths::FPathMetrics(Sub.Start);

		const int32 SubStart = Sub.OutStart + 1;
		for (int s = 0; s < Sub.NumSubdivisions; s++)
		{
			const int32 Index = SubStart + s;

			if (FlagWriter) { FlagWriter->GetMutable(Index) = true; }

			const FVector Position = Sub.Start + Sub.Dir * (Sub.StartOffset + s * Sub.StepSize);
			MutablePoints[Index].Transform.SetLocation(Position);
			const double Alpha = Metrics.Add(Position) / Sub.Dist;
			if (AlphaWriter) { AlphaWriter->GetMutable(SubStart + s) = Alpha; }
		}

		Metrics.Add(Sub.End);

		const TArrayView<FPCGPoint> View = MakeArrayView(MutablePoints.GetData() + SubStart, Sub.NumSubdivisions);
		Blending->ProcessSubPoints(PointDataFacade->Source->GetOutPointRef(Sub.OutStart), PointDataFacade->Source->GetOutPointRef(Sub.OutEnd), View, Metrics, SubStart);

		for (FPCGPoint& Pt : View) { Pt.Seed = PCGExRandom::ComputeSeed(Pt); }
	}

	void FProcessor::CompleteWork()
	{
		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		int32 NumPoints = 0;

		if (!bClosedLoop) { Subdivisions[Subdivisions.Num() - 1].NumSubdivisions = 0; }

		for (FSubdivision& Sub : Subdivisions)
		{
			Sub.OutStart = NumPoints++;
			NumPoints += Sub.NumSubdivisions;
			Sub.OutEnd = NumPoints;
		}

		if (bClosedLoop) { Subdivisions[Subdivisions.Num() - 1].OutEnd = 0; }
		else { Subdivisions[Subdivisions.Num() - 1].NumSubdivisions = 0; }

		if (NumPoints == PointIO->GetNum())
		{
			PointIO->InitializeOutput(Context, PCGExData::EInit::DuplicateInput);
			if (Settings->bFlagSubPoints) { WriteMark(PointIO, Settings->SubPointFlagName, false); }
			if (Settings->bWriteAlpha) { WriteMark(PointIO, Settings->AlphaAttributeName, Settings->DefaultAlpha); }
			return;
		}

		PointIO->InitializeOutput(Context, PCGExData::EInit::NewOutput);
		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;

		PCGEx::InitArray(MutablePoints, NumPoints);

		for (int i = 0; i < Subdivisions.Num(); i++)
		{
			const FSubdivision& Sub = Subdivisions[i];
			const FPCGPoint& OriginalPoint = InPoints[i];
			MutablePoints[Sub.OutStart] = OriginalPoint;
			Metadata->InitializeOnSet(MutablePoints[Sub.OutStart].MetadataEntry);

			if (Sub.NumSubdivisions == 0) { continue; }

			const int32 SubStart = Sub.OutStart + 1;

			for (int s = 0; s < Sub.NumSubdivisions; s++)
			{
				MutablePoints[SubStart + s] = OriginalPoint;
				Metadata->InitializeOnSet(MutablePoints[SubStart + s].MetadataEntry);
			}
		}

		if (Settings->bFlagSubPoints)
		{
			FlagWriter = PointDataFacade->GetWritable<bool>(Settings->SubPointFlagName, false, false, true);
			ProtectedAttributes.Add(Settings->SubPointFlagName);
		}

		if (Settings->bWriteAlpha)
		{
			AlphaWriter = PointDataFacade->GetWritable<double>(Settings->AlphaAttributeName, Settings->DefaultAlpha, true, true);
			ProtectedAttributes.Add(Settings->AlphaAttributeName);
		}

		Blending->PrepareForData(PointDataFacade, PointDataFacade, PCGExData::ESource::Out, &ProtectedAttributes);
		StartParallelLoopForRange(Subdivisions.Num());
	}

	void FProcessor::Write()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

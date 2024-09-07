// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExSubdivide.h"

#include "PCGExRandom.h"
#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExSubdivideElement"
#define PCGEX_NAMESPACE Subdivide

PCGExData::EInit UPCGExSubdivideSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NewOutput; }

PCGEX_INITIALIZE_ELEMENT(Subdivide)

FPCGExSubdivideContext::~FPCGExSubdivideContext()
{
	PCGEX_TERMINATE_ASYNC
}

bool FPCGExSubdivideElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Subdivide)

	if (Settings->bFlagSubPoints) { PCGEX_VALIDATE_NAME(Settings->FlagName) }

	PCGEX_OPERATION_BIND(Blending, UPCGExSubPointsBlendInterpolate)
	Context->Blending->bClosedPath = Settings->bClosedPath;

	return true;
}


bool FPCGExSubdivideElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSubdivideElement::Execute);

	PCGEX_CONTEXT(Subdivide)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSubdivide::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bInvalidInputs = true;
					Entry->InitializeOutput(PCGExData::EInit::Forward);
					return false;
				}
				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExSubdivide::FProcessor>* NewBatch)
			{
				NewBatch->PrimaryOperation = Context->Blending;
				NewBatch->bRequiresWriteStep = true;
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any paths to subdivide."));
			return true;
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExSubdivide
{
	FProcessor::~FProcessor()
	{
		Subdivisions.Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSubdivide::Process);

		PCGEX_TYPED_CONTEXT_AND_SETTINGS(Subdivide)

		// Must be set before process for filters
		PointDataFacade->bSupportsDynamic = true;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalSettings = Settings;
		LocalTypedContext = TypedContext;

		if (Settings->ValueSource == EPCGExFetchType::Attribute)
		{
			AmountGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->SubdivisionAmount);
			if (!AmountGetter)
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Subdivision Amount attribute is invalid."));
				return false;
			}
		}
		else
		{
			ConstantAmount = Settings->SubdivideMethod == EPCGExSubdivideMode::Count ? Settings->Count : Settings->Distance;
		}

		bUseCount = Settings->SubdivideMethod == EPCGExSubdivideMode::Count;

		Blending = Cast<UPCGExSubPointsBlendOperation>(PrimaryOperation);

		PCGEX_SET_NUM(Subdivisions, PointIO->GetNum())

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(Subdivide)

		FSubdivision& Sub = Subdivisions[Index];

		Sub.NumSubdivisions = 0;
		Sub.Start = PointIO->GetInPoint(Index).Transform.GetLocation();
		Sub.End = PointIO->GetInPoint(Index + 1 == PointIO->GetNum() ? 0 : Index + 1).Transform.GetLocation();

		Sub.Dist = FVector::Distance(Sub.Start, Sub.End);

		double Amount = AmountGetter ? AmountGetter->Values[Index] : ConstantAmount;
		if (!bUseCount) { Amount = Sub.Dist / Amount; }

		Sub.Dir = (Sub.End - Sub.Start).GetSafeNormal();

		Sub.NumSubdivisions = FMath::Floor(Amount);
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		const FSubdivision& Sub = Subdivisions[Iteration];

		if (Sub.NumSubdivisions == 0) { return; }

		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();

		PCGExMath::FPathMetricsSquared Metrics = PCGExMath::FPathMetricsSquared(Sub.Start);

		const double StepSize = Sub.Dist / static_cast<double>(Sub.NumSubdivisions);
		const double StartOffset = (Sub.Dist - StepSize * Sub.NumSubdivisions) * 0.5;

		for (int i = 1; i <= Sub.NumSubdivisions; i++)
		{
			int32 Index = Sub.OutStart + i;
			if (FlagWriter) { FlagWriter->Values[Index + 1] = true; }

			const FVector Position = Sub.Start + Sub.Dir * (StartOffset + i * StepSize);
			MutablePoints[Index].Transform.SetLocation(Position);
			Metrics.Add(Position);
		}

		Metrics.Add(Sub.End);

		const int32 FirstSubdivIndex = Sub.OutStart + 1;
		TArrayView<FPCGPoint> View = MakeArrayView(MutablePoints.GetData() + FirstSubdivIndex, Sub.NumSubdivisions);
		Blending->ProcessSubPoints(PointIO->GetOutPointRef(Sub.OutStart), PointIO->GetOutPointRef(Sub.OutEnd), View, Metrics, FirstSubdivIndex);

		for (FPCGPoint& Pt : View) { Pt.Seed = PCGExRandom::ComputeSeed(Pt); }
	}

	void FProcessor::CompleteWork()
	{
		int32 NumPoints = 0;
		if (!LocalSettings->bClosedPath) { Subdivisions[Subdivisions.Num() - 1].NumSubdivisions = 0; }

		for (FSubdivision& Sub : Subdivisions)
		{
			Sub.OutStart = NumPoints++;
			NumPoints += Sub.NumSubdivisions;
			Sub.OutEnd = NumPoints;
		}

		if (LocalSettings->bClosedPath) { Subdivisions[Subdivisions.Num() - 1].OutEnd = 0; }
		else { Subdivisions[Subdivisions.Num() - 1].NumSubdivisions = 0; }

		if (NumPoints == PointIO->GetNum())
		{
			PointIO->InitializeOutput(PCGExData::EInit::DuplicateInput);
			if (LocalSettings->bFlagSubPoints) { PointIO->GetOut()->Metadata->FindOrCreateAttribute(LocalSettings->FlagName, false); }
			return;
		}

		PointIO->InitializeOutput(PCGExData::EInit::NewOutput);
		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;

		PCGEX_SET_NUM(MutablePoints, NumPoints)
		for (int i = 0; i < Subdivisions.Num(); i++)
		{
			const FSubdivision& Sub = Subdivisions[i];
			const FPCGPoint& OriginalPoint = InPoints[i];
			MutablePoints[Sub.OutStart] = OriginalPoint;
			Metadata->InitializeOnSet(MutablePoints[Sub.OutStart].MetadataEntry);

			if (Sub.NumSubdivisions == 0) { continue; }

			for (int s = 1; s <= Sub.NumSubdivisions; s++)
			{
				MutablePoints[Sub.OutStart + s] = OriginalPoint;
				Metadata->InitializeOnSet(MutablePoints[Sub.OutStart + s].MetadataEntry);
			}
		}

		Blending->PrepareForData(PointDataFacade, PointDataFacade, PCGExData::ESource::Out);
		StartParallelLoopForRange(Subdivisions.Num());
	}

	void FProcessor::Write()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

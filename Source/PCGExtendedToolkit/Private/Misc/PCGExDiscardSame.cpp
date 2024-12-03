// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDiscardSame.h"

#include "Misc/PCGExDiscardByPointCount.h"


#define LOCTEXT_NAMESPACE "PCGExDiscardSameElement"
#define PCGEX_NAMESPACE DiscardSame

PCGExData::EIOInit UPCGExDiscardSameSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Forward; }

TArray<FPCGPinProperties> UPCGExDiscardSameSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExDiscardByPointCount::OutputDiscardedLabel, "Discarded outputs.", Normal, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(DiscardSame)

bool FPCGExDiscardSameElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(DiscardSame)

	return true;
}

bool FPCGExDiscardSameElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDiscardSameElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(DiscardSame)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExDiscardSame::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExDiscardSame::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = true; // Not really but we need the step
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any input to check."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	for (const TSharedPtr<PCGExData::FPointIO>& PointIO : Context->MainPoints->Pairs)
	{
		if (!PointIO->IsEnabled())
		{
			PointIO->OutputPin = PCGExDiscardByPointCount::OutputDiscardedLabel;
			PointIO->Enable();
		}

		PointIO->StageOutput();
	};

	return Context->TryComplete();
}

namespace PCGExDiscardSame
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		// 1 - Build comparison points

		TSet<uint32> PositionHashes;
		const FVector PosCWTolerance = FVector(1 / Settings->TestPositionTolerance);


		const TArray<FPCGPoint>& InPoints = PointDataFacade->GetIn()->GetPoints();
		HashPointsCount = InPoints.Num();

		if (Settings->bTestPositions) { PositionHashes.Reserve(HashPointsCount); }

		FBox Bounds = FBox(ForceInit);

		for (const FPCGPoint& Point : InPoints)
		{
			const FVector& Pos = Point.Transform.GetLocation();
			Bounds += Pos;
			if (Settings->bTestPositions) { PositionHashes.Add(PCGEx::GH3(Pos, PosCWTolerance)); }
		}

		if (Settings->bTestPositions)
		{
			TArray<uint32> PosHashes = PositionHashes.Array();
			PositionHashes.Empty();
			PosHashes.Sort();
			for (int32 i = 0; i < PosHashes.Num(); i++) { HashPositions = HashCombineFast(HashPositions, PosHashes[i]); }
		}
		else
		{
			HashPositions = PointDataFacade->Source->IOIndex;
		}

		if (Settings->bTestBounds)
		{
			const FVector BoundsCWTolerance = FVector(1 / Settings->TestBoundsTolerance);
			HashBounds = HashCombineFast(
				PCGEx::GH3(Bounds.Min, BoundsCWTolerance),
				PCGEx::GH3(Bounds.Max, BoundsCWTolerance));
		}
		else
		{
			HashBounds = PointDataFacade->Source->IOIndex;
		}

		return true;
	}

	void FProcessor::CompleteWork()
	{
		TArray<TSharedRef<FProcessor>> SameAs;
		SameAs.Reserve(Context->MainPoints->Num());

		const TSharedPtr<PCGExPointsMT::TBatch<PCGExDiscardSame::FProcessor>> Batch = StaticCastSharedPtr<PCGExPointsMT::TBatch<PCGExDiscardSame::FProcessor>>(ParentBatch.Pin());

		TSharedRef<FProcessor> ThisRef = SharedThis(this);

		const double Tol = static_cast<double>(Settings->TestPointCountTolerance);

		if (Settings->TestMode == EPCGExFilterGroupMode::AND)
		{
			for (TSharedRef<FProcessor> P : Batch->Processors)
			{
				if (P == ThisRef) { continue; }

				if (Settings->bTestBounds && P->HashBounds != HashBounds) { continue; }
				if (Settings->bTestPositions && P->HashPositions != HashPositions) { continue; }
				if (Settings->bTestPointCount && !FMath::IsNearlyEqual(P->HashPointsCount, HashPointsCount, Tol)) { continue; }

				SameAs.Add(P);
			}
		}
		else
		{
			for (TSharedRef<FProcessor> P : Batch->Processors)
			{
				if (P == ThisRef) { continue; }

				if ((Settings->bTestBounds && P->HashBounds == HashBounds) ||
					(Settings->bTestPositions && P->HashPositions == HashPositions) ||
					(Settings->bTestPointCount && FMath::IsNearlyEqual(P->HashPointsCount, HashPointsCount, Tol)))
				{
					SameAs.Add(P);
				}
			}
		}

		if (SameAs.IsEmpty()) { return; }

		if (Settings->Mode == EPCGExDiscardSameMode::FIFO)
		{
			for (const TSharedRef<FProcessor>& P : SameAs)
			{
				if (P->PointDataFacade->Source->IOIndex > PointDataFacade->Source->IOIndex)
				{
					PointDataFacade->Source->Disable();
					return;
				}
			}
		}
		else if (Settings->Mode == EPCGExDiscardSameMode::LIFO)
		{
			for (const TSharedRef<FProcessor>& P : SameAs)
			{
				if (P->PointDataFacade->Source->IOIndex < PointDataFacade->Source->IOIndex)
				{
					PointDataFacade->Source->Disable();
					return;
				}
			}
		}
		else
		{
			PointDataFacade->Source->Disable();
		}
	}
}
#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

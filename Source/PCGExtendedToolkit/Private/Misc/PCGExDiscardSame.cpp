// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExDiscardSame.h"


#include "PCGExMT.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Misc/PCGExDiscardByPointCount.h"


#define LOCTEXT_NAMESPACE "PCGExDiscardSameElement"
#define PCGEX_NAMESPACE DiscardSame

TArray<FPCGPinProperties> UPCGExDiscardSameSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExDiscardByPointCount::OutputDiscardedLabel, "Discarded outputs.", Normal)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(DiscardSame)
PCGEX_ELEMENT_BATCH_POINT_IMPL(DiscardSame)

bool FPCGExDiscardSameElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(DiscardSame)

	return true;
}

bool FPCGExDiscardSameElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExDiscardSameElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(DiscardSame)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints([&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; }, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
		{
		}))
		{
			return Context->CancelExecution(TEXT("Could not find any input to check."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	for (const TSharedPtr<PCGExData::FPointIO>& PointIO : Context->MainPoints->Pairs)
	{
		if (!PointIO->IsEnabled())
		{
			PointIO->OutputPin = PCGExDiscardByPointCount::OutputDiscardedLabel;
			PointIO->Enable();
		}

		(void)PointIO->StageOutput(Context);
	}

	return Context->TryComplete();
}

namespace PCGExDiscardSame
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Forward)

		// 1 - Build comparison points

		if (Settings->bTestAttributeHash)
		{
			Hasher = MakeShared<PCGEx::FAttributeHasher>(Settings->AttributeHashConfig);
			if (!Hasher->Init(Context, PointDataFacade)) { return false; }
			if (Hasher->RequiresCompilation()) { Hasher->Compile(TaskManager, nullptr); }
		}

		TSet<uint64> PositionHashes;
		const FVector PosCWTolerance = FVector(1 / Settings->TestPositionTolerance);

		const UPCGBasePointData* InPoints = PointDataFacade->GetIn();
		HashPointsCount = InPoints->GetNumPoints();

		if (Settings->bTestPositions) { PositionHashes.Reserve(HashPointsCount); }

		FBox Bounds = FBox(ForceInit);

		TConstPCGValueRange<FTransform> InTransforms = InPoints->GetConstTransformValueRange();

		for (const FTransform& InTransform : InTransforms)
		{
			const FVector& Pos = InTransform.GetLocation();
			Bounds += Pos;
			if (Settings->bTestPositions) { PositionHashes.Add(PCGEx::GH3(Pos, PosCWTolerance)); }
		}

		if (Settings->bTestPositions)
		{
			TArray<uint64> PosHashes = PositionHashes.Array();
			PositionHashes.Empty();
			PosHashes.Sort();
			HashPositions = CityHash64(reinterpret_cast<const char*>(PosHashes.GetData()), PosHashes.Num() * sizeof(int64));
		}
		else
		{
			HashPositions = PointDataFacade->Source->IOIndex;
		}

		if (Settings->bTestBounds)
		{
			const FVector BoundsCWTolerance = FVector(1 / Settings->TestBoundsTolerance);
			HashBounds = HashCombineFast(PCGEx::GH3(Bounds.Min, BoundsCWTolerance), PCGEx::GH3(Bounds.Max, BoundsCWTolerance));
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

		const TSharedPtr<PCGExPointsMT::TBatch<FProcessor>> Batch = StaticCastSharedPtr<PCGExPointsMT::TBatch<FProcessor>>(ParentBatch.Pin());

		PCGEX_SHARED_THIS_DECL

		const double Tol = Settings->TestPointCountTolerance;

		if (Settings->TestMode == EPCGExFilterGroupMode::AND)
		{
			for (int Pi = 0; Pi < Batch->GetNumProcessors(); Pi++)
			{
				const TSharedRef<FProcessor> P = Batch->GetProcessorRef<FProcessor>(Pi);
				if (P == ThisPtr) { continue; }

				if (Settings->bTestBounds && P->HashBounds != HashBounds) { continue; }
				if (Settings->bTestPositions && P->HashPositions != HashPositions) { continue; }
				if (Settings->bTestPointCount && !FMath::IsNearlyEqual(P->HashPointsCount, HashPointsCount, Tol)) { continue; }
				if (Settings->bTestAttributeHash && P->Hasher->GetHash() != Hasher->GetHash()) { continue; }

				SameAs.Add(P);
			}
		}
		else
		{
			for (int Pi = 0; Pi < Batch->GetNumProcessors(); Pi++)
			{
				const TSharedRef<FProcessor> P = Batch->GetProcessorRef<FProcessor>(Pi);
				if (P == ThisPtr) { continue; }

				if ((Settings->bTestBounds && P->HashBounds == HashBounds) || (Settings->bTestPositions && P->HashPositions == HashPositions) || (Settings->bTestPointCount && FMath::IsNearlyEqual(P->HashPointsCount, HashPointsCount, Tol)) || (Settings->bTestAttributeHash && P->Hasher->GetHash() != Hasher->GetHash()))
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

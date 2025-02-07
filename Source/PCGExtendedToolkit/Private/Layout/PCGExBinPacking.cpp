// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Layout/PCGExBinPacking.h"

#include "Layout/PCGExLayout.h"

#define LOCTEXT_NAMESPACE "PCGExBinPackingElement"
#define PCGEX_NAMESPACE BinPacking

PCGExData::EIOInit UPCGExBinPackingSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

bool UPCGExBinPackingSettings::GetSortingRules(FPCGExContext* InContext, TArray<FPCGExSortRuleConfig>& OutRules) const
{
	OutRules.Append(PCGExSorting::GetSortingRules(InContext, PCGExSorting::SourceSortingRules));
	return !OutRules.IsEmpty();
}

TArray<FPCGPinProperties> UPCGExBinPackingSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINTS(PCGExLayout::SourceBinsLabel, "List of bins to fit input points into. Each input collection is expected to have a matching collection of bins.", Required, {})
	PCGEX_PIN_FACTORIES(PCGExSorting::SourceSortingRules, "Plug sorting rules here. Order is defined by each rule' priority value, in ascending order.", Normal, {})
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExBinPackingSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExLayout::OutputBinsLabel, "Input bins, with added statistics.", Required, {})
	PCGEX_PIN_POINTS(PCGExLayout::OutputDiscardedLabel, "Discarded points, one that could not fit into any bin.", Required, {})
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BinPacking)

bool FPCGExBinPackingElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BinPacking)

	Context->Bins = MakeShared<PCGExData::FPointIOCollection>(InContext, PCGExLayout::SourceBinsLabel, PCGExData::EIOInit::None);
	Context->Bins->OutputPin = PCGExLayout::OutputBinsLabel;
	
	const int32 NumBins = Context->Bins->Num();
	const int32 NumInputs = Context->MainPoints->Num();

	if (NumBins != NumInputs)
	{
		if (NumBins > NumInputs)
		{
			if (!Settings->bQuietTooManyBinsWarning)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("There are more bins than there are inputs. Extra bins will be ignored."));
			}

			for (int i = 0; i < NumInputs; i++)
			{
				Context->MainPoints->Pairs[i]->InitializeOutput(PCGExData::EIOInit::Duplicate);
				Context->Bins->Pairs[i]->InitializeOutput(PCGExData::EIOInit::Duplicate);
				Context->Bins->Pairs[i]->OutputPin = Context->Bins->OutputPin;
			}
		}
		else if (NumInputs > NumBins)
		{
			if (!Settings->bQuietTooFewBinsWarning)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("There are more inputs than there are bins. Extra inputs will be ignored."));
			}

			for (int i = 0; i < NumBins; i++)
			{
				Context->MainPoints->Pairs[i]->InitializeOutput(PCGExData::EIOInit::Duplicate);
				Context->Bins->Pairs[i]->InitializeOutput(PCGExData::EIOInit::Duplicate);
				Context->Bins->Pairs[i]->OutputPin = Context->Bins->OutputPin;
			}
		}
	}
	else
	{
		for (int i = 0; i < NumInputs; i++)
		{
			Context->MainPoints->Pairs[i]->InitializeOutput(PCGExData::EIOInit::Duplicate);
			Context->Bins->Pairs[i]->InitializeOutput(PCGExData::EIOInit::Duplicate);
			Context->Bins->Pairs[i]->OutputPin = Context->Bins->OutputPin;
		}
	}

	Context->Discarded = MakeShared<PCGExData::FPointIOCollection>(InContext);
	Context->Discarded->OutputPin = PCGExLayout::OutputDiscardedLabel;

	return true;
}

bool FPCGExBinPackingElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBinPackingElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BinPacking)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBinPacking::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				return Entry->GetOut() != nullptr;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExBinPacking::FProcessor>>& NewBatch)
			{
				TArray<FPCGExSortRuleConfig> OutRules;
				Settings->GetSortingRules(Context, OutRules);
				NewBatch->bPrefetchData = !OutRules.IsEmpty();
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();
	Context->Bins->StageOutputs();
	Context->Discarded->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExBinPacking
{
	FBin::FBin(const FPCGPoint& InBinPoint)
	{
		BinBounds = PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(InBinPoint);
		Transform = InBinPoint.Transform;
		Transform.SetScale3D(FVector::OneVector); // Reset scale for later transform
		FreeSpaces.Add(BinBounds);
	}

	void FBin::AddItem(int32 SpaceIndex, FBinItem& InItem)
	{
		PlacedItems.Add(InItem);

		const FBox& Space = FreeSpaces[SpaceIndex];

		const FVector ItemSize = InItem.Box.GetSize();
		FVector ItemMin = Space.Min;

		for (int C = 0; C < 3; C++) { ItemMin[C] = FMath::Clamp(CenterOfGravity[C] - ItemSize[C] * 0.5, Space.Min[C], Space.Max[C] - ItemSize[C]); }

		const FVector ItemMax = ItemMin + ItemSize;
		InItem.Box = FBox(ItemMin, ItemMax);

		// TODO : Add buffer to ItemMin/ItemMax to "fill" the space and avoid extra small subdivisions
		
		FBox Left(Space.Min, FVector(ItemMin.X, Space.Max.Y, Space.Max.Z));
		FBox Right(FVector(ItemMax.X, Space.Min.Y, Space.Min.Z), Space.Max);

		FBox Bottom(FVector(ItemMin.X, Space.Min.Y, Space.Min.Z), FVector(ItemMax.X, Space.Max.Y, ItemMin.Z));
		FBox Top(FVector(ItemMin.X, ItemMin.Y, ItemMax.Z), FVector(ItemMax.X, ItemMax.Y, Space.Max.Z));

		FBox Front(FVector(ItemMin.X, ItemMax.Y, ItemMin.Z), FVector(ItemMax.X, Space.Max.Y, Space.Max.Z));
		FBox Back(FVector(ItemMin.X, Space.Min.Y, ItemMin.Z), FVector(ItemMax.X, ItemMin.Y, Space.Max.Z));

		FreeSpaces.RemoveAt(SpaceIndex);
		FreeSpaces.Reserve(FreeSpaces.Num() + 6);

		if (!FMath::IsNearlyZero(Left.GetVolume())) { FreeSpaces.Add(Left); }
		if (!FMath::IsNearlyZero(Right.GetVolume())) { FreeSpaces.Add(Right); }
		if (!FMath::IsNearlyZero(Bottom.GetVolume())) { FreeSpaces.Add(Bottom); }
		if (!FMath::IsNearlyZero(Top.GetVolume())) { FreeSpaces.Add(Top); }
		if (!FMath::IsNearlyZero(Front.GetVolume())) { FreeSpaces.Add(Front); }
		if (!FMath::IsNearlyZero(Back.GetVolume())) { FreeSpaces.Add(Back); }
	}


	bool FBin::Insert(FBinItem& InItem)
	{
		double SmallestRemainder = MAX_dbl;
		double SmallestDist = MAX_dbl;
		double BoxVolume = InItem.Box.GetVolume();
		int32 BestIndex = -1;
		const FVector ItemSize = InItem.Box.GetSize();

		for (int i = 0; i < FreeSpaces.Num(); i++)
		{
			const FBox& Space = FreeSpaces[i];

			// TODO : Rotate & try fit

			if (PCGExLayout::CanBoxFit(Space, ItemSize))
			{
				double Remainder = Space.GetVolume() - BoxVolume;
				if (SmallestRemainder >= Remainder)
				{
					// Find point in space closest to center of gravity
					FVector Min = FVector::ZeroVector;
					for (int C = 0; C < 3; C++) { Min[C] = FMath::Clamp(CenterOfGravity[C], Space.Min[C], Space.Max[C]); }

					const double Dist = FVector::DistSquared(Min, CenterOfGravity);
					if (SmallestRemainder == Remainder && Dist > SmallestDist) { continue; }

					SmallestDist = Dist;
					SmallestRemainder = Remainder;
					BestIndex = i;

					if (FMath::IsNearlyZero(Dist)) { break; }
				}
			}
		}

		if (BestIndex == -1) { return false; }

		// Can fit.
		// TODO : Check other bins as well, while it fits, this one may not be the ideal candidate
		AddItem(BestIndex, InItem);
		return true;
	}

	void FBin::UpdatePoint(FPCGPoint& InPoint, const FBinItem& InItem) const
	{
		const FTransform T = FTransform(FQuat::Identity, InItem.Box.GetCenter() - InPoint.GetLocalCenter(), InPoint.Transform.GetScale3D());
		InPoint.Transform = T * Transform;
	}


	void FProcessor::RegisterBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader)
	{
		TPointsProcessor::RegisterBuffersDependencies(FacadePreloader);

		TArray<FPCGExSortRuleConfig> RuleConfigs;
		if (Settings->GetSortingRules(ExecutionContext, RuleConfigs) && !RuleConfigs.IsEmpty())
		{
			Sorter = MakeShared<PCGExSorting::PointSorter<true>>(Context, PointDataFacade, RuleConfigs);
			Sorter->SortDirection = Settings->SortDirection;
			Sorter->RegisterBuffersDependencies(FacadePreloader);
		}
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBinPacking::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		Fitted.Init(false, PointDataFacade->GetNum());

		TSharedPtr<PCGExData::FPointIO> TargetBins = Context->Bins->Pairs[BatchIndex];
		Bins.Reserve(TargetBins->GetNum());
		for (int i = 0; i < TargetBins->GetNum(); i++)
		{
			PCGEX_MAKE_SHARED(NewBin, FBin, TargetBins->GetInPoint(i))
			NewBin->CenterOfGravity = NewBin->BinBounds.GetCenter() + NewBin->BinBounds.GetExtent() * Settings->BinUVW;
			Bins.Add(NewBin);
		}

		if (Sorter && Sorter->Init())
		{
			PointDataFacade->GetOut()->GetMutablePoints().Sort([&](const FPCGPoint& A, const FPCGPoint& B) { return Sorter->Sort(A, B); });
		}

		// OPTIM : Find the smallest bound dimension possible to use as min threshold for free spaces later on

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		FBinItem Item = FBinItem();
		Item.Index = Index;
		Item.Box = FBox(FVector::ZeroVector, PCGExMath::GetLocalBounds<EPCGExPointBoundsSource::ScaledBounds>(Point).GetSize());

		bool bPlaced = false;
		for (const TSharedPtr<FBin>& Bin : Bins)
		{
			if (Bin->Insert(Item))
			{
				bPlaced = true;
				Bin->UpdatePoint(Point, Item);
				break;
			}
		}

		Fitted[Index] = bPlaced;
		if (!bPlaced) { bHasUnfitted = true; }
	}

	void FProcessor::CompleteWork()
	{
		if (bHasUnfitted)
		{
			const TArray<FPCGPoint>& SourcePoints = PointDataFacade->GetIn()->GetPoints();
			TArray<FPCGPoint>& FittedPoints = PointDataFacade->GetMutablePoints();
			TArray<FPCGPoint>& DiscardedPoints = Context->Discarded->Emplace_GetRef(PointDataFacade->GetIn(), PCGExData::EIOInit::New)->GetMutablePoints();
			const int32 NumPoints = PointDataFacade->GetNum();
			int32 WriteIndex = 0;

			DiscardedPoints.Reserve(NumPoints);

			for (int32 i = 0; i < NumPoints; i++)
			{
				if (Fitted[i]) { FittedPoints[WriteIndex++] = FittedPoints[i]; }
				else { DiscardedPoints.Add(SourcePoints[i]); }
			}

			FittedPoints.SetNum(WriteIndex);
			DiscardedPoints.Shrink();
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

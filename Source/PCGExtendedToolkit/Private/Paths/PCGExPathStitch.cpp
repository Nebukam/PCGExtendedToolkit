// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathStitch.h"
#include "PCGExMath.h"
#include "Chaos/Deformable/MuscleActivationConstraints.h"
#include "Data/Blending/PCGExUnionBlender.h"


#include "Paths/SubPoints/DataBlending/PCGExSubPointsBlendInterpolate.h"

#define LOCTEXT_NAMESPACE "PCGExPathStitchElement"
#define PCGEX_NAMESPACE PathStitch

TArray<FPCGPinProperties> UPCGExPathStitchSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_FACTORIES(PCGExPaths::SourceCanCutFilters, "Fiter which edges can 'cut' other edges. Leave empty so all edges are can cut other edges.", Normal, {})
	PCGEX_PIN_FACTORIES(PCGExPaths::SourceCanBeCutFilters, "Fiter which edges can be 'cut' by other edges. Leave empty so all edges are can cut other edges.", Normal, {})
	PCGEX_PIN_OPERATION_OVERRIDES(PCGExDataBlending::SourceOverridesBlendingOps)
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(PathStitch)

bool FPCGExPathStitchElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathStitch)

	PCGEX_FWD(DotComparisonDetails)
	Context->DotComparisonDetails.Init();

	Context->Datas.Reset(Context->MainPoints->Pairs.Num());

	// TODO : Ignore & log closed loops (probably better moved to batch processing)

	return true;
}

bool FPCGExPathStitchElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathStitchElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathStitch)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs are either closed loop or have less than 2 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPathStitch::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2 || PCGExPaths::GetClosedLoop(Entry->GetIn()))
				{
					Entry->InitializeOutput(PCGExData::EIOInit::Forward);
					bHasInvalidInputs = true;
					return false;
				}

				Context->Datas.Add(FPCGTaggedData(Entry->GetIn(), Entry->Tags->Flatten(), NAME_None));
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExPathStitch::FProcessor>>& NewBatch)
			{
				//NewBatch->SetPointsFilterData(&Context->FilterFactories);
				//NewBatch->bRequiresWriteStep = Settings->bDoCrossBlending;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to work with."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExPathStitch
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathStitch::Process);

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		if (!IPointsProcessor::Process(InAsyncManager)) { return false; }
		const TConstPCGValueRange<FTransform> InTransform = PointDataFacade->GetIn()->GetConstTransformValueRange();

		StartSegment = PCGExMath::FSegment(InTransform[0].GetLocation(), InTransform[1].GetLocation(), Settings->Tolerance);
		EndSegment = PCGExMath::FSegment(InTransform[InTransform.Num() - 2].GetLocation(), InTransform[InTransform.Num() - 1].GetLocation(), Settings->Tolerance);

		return true;
	}

	FBatch::FBatch(FPCGExContext* InContext, const TArray<TWeakPtr<PCGExData::FPointIO>>& InPointsCollection)
		: TBatch(InContext, InPointsCollection)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathStitch);
	}

	void FBatch::OnInitialPostProcess()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathStitch);

		TBatch<FProcessor>::OnInitialPostProcess();

		TArray<TSharedPtr<FProcessor>> SortedProcessors;
		SortedProcessors.Reserve(Processors.Num());

		for (const TSharedRef<FProcessor>& Processor : this->Processors) { SortedProcessors.Add(Processor); }

		// Attempt to sort -- if it fails it's ok, just throw a warning

		TArray<FPCGExSortRuleConfig> RuleConfigs = PCGExSorting::GetSortingRules(Context, PCGExSorting::SourceSortingRules);
		if (!RuleConfigs.IsEmpty())
		{
			const TSharedPtr<PCGExSorting::FPointSorter> Sorter = MakeShared<PCGExSorting::FPointSorter>(RuleConfigs);
			Sorter->SortDirection = Settings->SortDirection;

			if (Sorter->Init(Context, Context->Datas))
			{
				SortedProcessors.Sort(
					[&](const TSharedPtr<FProcessor>& A, const TSharedPtr<FProcessor>& B)
					{
						return Sorter->SortData(A->BatchIndex, B->BatchIndex);
					});
			}
			else
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Problem with initializing sorting rules."));
			}
		}

		// Build data octree
		TSharedPtr<PCGEx::FIndexedItemOctree> PathOctree = MakeShared<PCGEx::FIndexedItemOctree>();
		for (int i = 0; i < SortedProcessors.Num(); ++i)
		{
			const TSharedPtr<FProcessor> Processor = SortedProcessors[i];
			Processor->WorkIndex = i;
			PathOctree->AddElement(PCGEx::FIndexedItem(Processor->BatchIndex, Processor->PointDataFacade->GetIn()->GetBounds()));
		}

		// ---A---x x---B---
		auto CanStitch = [&](const PCGExMath::FSegment& A, const PCGExMath::FSegment& B)
		{
			if (FVector::Dist(A.B, B.B) > Settings->Tolerance) { return false; }
			if (Settings->bDoRequireAlignment && !Context->DotComparisonDetails.Test(FVector::DotProduct(A.Direction, B.Direction * -1))) { return false; }
			return true;
		};

		// Resolve stitching
		for (int i = 0; i < SortedProcessors.Num(); ++i)
		{
			TSharedPtr<FProcessor> Current = SortedProcessors[i];
			if (!Current->IsAvailableForStitching()) { continue; }

			// Find candidates that could connect to this path' end first
			if (!Current->EndStitch)
			{
				const PCGExMath::FSegment& CurrentSegment = Current->EndSegment;
				PathOctree->FindFirstElementWithBoundsTest(
					CurrentSegment.Bounds, [&](const PCGEx::FIndexedItem& Item)
					{
						if (Current->EndStitch) { return false; }

						const TSharedPtr<FProcessor>& OtherProcessor = Processors[Item.Index];

						// Ignore anterior working paths
						if (OtherProcessor->WorkIndex < Current->WorkIndex) { return true; }

						if (!OtherProcessor->StartStitch &&
							CanStitch(CurrentSegment, Current->StartSegment))
						{
							Current->EndStitch = OtherProcessor;
							OtherProcessor->StartStitch = Current;
							return false;
						}
						else if (!OtherProcessor->EndStitch && !Settings->bOnlyMatchStartAndEnds &&
							CanStitch(CurrentSegment, Current->EndSegment))
						{
							Current->EndStitch = OtherProcessor;
							OtherProcessor->EndStitch = Current;
							return false;
						}
					});
			}

			if (!Current->StartStitch)
			{
				const PCGExMath::FSegment& CurrentSegment = Current->StartSegment;
				PathOctree->FindFirstElementWithBoundsTest(
					CurrentSegment.Bounds, [&](const PCGEx::FIndexedItem& Item)
					{
						if (Current->StartStitch) { return false; }

						const TSharedPtr<FProcessor>& OtherProcessor = Processors[Item.Index];

						// Ignore anterior working paths
						if (OtherProcessor->WorkIndex < Current->WorkIndex) { return true; }

						if (!OtherProcessor->EndStitch &&
							CanStitch(CurrentSegment, Current->EndSegment))
						{
							Current->StartStitch = OtherProcessor;
							OtherProcessor->EndStitch = Current;
							return false;
						}
						else if (!OtherProcessor->StartStitch && !Settings->bOnlyMatchStartAndEnds &&
							CanStitch(CurrentSegment, Current->StartSegment))
						{
							Current->StartStitch = OtherProcessor;
							OtherProcessor->StartStitch = Current;
							return false;
						}

						return true;
					});
			}
		}

		//
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

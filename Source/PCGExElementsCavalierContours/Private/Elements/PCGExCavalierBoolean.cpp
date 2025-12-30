// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExCavalierBoolean.h"

#include "Core/PCGExCCBoolean.h"
#include "Core/PCGExCCPolyline.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGExDataMatcher.h"
#include "Helpers/PCGExMatchingHelpers.h"
#include "Math/PCGExBestFitPlane.h"

#define LOCTEXT_NAMESPACE "PCGExCavalierBooleanElement"
#define PCGEX_NAMESPACE CavalierBoolean

PCGEX_INITIALIZE_ELEMENT(CavalierBoolean)

#if WITH_EDITOR
TArray<FPCGPreConfiguredSettingsInfo> UPCGExCavalierBooleanSettings::GetPreconfiguredInfo() const
{
	return FPCGPreConfiguredSettingsInfo::PopulateFromEnum<EPCGExCCBooleanOp>({}, FTEXT("Cavalier Boolean : {0}"));
}
#endif

TArray<FPCGPinProperties> UPCGExCavalierBooleanSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	// Add matching rules input when in matched mode
	if (DataMatching.IsEnabled())
	{
		PCGExMatching::Helpers::DeclareMatchingRulesInputs(DataMatching, PinProperties);
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExCavalierBooleanSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();

	// Add unmatched outputs when in matched mode
	if (DataMatching.IsEnabled())
	{
		PCGExMatching::Helpers::DeclareMatchingRulesOutputs(DataMatching, PinProperties);
	}

	return PinProperties;
}

void UPCGExCavalierBooleanSettings::ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfigureInfo)
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGExCCBooleanOp>())
	{
		if (EnumPtr->IsValidEnumValue(PreconfigureInfo.PreconfiguredIndex))
		{
			Operation = static_cast<EPCGExCCBooleanOp>(PreconfigureInfo.PreconfiguredIndex);
		}
	}
}

FPCGExGeo2DProjectionDetails UPCGExCavalierBooleanSettings::GetProjectionDetails() const
{
	return ProjectionDetails;
}

bool UPCGExCavalierBooleanSettings::NeedsOperands() const
{
	return Operation == EPCGExCCBooleanOp::Difference || DataMatching.IsEnabled() || Super::NeedsOperands();
}

bool FPCGExCavalierBooleanElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExCavalierProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CavalierBoolean)

	// Initialize data matcher for matched mode
	if (Settings->DataMatching.IsEnabled() &&
		Settings->DataMatching.Mode != EPCGExMapMatchMode::Disabled)
	{
		// Build tagged data from operands for matching
		Context->DataMatcher = MakeShared<PCGExMatching::FDataMatcher>();
		Context->DataMatcher->SetDetails(&Settings->DataMatching);

		if (!Context->DataMatcher->Init(Context, Context->OperandsFacades, false))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Failed to initialize data matcher."));
			Context->DataMatcher.Reset();
		}
	}

	if (Context->MainPolylines.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog, FTEXT("No valid closed paths found in main input."));
		return false;
	}

	return true;
}

bool FPCGExCavalierBooleanElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCavalierBooleanElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(CavalierBoolean)
	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		TArray<PCGExCavalier::FPolyline> Results;

		if (!Settings->DataMatching.IsEnabled()) { Results = ExecuteCombineAll(Context, Settings); }
		else { Results = ExecuteMatched(Context, Settings); }

		// Output results
		for (PCGExCavalier::FPolyline& ResultPline : Results)
		{
			if (Settings->bTessellateArcs)
			{
				PCGExCavalier::FPolyline Tessellated = ResultPline.Tessellated(Settings->ArcTessellationSettings);
				Context->OutputPolyline(Tessellated, false, Context->ProjectionDetails);
			}
			else
			{
				Context->OutputPolyline(ResultPline, false, Context->ProjectionDetails);
			}
		}

		Context->Done();
	}

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	return Context->TryComplete();
}

TArray<PCGExCavalier::FPolyline> FPCGExCavalierBooleanElement::ExecuteCombineAll(
	FPCGExCavalierBooleanContext* Context,
	const UPCGExCavalierBooleanSettings* Settings) const
{
	using namespace PCGExCavalier;
	using namespace PCGExCavalier::BooleanOps;

	TArray<FPolyline> Results;

	// Combine all polylines into one array
	TArray<FPolyline> AllPolylines;
	AllPolylines.Reserve(Context->MainPolylines.Num() + Context->OperandPolylines.Num());
	AllPolylines.Append(Context->MainPolylines);
	AllPolylines.Append(Context->OperandPolylines);

	if (AllPolylines.IsEmpty()) return Results;
	if (AllPolylines.Num() == 1)
	{
		Results.Add(MoveTemp(AllPolylines[0]));
		return Results;
	}

	// For difference, we treat first polyline(s) from main as base
	if (Settings->Operation == EPCGExCCBooleanOp::Difference)
	{
		// Union all main polylines as base
		FBooleanResult BaseResult = PerformMultiBoolean(
			Context->MainPolylines, EPCGExCCBooleanOp::Union, Settings->BooleanOptions);

		if (!BaseResult.HasResult())
		{
			// No valid base, return empty
			return Results;
		}

		// Union all operand polylines as subtractor
		if (Context->OperandPolylines.IsEmpty())
		{
			// No operands, return base unchanged
			Results.Append(MoveTemp(BaseResult.PositivePolylines));
			return Results;
		}

		FBooleanResult SubtractorResult = PerformMultiBoolean(
			Context->OperandPolylines, EPCGExCCBooleanOp::Union, Settings->BooleanOptions);

		if (!SubtractorResult.HasResult())
		{
			// No valid subtractor, return base unchanged
			Results.Append(MoveTemp(BaseResult.PositivePolylines));
			return Results;
		}

		// Perform difference: Base - Subtractor
		for (const FPolyline& BasePline : BaseResult.PositivePolylines)
		{
			TArray<FPolyline> CurrentBase;
			CurrentBase.Add(BasePline);

			for (const FPolyline& SubPline : SubtractorResult.PositivePolylines)
			{
				TArray<FPolyline> NextBase;

				for (const FPolyline& B : CurrentBase)
				{
					FBooleanResult DiffResult = PerformBoolean(
						FBooleanOperand(B, B.GetPrimaryPathId()),
						FBooleanOperand(SubPline, SubPline.GetPrimaryPathId()),
						EPCGExCCBooleanOp::Difference, Settings->BooleanOptions);

					NextBase.Append(MoveTemp(DiffResult.PositivePolylines));

					// Handle negative space if requested
					if (Settings->bOutputNegativeSpace)
					{
						for (FPolyline& NegPline : DiffResult.NegativePolylines)
						{
							Context->OutputPolyline(NegPline, true, Context->ProjectionDetails);
						}
					}
				}

				CurrentBase = MoveTemp(NextBase);
				if (CurrentBase.IsEmpty()) break;
			}

			Results.Append(MoveTemp(CurrentBase));
		}
	}
	else
	{
		// Union, Intersection, or XOR - apply to all polylines
		FBooleanResult Result = PerformMultiBoolean(AllPolylines, Settings->Operation, Settings->BooleanOptions);
		Results.Append(MoveTemp(Result.PositivePolylines));

		// Handle negative space if requested
		if (Settings->bOutputNegativeSpace)
		{
			for (FPolyline& NegPline : Result.NegativePolylines)
			{
				Context->OutputPolyline(NegPline, true, Context->ProjectionDetails);
			}
		}
	}

	return Results;
}

TArray<PCGExCavalier::FPolyline> FPCGExCavalierBooleanElement::ExecuteMatched(
	FPCGExCavalierBooleanContext* Context,
	const UPCGExCavalierBooleanSettings* Settings) const
{
	using namespace PCGExCavalier;
	using namespace PCGExCavalier::BooleanOps;

	TArray<FPolyline> Results;

	if (Context->MainPolylines.IsEmpty()) return Results;

	// If no matcher or operands, just return main polylines
	if (!Context->DataMatcher || Context->OperandPolylines.IsEmpty())
	{
		Results.Append(Context->MainPolylines);
		return Results;
	}

	// Process each main polyline
	for (int32 MainIdx = 0; MainIdx < Context->MainPolylines.Num(); ++MainIdx)
	{
		const FPolyline& MainPline = Context->MainPolylines[MainIdx];
		const TSharedPtr<PCGExData::FFacade>& MainFacade = Context->MainFacades[MainIdx];

		if (!MainFacade) continue;

		// Find matching operands using the data matcher
		PCGExMatching::FScope MatchingScope(Context->OperandsFacades.Num(), false);
		TArray<int32> MatchedIndices;

		Context->DataMatcher->GetMatchingSourcesIndices(MainFacade->Source->GetTaggedData(), MatchingScope, MatchedIndices);

		if (MatchedIndices.IsEmpty())
		{
			// No matches, output main unchanged (or handle unmatched)
			if (Context->DataMatcher->HandleUnmatchedOutput(MainFacade, true)) { Results.Add(MainPline); }
			continue;
		}

		// Collect matched operand polylines
		TArray<FPolyline> MatchedOperands;
		MatchedOperands.Reserve(MatchedIndices.Num());
		for (int32 OpIdx : MatchedIndices)
		{
			if (OpIdx >= 0 && OpIdx < Context->OperandPolylines.Num())
			{
				MatchedOperands.Add(Context->OperandPolylines[OpIdx]);
			}
		}

		if (MatchedOperands.IsEmpty())
		{
			Results.Add(MainPline);
			continue;
		}

		// Perform boolean operation with matched operands
		TArray<FPolyline> OperationInput;
		OperationInput.Add(MainPline);
		OperationInput.Append(MatchedOperands);

		if (Settings->Operation == EPCGExCCBooleanOp::Difference)
		{
			// For difference: Main - Union(MatchedOperands)
			FBooleanResult SubtractorResult = PerformMultiBoolean(
				MatchedOperands, EPCGExCCBooleanOp::Union, Settings->BooleanOptions);

			TArray<FPolyline> CurrentBase;
			CurrentBase.Add(MainPline);

			for (const FPolyline& SubPline : SubtractorResult.PositivePolylines)
			{
				TArray<FPolyline> NextBase;

				for (const FPolyline& B : CurrentBase)
				{
					FBooleanResult DiffResult = PerformBoolean(
						FBooleanOperand(B, B.GetPrimaryPathId()),
						FBooleanOperand(SubPline, SubPline.GetPrimaryPathId()),
						EPCGExCCBooleanOp::Difference, Settings->BooleanOptions);

					NextBase.Append(MoveTemp(DiffResult.PositivePolylines));

					if (Settings->bOutputNegativeSpace)
					{
						for (FPolyline& NegPline : DiffResult.NegativePolylines)
						{
							Context->OutputPolyline(NegPline, true, Context->ProjectionDetails);
						}
					}
				}

				CurrentBase = MoveTemp(NextBase);
				if (CurrentBase.IsEmpty()) break;
			}

			Results.Append(MoveTemp(CurrentBase));
		}
		else
		{
			// Union, Intersection, XOR
			FBooleanResult Result = PerformMultiBoolean(OperationInput, Settings->Operation, Settings->BooleanOptions);
			Results.Append(MoveTemp(Result.PositivePolylines));

			if (Settings->bOutputNegativeSpace)
			{
				for (FPolyline& NegPline : Result.NegativePolylines)
				{
					Context->OutputPolyline(NegPline, true, Context->ProjectionDetails);
				}
			}
		}
	}

	return Results;
}

PCGExCavalier::BooleanOps::FBooleanResult FPCGExCavalierBooleanElement::PerformMultiBoolean(
	const TArray<PCGExCavalier::FPolyline>& Polylines,
	EPCGExCCBooleanOp Operation,
	const FPCGExContourBooleanOptions& Options) const
{
	using namespace PCGExCavalier;
	using namespace PCGExCavalier::BooleanOps;

	FBooleanResult Result;

	if (Polylines.IsEmpty())
	{
		Result.ResultInfo = EBooleanResultInfo::InvalidInput;
		return Result;
	}

	if (Polylines.Num() == 1)
	{
		Result.PositivePolylines.Add(Polylines[0]);
		Result.ResultInfo = EBooleanResultInfo::Disjoint;
		return Result;
	}

	auto MakeOperand = [](const FPolyline& P) -> FBooleanOperand
	{
		return FBooleanOperand(&P, P.GetPrimaryPathId());
	};

	// Helper to check if two polylines' bounding boxes overlap (potential intersection)
	auto BoundsOverlap = [](const FPolyline& A, const FPolyline& B) -> bool
	{
		const FBox2D BoxA = A.BoundingBox();
		const FBox2D BoxB = B.BoundingBox();
		return BoxA.Intersect(BoxB);
	};

	switch (Operation)
	{
	case EPCGExCCBooleanOp::Union:
	case EPCGExCCBooleanOp::Xor:
		{
			// Greedy merge: repeatedly find ANY pair that actually merges
			// (not just adjacent indices) until no more merges possible
			TArray<FPolyline> Working = Polylines;

			const int32 MaxIterations = Polylines.Num() * Polylines.Num();
			int32 IterationCount = 0;
			bool bMadeProgress = true;

			while (bMadeProgress && Working.Num() > 1 && IterationCount < MaxIterations)
			{
				++IterationCount;
				bMadeProgress = false;

				// Try ALL pairs, not just adjacent indices
				for (int32 i = 0; i < Working.Num() && !bMadeProgress; ++i)
				{
					for (int32 j = i + 1; j < Working.Num() && !bMadeProgress; ++j)
					{
						FBooleanResult Partial = PerformBoolean(
							MakeOperand(Working[i]),
							MakeOperand(Working[j]),
							Operation, Options);

						// FIX: Check ResultInfo, not polyline count!
						// Disjoint = no relationship, shapes don't interact
						// Any other result = shapes were related and processed
						if (Partial.ResultInfo != EBooleanResultInfo::Disjoint)
						{
							// Remove originals (higher index first)
							Working.RemoveAt(j);
							Working.RemoveAt(i);

							// Add result(s)
							Working.Append(MoveTemp(Partial.PositivePolylines));

							bMadeProgress = true;
						}
					}
				}
			}

			Result.PositivePolylines = MoveTemp(Working);
			Result.ResultInfo = Working.Num() < Polylines.Num()
				                    ? EBooleanResultInfo::Intersected
				                    : EBooleanResultInfo::Disjoint;
		}
		break;

	case EPCGExCCBooleanOp::Intersection:
		{
			// Sequential with early termination, sorted by area (smallest first)
			TArray<FPolyline> Sorted = Polylines;
			Sorted.Sort([](const FPolyline& A, const FPolyline& B)
			{
				return FMath::Abs(A.Area()) < FMath::Abs(B.Area());
			});

			TArray<FPolyline> Current;
			Current.Add(MoveTemp(Sorted[0]));

			for (int32 i = 1; i < Sorted.Num(); ++i)
			{
				TArray<FPolyline> Next;

				for (const FPolyline& Existing : Current)
				{
					FBooleanResult Partial = PerformBoolean(
						MakeOperand(Existing),
						MakeOperand(Sorted[i]),
						EPCGExCCBooleanOp::Intersection, Options);

					Next.Append(MoveTemp(Partial.PositivePolylines));
				}

				if (Next.IsEmpty())
				{
					// Early termination - no intersection possible
					Result.ResultInfo = EBooleanResultInfo::Disjoint;
					return Result;
				}

				Current = MoveTemp(Next);
			}

			Result.PositivePolylines = MoveTemp(Current);
			Result.ResultInfo = EBooleanResultInfo::Intersected;
		}
		break;

	case EPCGExCCBooleanOp::Difference:
		{
			// Sequential subtraction: ((A - B) - C) - D ...
			TArray<FPolyline> Current;
			Current.Add(Polylines[0]);

			for (int32 i = 1; i < Polylines.Num(); ++i)
			{
				TArray<FPolyline> Next;

				for (const FPolyline& Existing : Current)
				{
					FBooleanResult Partial = PerformBoolean(
						MakeOperand(Existing),
						MakeOperand(Polylines[i]),
						EPCGExCCBooleanOp::Difference, Options);

					Next.Append(MoveTemp(Partial.PositivePolylines));
					Result.NegativePolylines.Append(MoveTemp(Partial.NegativePolylines));
				}

				if (Next.IsEmpty()) break;
				Current = MoveTemp(Next);
			}

			Result.PositivePolylines = MoveTemp(Current);
			Result.ResultInfo = EBooleanResultInfo::Intersected;
		}
		break;
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

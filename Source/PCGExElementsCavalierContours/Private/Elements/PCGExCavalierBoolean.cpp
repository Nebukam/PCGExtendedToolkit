// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExCavalierBoolean.h"

#include "Core/PCGExCCBoolean.h"
#include "Core/PCGExCCPolyline.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGExAsyncHelpers.h"
#include "Helpers/PCGExDataMatcher.h"
#include "Helpers/PCGExMatchingHelpers.h"
#include "Math/PCGExBestFitPlane.h"
#include "Paths/PCGExPathsHelpers.h"

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

	// Add operands pin for Difference or when using matched mode
	if (NeedsOperands())
	{
		PCGEX_PIN_POINTS(FName("Operands"), "Secondary paths to use as operands in the boolean operation.", Required)
	}

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

bool UPCGExCavalierBooleanSettings::NeedsOperands() const
{
	return Operation == EPCGExCCBooleanOp::Difference || DataMatching.IsEnabled();
}

bool FPCGExCavalierBooleanElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CavalierBoolean)

	// Initialize projection
	Context->ProjectionDetails = Settings->ProjectionDetails;

	// Check for operands input
	if (Settings->NeedsOperands())
	{
		Context->OperandsCollection = MakeShared<PCGExData::FPointIOCollection>(InContext, FName("Operands"), PCGExData::EIOInit::NoInit, true);

		if (Context->OperandsCollection->IsEmpty())
		{
			PCGEX_LOG_MISSING_INPUT(Context, FTEXT("Operands input is required for this operation mode."));
			return false;
		}
	}

	// Reserve space
	const int32 NumMain = Context->MainPoints->Num();
	const int32 NumOperands = Context->OperandsCollection ? Context->OperandsCollection->Num() : 0;
	const int32 TotalInputs = NumMain + NumOperands;

	Context->RootPaths.Reserve(TotalInputs);
	Context->MainPolylines.Reserve(NumMain);
	Context->MainPolylinesSourceIO.Reserve(NumMain);

	if (Context->OperandsCollection)
	{
		Context->OperandPolylines.Reserve(NumOperands);
		Context->OperandPolylinesSourceIO.Reserve(NumOperands);
	}

	// Build polylines from main input (parallel)
	BuildPolylinesFromCollection(
		Context, Settings,
		Context->MainPoints.Get(),
		Context->MainPolylines,
		Context->MainPolylinesSourceIO);

	// Build polylines from operands input (parallel)
	if (Context->OperandsCollection)
	{
		BuildPolylinesFromCollection(
			Context, Settings,
			Context->OperandsCollection.Get(),
			Context->OperandPolylines,
			Context->OperandPolylinesSourceIO);
	}

	// Initialize data matcher for matched mode
	if (Settings->DataMatching.IsEnabled() &&
		Settings->DataMatching.Mode != EPCGExMapMatchMode::Disabled)
	{
		// Build tagged data from operands for matching
		Context->DataMatcher = MakeShared<PCGExMatching::FDataMatcher>();
		Context->DataMatcher->SetDetails(&Settings->DataMatching);

		// Collect operand facades for matching
		TArray<TSharedPtr<PCGExData::FFacade>> OperandFacades;
		OperandFacades.Reserve(Context->OperandPolylinesSourceIO.Num());
		for (const TSharedPtr<PCGExData::FPointIO>& IO : Context->OperandPolylinesSourceIO)
		{
			if (IO)
			{
				OperandFacades.Add(MakeShared<PCGExData::FFacade>(IO.ToSharedRef()));
			}
		}

		if (!Context->DataMatcher->Init(Context, OperandFacades, false))
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

void FPCGExCavalierBooleanElement::BuildPolylinesFromCollection(
	FPCGExCavalierBooleanContext* Context,
	const UPCGExCavalierBooleanSettings* Settings,
	PCGExData::FPointIOCollection* Collection,
	TArray<PCGExCavalier::FPolyline>& OutPolylines,
	TArray<TSharedPtr<PCGExData::FPointIO>>& OutSourceIOs) const
{
	if (!Collection) return;

	const int32 NumInputs = Collection->Num();
	if (NumInputs == 0) return;

	// Thread-safe containers for parallel building
	FCriticalSection Lock;

	struct FBuildResult
	{
		PCGExCavalier::FRootPath RootPath;
		PCGExCavalier::FPolyline Polyline;
		TSharedPtr<PCGExData::FPointIO> SourceIO;
		bool bValid = false;
	};

	TArray<FBuildResult> Results;
	Results.SetNum(NumInputs);

	{
		PCGExAsyncHelpers::FAsyncExecutionScope BuildScope(NumInputs);

		for (int32 i = 0; i < NumInputs; ++i)
		{
			const TSharedPtr<PCGExData::FPointIO>& IO = (*Collection)[i];

			BuildScope.Execute([&, IO, i]()
			{
				FBuildResult& Result = Results[i];

				// Skip paths with insufficient points
				if (IO->GetNum() < 3) return;

				// Check if closed (required for boolean ops)
				const bool bIsClosed = PCGExPaths::Helpers::GetClosedLoop(IO->GetIn());
				if (!bIsClosed && Settings->bSkipOpenPaths) return;

				// Allocate unique path ID
				const int32 PathId = Context->AllocatePathId();

				// Initialize projection for this path
				FPCGExGeo2DProjectionDetails LocalProjection = Context->ProjectionDetails;
				if (LocalProjection.Method == EPCGExProjectionMethod::Normal)
				{
					TSharedPtr<PCGExData::FFacade> Facade = MakeShared<PCGExData::FFacade>(IO.ToSharedRef());
					if (!LocalProjection.Init(Facade)) { return; }
				}
				else
				{
					LocalProjection.Init(PCGExMath::FBestFitPlane(IO->GetIn()->GetConstTransformValueRange()));
				}

				// Build root path
				Result.RootPath = PCGExCavalier::FRootPath(PathId, true);
				Result.RootPath.Reserve(IO->GetNum());

				TConstPCGValueRange<FTransform> Transforms = IO->GetIn()->GetConstTransformValueRange();
				for (int32 j = 0; j < IO->GetNum(); ++j)
				{
					Result.RootPath.AddPoint(LocalProjection.Project(Transforms[j], j));
				}

				// Convert to polyline
				Result.Polyline = PCGExCavalier::FContourUtils::CreateFromRootPath(Result.RootPath, Settings->bAddFuzzinessToPositions);
				Result.Polyline.SetClosed(true);

				Result.SourceIO = IO;
				Result.bValid = true;
			});
		}
	}

	// Collect results (single-threaded)
	for (FBuildResult& Result : Results)
	{
		if (!Result.bValid) continue;

		const int32 PathId = Result.RootPath.PathId;
		Context->RootPaths.Add(PathId, MoveTemp(Result.RootPath));
		OutPolylines.Add(MoveTemp(Result.Polyline));
		OutSourceIOs.Add(Result.SourceIO);
	}
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
				PCGExCavalier::FPolyline Tessellated = ResultPline.Tessellated(Settings->TessellationSettings);
				OutputPolyline(Context, Settings, Tessellated, false);
			}
			else
			{
				OutputPolyline(Context, Settings, ResultPline, false);
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
							OutputPolyline(Context, Settings, NegPline, true);
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
				OutputPolyline(Context, Settings, NegPline, true);
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
		const TSharedPtr<PCGExData::FPointIO>& MainIO = Context->MainPolylinesSourceIO[MainIdx];

		if (!MainIO) continue;

		// Find matching operands using the data matcher
		PCGExMatching::FScope MatchingScope(Context->OperandPolylinesSourceIO.Num(), false);
		TArray<int32> MatchedIndices;

		Context->DataMatcher->GetMatchingTargets(MainIO, MatchingScope, MatchedIndices);

		if (MatchedIndices.IsEmpty())
		{
			// No matches, output main unchanged (or handle unmatched)
			if (Context->DataMatcher->HandleUnmatchedOutput(
				MakeShared<PCGExData::FFacade>(MainIO.ToSharedRef()), true))
			{
				Results.Add(MainPline);
			}
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
							OutputPolyline(Context, Settings, NegPline, true);
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
					OutputPolyline(Context, Settings, NegPline, true);
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

	switch (Operation)
	{
	case EPCGExCCBooleanOp::Union:
	case EPCGExCCBooleanOp::Xor:
		{
			// Tree reduction - can be parallelized
			TArray<FPolyline> Current = Polylines;

			while (Current.Num() > 1)
			{
				TArray<FPolyline> NextLevel;

				NextLevel.Reserve((Current.Num() + 1) / 2);

				for (int32 i = 0; i < Current.Num(); i += 2)
				{
					if (i + 1 < Current.Num())
					{
						FBooleanResult Partial = PerformBoolean(
							MakeOperand(Current[i]),
							MakeOperand(Current[i + 1]),
							Operation, Options);

						NextLevel.Append(MoveTemp(Partial.PositivePolylines));
					}
					else
					{
						NextLevel.Add(MoveTemp(Current[i]));
					}
				}

				Current = MoveTemp(NextLevel);
			}

			Result.PositivePolylines = MoveTemp(Current);
			Result.ResultInfo = EBooleanResultInfo::Intersected;
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
			// This should be handled specially - difference is not associative
			// Just do sequential subtraction: ((A - B) - C) - D ...
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

void FPCGExCavalierBooleanElement::OutputPolyline(
	FPCGExCavalierBooleanContext* Context,
	const UPCGExCavalierBooleanSettings* Settings,
	PCGExCavalier::FPolyline& Polyline,
	bool bIsNegativeSpace) const
{
	const int32 NumVertices = Polyline.VertexCount();
	if (NumVertices < 3) return;

	// Convert back to 3D using source tracking
	PCGExCavalier::FContourResult3D Result3D = PCGExCavalier::FContourUtils::ConvertTo3D(
		Polyline, Context->RootPaths, Settings->bBlendTransforms);

	// Find a source IO for metadata inheritance
	TSharedPtr<PCGExData::FPointIO> SourceIO = FindSourceIO(Context, Polyline);

	// Create output
	const TSharedPtr<PCGExData::FPointIO> PathIO = Context->MainPoints->Emplace_GetRef(
		SourceIO, PCGExData::EIOInit::New);

	if (!PathIO) return;

	EPCGPointNativeProperties Allocations = EPCGPointNativeProperties::Transform;
	if (SourceIO)
	{
		const TSharedPtr<PCGExData::FFacade> SourceFacade = MakeShared<PCGExData::FFacade>(SourceIO.ToSharedRef());
		Allocations |= SourceFacade->GetAllocations();
	}

	PCGExPointArrayDataHelpers::SetNumPointsAllocated(PathIO->GetOut(), NumVertices, Allocations);
	//TArray<int32>& IdxMapping = PathIO->GetIdxMapping(NumVertices);

	TPCGValueRange<FTransform> OutTransforms = PathIO->GetOut()->GetTransformValueRange();
	for (int32 i = 0; i < NumVertices; ++i)
	{
		const int32 OriginalIndex = Result3D.GetPointIndex(i);
		//IdxMapping[i] = FMath::Max(0, OriginalIndex); // Ensure valid index for mapping

		// Full transform with proper Z, rotation, and scale from source
		OutTransforms[i] = Context->ProjectionDetails.Restore(Result3D.Transforms[i], OriginalIndex);
	}

	EnumRemoveFlags(Allocations, EPCGPointNativeProperties::Transform);
	//PathIO->ConsumeIdxMapping(Allocations);

	PCGExPaths::Helpers::SetClosedLoop(PathIO, Polyline.IsClosed());

	// Tag negative space outputs
	if (bIsNegativeSpace && Settings->bOutputNegativeSpace)
	{
		PathIO->Tags->AddRaw(Settings->NegativeSpaceTag);
	}
}

TSharedPtr<PCGExData::FPointIO> FPCGExCavalierBooleanElement::FindSourceIO(
	FPCGExCavalierBooleanContext* Context,
	const PCGExCavalier::FPolyline& Polyline) const
{
	const TSet<int32>& ContributingIds = Polyline.GetContributingPathIds();

	// Try to find a source IO from the contributing paths
	for (int32 PathId : ContributingIds)
	{
		// Check main polylines
		for (int32 i = 0; i < Context->MainPolylines.Num(); ++i)
		{
			if (Context->MainPolylines[i].GetPrimaryPathId() == PathId)
			{
				if (Context->MainPolylinesSourceIO.IsValidIndex(i) && Context->MainPolylinesSourceIO[i])
				{
					return Context->MainPolylinesSourceIO[i];
				}
			}
		}

		// Check operand polylines
		for (int32 i = 0; i < Context->OperandPolylines.Num(); ++i)
		{
			if (Context->OperandPolylines[i].GetPrimaryPathId() == PathId)
			{
				if (Context->OperandPolylinesSourceIO.IsValidIndex(i) && Context->OperandPolylinesSourceIO[i])
				{
					return Context->OperandPolylinesSourceIO[i];
				}
			}
		}
	}

	// Fallback: use first main polyline source if available
	if (!Context->MainPolylinesSourceIO.IsEmpty() && Context->MainPolylinesSourceIO[0])
	{
		return Context->MainPolylinesSourceIO[0];
	}

	return nullptr;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExClipper2Processor.h"

#include "Core/PCGExClipper2Common.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGExArrayHelpers.h"
#include "Helpers/PCGExAsyncHelpers.h"
#include "Helpers/PCGExDataMatcher.h"
#include "Helpers/PCGExMatchingHelpers.h"
#include "Math/PCGExBestFitPlane.h"
#include "Paths/PCGExPath.h"
#include "Paths/PCGExPathsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExClipper2ProcessorElement"

namespace PCGExClipper2
{
	FOpData::FOpData(const int32 InReserve)
	{
		Facades = MakeShared<TArray<TSharedPtr<PCGExData::FFacade>>>();
		AddReserve(InReserve);
	}

	void FOpData::AddReserve(const int32 InReserve)
	{
		const int32 Reserve = Paths.Num() + InReserve;
		Facades->Reserve(Reserve);
		Paths.Reserve(Reserve);
		IsClosedLoop.Reserve(Reserve);
		Projections.Reserve(Reserve);
	}
}

UPCGExClipper2ProcessorSettings::UPCGExClipper2ProcessorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPCGExClipper2ProcessorSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if (InPin->Properties.Label == PCGExClipper2::Labels::SourceOperandsLabel) { return NeedsOperands(); }
	return Super::IsPinUsedByNodeExecution(InPin);
}

TArray<FPCGPinProperties> UPCGExClipper2ProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGExMatching::Helpers::DeclareMatchingRulesInputs(MainDataMatching, PinProperties);

	if (NeedsOperands())
	{
		PCGEX_PIN_POINTS(PCGExClipper2::Labels::SourceOperandsLabel, "Operands", Required)
		PCGExMatching::Helpers::DeclareMatchingRulesInputs(OperandsDataMatching, PinProperties, PCGExClipper2::Labels::SourceOperandsMatchRulesLabel);
	}
	else
	{
		PCGEX_PIN_POINTS(PCGExClipper2::Labels::SourceOperandsLabel, "Operands", Advanced)

		FPCGExMatchingDetails OperandsDataMatchingCopy = OperandsDataMatching;
		OperandsDataMatchingCopy.Mode = EPCGExMapMatchMode::Disabled;
		PCGExMatching::Helpers::DeclareMatchingRulesInputs(OperandsDataMatchingCopy, PinProperties, PCGExClipper2::Labels::SourceOperandsMatchRulesLabel);
	}

	return PinProperties;
}

bool UPCGExClipper2ProcessorSettings::NeedsOperands() const
{
	return false;
}

FPCGExGeo2DProjectionDetails UPCGExClipper2ProcessorSettings::GetProjectionDetails() const
{
	return FPCGExGeo2DProjectionDetails();
}

void FPCGExClipper2ProcessorContext::OutputPaths64(PCGExClipper2Lib::Paths64& InPaths, TArray<TSharedPtr<PCGExData::FPointIO>>& OutPaths) const
{
	const UPCGExClipper2ProcessorSettings* Settings = GetInputSettings<UPCGExClipper2ProcessorSettings>();
	check(Settings)
	
	// Need to loop over every InPaths
	
	// TODO : InPaths contains the index of the originating point & source data index as H64
	// We can restore transform and metadata from there
}

void FPCGExClipper2ProcessorContext::Process(const TArray<int32>& Subjects, const TArray<int32>* Operands)
{
	// TODO : Each processor is responsible for this implementation
	// and should use OutputPaths64 to output the result of their operations
}

bool FPCGExClipper2ProcessorElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Clipper2Processor)

	// Initialize projection
	Context->ProjectionDetails = Settings->GetProjectionDetails();

	// Check for operands input
	if (Settings->NeedsOperands())
	{
		Context->OperandsCollection = MakeShared<PCGExData::FPointIOCollection>(InContext, PCGExClipper2::Labels::SourceOperandsLabel, PCGExData::EIOInit::NoInit, false);

		if (Context->OperandsCollection->IsEmpty())
		{
			PCGEX_LOG_MISSING_INPUT(Context, FTEXT("Operands input is required for this operation mode."));
			return false;
		}
	}

	const int32 TotalInputNum = Context->MainPoints->Num() + (Context->OperandsCollection ? Context->OperandsCollection->Num() : 0);
	Context->AllOpData->AddReserve(TotalInputNum);


	int32 NumInputs = BuildDataFromCollection(Context, Settings, Context->MainPoints, Context->MainOpDataPartitions);
	if (NumInputs != Context->MainPoints->Num())
	{
		PCGEX_LOG_INVALID_INPUT(Context, FTEXT("Some inputs have less than 2 points and won't be processed."))
	}

	if (!NumInputs)
	{
		PCGE_LOG(Warning, GraphAndLog, FTEXT("No valid paths found in main input."));
		return false;
	}

	const TArray<TSharedPtr<PCGExData::FFacade>> MainFacades = *Context->AllOpData->Facades.Get();

	{
		// Partition main data before adding operands
		bool bDoMatching = false;
		if (Settings->MainDataMatching.IsEnabled() &&
			Settings->MainDataMatching.Mode != EPCGExMapMatchMode::Disabled)
		{
			// Build tagged data from operands for matching
			auto Matcher = MakeShared<PCGExMatching::FDataMatcher>();
			Matcher->SetDetails(&Settings->MainDataMatching);

			// Collect operand facades for matching
			bDoMatching = Matcher->Init(Context, MainFacades, true);

			if (bDoMatching)
			{
				PCGExMatching::Helpers::GetMatchingSourcePartitions(Matcher, MainFacades, Context->MainOpDataPartitions, true);
			}
			else
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Failed to initialize data matcher."));
			}
		}

		if (!bDoMatching)
		{
			Context->MainOpDataPartitions.Reserve(MainFacades.Num());
			for (int i = 0; i < MainFacades.Num(); i++) { Context->MainOpDataPartitions.Add({i}); }
		}

		// Create placeholder for operands partitions matching OpData partitions
		Context->OperandsOpDataPartitions.Reserve(Context->MainOpDataPartitions.Num());
		for (int i = 0; i < Context->OperandsOpDataPartitions.Num(); i++) { Context->OperandsOpDataPartitions.Add({}); }
	}

	if (Context->OperandsCollection)
	{
		const int32 IndexOffset = Context->AllOpData->Num();

		// Build polylines from operands input (parallel)
		NumInputs = BuildDataFromCollection(Context, Settings, Context->OperandsCollection, Context->OperandsOpDataPartitions);
		if (NumInputs != Context->OperandsCollection->Num())
		{
			PCGEX_LOG_INVALID_INPUT(Context, FTEXT("Some operands have less than 2 points and won't be processed."))
		}

		if (!NumInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("No valid operands found in operands input."));
			return false;
		}

		TArray<TSharedPtr<PCGExData::FFacade>> OperandsFacades;
		OperandsFacades.Reserve(NumInputs);

		for (int i = 0; i < NumInputs; i++) { OperandsFacades.Add(Context->AllOpData->Facades->Last(NumInputs - (i + 1))); }

		// Partition main data before adding operands
		bool bDoOperandMatching = false;
		if (Settings->OperandsDataMatching.IsEnabled() &&
			Settings->OperandsDataMatching.Mode != EPCGExMapMatchMode::Disabled)
		{
			// Build tagged data from operands for matching
			auto Matcher = MakeShared<PCGExMatching::FDataMatcher>();
			Matcher->SetDetails(&Settings->OperandsDataMatching);

			// Collect operand facades for matching
			bDoOperandMatching = Matcher->Init(Context, OperandsFacades, true, PCGExClipper2::Labels::SourceOperandsMatchRulesLabel);

			if (bDoOperandMatching)
			{
				PCGExMatching::FScope Scope(OperandsFacades.Num(), true);
				Context->OperandsOpDataPartitions.Reserve(Context->MainOpDataPartitions.Num());

				bool bMissingOperands = false;

				for (int i = 0; i < Context->MainOpDataPartitions.Num(); i++)
				{
					const TArray<int32>& MainPartition = Context->MainOpDataPartitions[i];
					TArray<int32>& Matches = Context->OperandsOpDataPartitions.Emplace_GetRef();
					for (const int32 MainIndex : MainPartition) { Matcher->GetMatchingSourcesIndices(MainFacades[MainIndex]->Source->GetTaggedData(), Scope, Matches); }

					if (Matches.IsEmpty())
					{
						// No matching operands for this group, remove it
						bMissingOperands = true;
						Context->OperandsOpDataPartitions.Pop();
						Context->MainOpDataPartitions.RemoveAt(i);
						i--;
					}
				}

				if (Context->OperandsOpDataPartitions.IsEmpty())
				{
					PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not match any data with any operand."));
					return false;
				}

				if (bMissingOperands)
				{
					PCGE_LOG(Warning, GraphAndLog, FTEXT("Some data could not be matched with any operands and will be ignored."));
				}
			}
			else
			{
				PCGE_LOG(Warning, GraphAndLog, FTEXT("Failed to initialize data matcher."));
			}
		}

		if (!bDoOperandMatching)
		{
			Context->MainOpDataPartitions.Reserve(OperandsFacades.Num());
			const int32 NumOperands = OperandsFacades.Num();

			for (int i = 0; i < NumOperands; i++)
			{
				// Fill all matching operand partitions
				PCGExArrayHelpers::ArrayOfIndices(Context->OperandsOpDataPartitions.Emplace_GetRef(), NumOperands);
			}
		}

		// Bump indices to match actual ordering within AllOpData
		for (TArray<int32>& Partition : Context->OperandsOpDataPartitions) { for (int32& Idx : Partition) { Idx += IndexOffset; } }
	}


	return true;
}

bool FPCGExClipper2ProcessorElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExClipper2BooleanElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(Clipper2Processor)

	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->SetState(PCGExCommon::States::State_Processing);
		PCGEX_ASYNC_GROUP_CHKD_RET(Context->GetTaskManager(), WorkTasks, true)

		TWeakPtr<FPCGContextHandle> WeakHandle = Context->GetOrCreateHandle();

		const int32 NumGroups = Context->MainOpDataPartitions.Num();
		if (Settings->NeedsOperands())
		{
			for (int i = 0; i < NumGroups; i++)
			{
				WorkTasks->AddSimpleCallback([WeakHandle, Index = i]
				{
					FPCGContext::FSharedContext<FPCGExClipper2ProcessorContext> SharedContext(WeakHandle);
					if (!SharedContext.Get()) { return; }

					SharedContext.Get()->Process(SharedContext.Get()->MainOpDataPartitions[Index], nullptr);
				});
			}
		}
		else
		{
			for (int i = 0; i < NumGroups; i++)
			{
				WorkTasks->AddSimpleCallback([WeakHandle, Index = i]
				{
					FPCGContext::FSharedContext<FPCGExClipper2ProcessorContext> SharedContext(WeakHandle);
					if (!SharedContext.Get()) { return; }

					SharedContext.Get()->Process(
						SharedContext.Get()->MainOpDataPartitions[Index],
						&SharedContext.Get()->OperandsOpDataPartitions[Index]);
				});
			}
		}

		WorkTasks->StartSimpleCallbacks();
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExCommon::States::State_Processing)
	{
		PCGEX_OUTPUT_VALID_PATHS(MainPoints)
		Context->Done();
	}

	return Context->TryComplete();
}

int32 FPCGExClipper2ProcessorElement::BuildDataFromCollection(
	FPCGExClipper2ProcessorContext* Context,
	const UPCGExClipper2ProcessorSettings* Settings,
	const TSharedPtr<PCGExData::FPointIOCollection>& Collection,
	TArray<TArray<int32>>& OutData) const
{
	if (!Collection) { return 0; }

	const int32 NumInputs = Collection->Num();
	if (NumInputs == 0) { return 0; }

	auto Data = MakeShared<TSharedPtr<PCGExClipper2::FOpData>>();

	// Thread-safe containers for parallel building
	FCriticalSection Lock;

	struct FBuildResult
	{
		bool bValid = false;
		PCGExClipper2Lib::Path64 Path64;
		TSharedPtr<PCGExData::FFacade> Facade;
		bool bIsClosedLoop = false;
		FPCGExGeo2DProjectionDetails Projection;
	};

	TArray<FBuildResult> Results;
	Results.SetNum(NumInputs);

	Context->AllOpData->AddReserve(NumInputs);

	{
		PCGExAsyncHelpers::FAsyncExecutionScope BuildScope(NumInputs);

		for (int32 i = 0; i < NumInputs; ++i)
		{
			BuildScope.Execute([&, IO = (*Collection)[i], i]()
			{
				FBuildResult& Result = Results[i];

				// Skip paths with insufficient points
				if (IO->GetNum() < 2) { return; }

				// Check if closed (required for boolean ops)
				const bool bIsClosed = PCGExPaths::Helpers::GetClosedLoop(IO->GetIn());
				if (!bIsClosed && Settings->bSkipOpenPaths) return;

				TSharedPtr<PCGExData::FFacade> Facade = MakeShared<PCGExData::FFacade>(IO.ToSharedRef());

				// Allocate unique path ID
				const int32 Idx = Context->AllocateSourceIdx();
				Facade->Idx = Idx;

				// Initialize projection for this path
				FPCGExGeo2DProjectionDetails LocalProjection = Context->ProjectionDetails;
				if (LocalProjection.Method == EPCGExProjectionMethod::Normal) { if (!LocalProjection.Init(Facade)) { return; } }
				else { LocalProjection.Init(PCGExMath::FBestFitPlane(IO->GetIn()->GetConstTransformValueRange())); }

				const int32 Scale = Settings->Precision;

				TConstPCGValueRange<FTransform> InTransforms = IO->GetIn()->GetConstTransformValueRange();

				// Build path with Z value mapped to source ID
				const int32 NumPoints = InTransforms.Num();
				Result.Path64.reserve(NumPoints);
				for (int32 j = 0; j < NumPoints; j++)
				{
					FVector Location = LocalProjection.Project(InTransforms[j].GetLocation(), j);
					Result.Path64.emplace_back(
						static_cast<int64_t>(Location.X * Scale),
						static_cast<int64_t>(Location.Y * Scale),
						static_cast<int64_t>(PCGEx::H64(j, Idx)) // retrieve using H64(const uint64 Hash, uint32& A, uint32& B)
					);
				}

				// Convert to polyline
				Result.bValid = true;
				Result.Facade = Facade;
				Result.Projection = LocalProjection;
				Result.bIsClosedLoop = PCGExPaths::Helpers::GetClosedLoop(IO);
			});
		}
	}

	int32 TotalDataNum = 0;
	TArray<int32>& OutIndices = OutData.Emplace_GetRef();
	OutIndices.Reserve(Results.Num());

	// Collect results (single-threaded)
	for (FBuildResult& Result : Results)
	{
		if (!Result.bValid) { continue; }

		OutIndices.Add(Result.Facade->Idx);

		Context->AllOpData->Facades->Add(Result.Facade);
		Context->AllOpData->Paths.Add(MoveTemp(Result.Path64));
		Context->AllOpData->Projections.Add(MoveTemp(Result.Projection));
		Context->AllOpData->IsClosedLoop.Add(Result.bIsClosedLoop);

		TotalDataNum++;
	}

	return TotalDataNum;
}

#undef LOCTEXT_NAMESPACE

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExClipper2Processor.h"

#include "Data/PCGBasePointData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "GeometryCollection/Facades/CollectionPositionTargetFacade.h"
#include "Helpers/PCGExAsyncHelpers.h"
#include "Math/PCGExBestFitPlane.h"
#include "Paths/PCGExPath.h"
#include "Paths/PCGExPathsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExClipper2ProcessorElement"

UPCGExClipper2ProcessorSettings::UPCGExClipper2ProcessorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPCGExClipper2ProcessorSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if (InPin->Properties.Label == PCGExCavalier::Labels::SourceOperandsLabel) { return NeedsOperands(); }
	return Super::IsPinUsedByNodeExecution(InPin);
}

TArray<FPCGPinProperties> UPCGExClipper2ProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	if (NeedsOperands()) { PCGEX_PIN_POINTS(PCGExCavalier::Labels::SourceOperandsLabel, "Operands", Required) }
	else { PCGEX_PIN_POINTS(PCGExCavalier::Labels::SourceOperandsLabel, "Operands", Advanced) }

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

TSharedPtr<PCGExData::FFacade> FPCGExClipper2ProcessorContext::FindSourceFacade(const PCGExCavalier::FPolyline& Polyline, const TMap<int32, PCGExCavalier::FRootPath>* RootPathsMapOverride) const
{
	const TSet<int32>& ContributingIds = Polyline.GetContributingPathIds();

	// Try to find a source IO from the contributing paths
	if (RootPathsMapOverride)
	{
		for (const int32 PathId : ContributingIds)
		{
			if (const PCGExCavalier::FRootPath* RootPath = RootPathsMapOverride->Find(PathId)) { return RootPath->PathFacade; }
		}
	}
	else
	{
		for (const int32 PathId : ContributingIds)
		{
			if (const PCGExCavalier::FRootPath* RootPath = RootPathsMap.Find(PathId)) { return RootPath->PathFacade; }
		}
	}


	// Fallback: use first main polyline source if available
	if (!MainFacades.IsEmpty() && MainFacades[0]) { return MainFacades[0]; }

	return nullptr;
}

TSharedPtr<PCGExData::FPointIO> FPCGExClipper2ProcessorContext::OutputPolyline(
	PCGExCavalier::FPolyline& Polyline, bool bIsNegativeSpace,
	const FPCGExGeo2DProjectionDetails& InProjectionDetails,
	const TMap<int32, PCGExCavalier::FRootPath>* RootPathsMapOverride) const
{
	const UPCGExClipper2ProcessorSettings* Settings = GetInputSettings<UPCGExClipper2ProcessorSettings>();
	check(Settings);

	return PathIO;
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
		Context->OperandsCollection = MakeShared<PCGExData::FPointIOCollection>(InContext, PCGExCavalier::Labels::SourceOperandsLabel, PCGExData::EIOInit::NoInit, false);

		if (Context->OperandsCollection->IsEmpty())
		{
			PCGEX_LOG_MISSING_INPUT(Context, FTEXT("Operands input is required for this operation mode."));
			return false;
		}
	}

	if (WantsRootPathsFromMainInput())
	{
		// Build polylines from main input (parallel)
		BuildRootPathsFromCollection(Context, Settings, Context->MainPoints, Context->MainPolylines, Context->MainFacades);

		if (Context->MainPolylines.IsEmpty())
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("No valid paths found in main input."));
			return false;
		}
	}

	if (Context->OperandsCollection)
	{
		// Build polylines from operands input (parallel)
		BuildRootPathsFromCollection(Context, Settings, Context->OperandsCollection, Context->OperandPolylines, Context->OperandsFacades);

		if (Context->OperandPolylines.IsEmpty())
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("No valid operands found in operands input."));
			return false;
		}
	}

	/*
	if (Settings->DataMatching.IsEnabled() &&
		Settings->DataMatching.Mode != EPCGExMapMatchMode::Disabled)
	{
		// Build tagged data from operands for matching
		Context->DataMatcher = MakeShared<PCGExMatching::FDataMatcher>();
		Context->DataMatcher->SetDetails(&Settings->DataMatching);

		// Collect operand facades for matching
		if (!Context->DataMatcher->Init(Context, Context->OperandsFacades, false))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Failed to initialize data matcher."));
			Context->DataMatcher.Reset();
		}
	}
	*/

	return true;
}

bool FPCGExClipper2ProcessorElement::WantsRootPathsFromMainInput() const
{
	return true;
}

void FPCGExClipper2ProcessorElement::BuildRootPathsFromCollection(
	FPCGExClipper2ProcessorContext* Context,
	const UPCGExClipper2ProcessorSettings* Settings,
	const TSharedPtr<PCGExData::FPointIOCollection>& Collection,
	TArray<PCGExCavalier::FPolyline>& OutPolylines,
	TArray<TSharedPtr<PCGExData::FFacade>>& OutSourceFacades) const
{
	if (!Collection) return;

	const int32 NumInputs = Collection->Num();
	if (NumInputs == 0) return;

	Context->RootPathsMap.Reserve(Context->RootPathsMap.Num() + NumInputs);
	OutPolylines.Reserve(OutPolylines.Num() + NumInputs);
	OutSourceFacades.Reserve(OutSourceFacades.Num() + NumInputs);

	// Thread-safe containers for parallel building
	FCriticalSection Lock;

	struct FBuildResult
	{
		PCGExCavalier::FRootPath RootPath;
		PCGExCavalier::FPolyline Polyline;
		TSharedPtr<PCGExData::FFacade> Facade;
		bool bValid = false;
	};

	TArray<FBuildResult> Results;
	Results.SetNum(NumInputs);

	{
		PCGExAsyncHelpers::FAsyncExecutionScope BuildScope(NumInputs);

		for (int32 i = 0; i < NumInputs; ++i)
		{
			BuildScope.Execute([&, IO = (*Collection)[i], i]()
			{
				FBuildResult& Result = Results[i];

				// Skip paths with insufficient points
				if (IO->GetNum() < 3) { return; }

				// Check if closed (required for boolean ops)
				const bool bIsClosed = PCGExPaths::Helpers::GetClosedLoop(IO->GetIn());
				if (!bIsClosed && Settings->bSkipOpenPaths) return;

				TSharedPtr<PCGExData::FFacade> Facade = MakeShared<PCGExData::FFacade>(IO.ToSharedRef());

				// Allocate unique path ID
				Facade->Idx = Context->AllocateSourceIdx();

				// Initialize projection for this path
				FPCGExGeo2DProjectionDetails LocalProjection = Context->ProjectionDetails;
				if (LocalProjection.Method == EPCGExProjectionMethod::Normal) { if (!LocalProjection.Init(Facade)) { return; } }
				else { LocalProjection.Init(PCGExMath::FBestFitPlane(IO->GetIn()->GetConstTransformValueRange())); }

				// Build root path
				Result.RootPath = PCGExCavalier::FRootPath(Facade->Idx, Facade, LocalProjection);

				// Convert to polyline
				Result.Polyline = PCGExCavalier::FContourUtils::CreateFromRootPath(Result.RootPath);
				Result.Polyline.SetClosed(bIsClosed);
				Result.Polyline.SetPrimaryPathId(Result.RootPath.PathId);
				Result.Facade = Facade;
				Result.bValid = true;
			});
		}
	}

	// Collect results (single-threaded)
	for (FBuildResult& Result : Results)
	{
		if (!Result.bValid) continue;

		const int32 PathId = Result.RootPath.PathId;
		Context->RootPathsMap.Add(PathId, MoveTemp(Result.RootPath));
		OutPolylines.Add(MoveTemp(Result.Polyline));
		OutSourceFacades.Add(Result.Facade);
	}
}

#undef LOCTEXT_NAMESPACE

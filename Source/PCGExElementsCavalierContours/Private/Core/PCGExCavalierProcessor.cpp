// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExCavalierProcessor.h"

#include "Core/PCGExCCCommon.h"
#include "Core/PCGExCCUtils.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "GeometryCollection/Facades/CollectionPositionTargetFacade.h"
#include "Helpers/PCGExAsyncHelpers.h"
#include "Math/PCGExBestFitPlane.h"
#include "Paths/PCGExPath.h"
#include "Paths/PCGExPathsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExCavalierProcessorElement"

UPCGExCavalierProcessorSettings::UPCGExCavalierProcessorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPCGExCavalierProcessorSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if (InPin->Properties.Label == PCGExCavalier::Labels::SourceOperandsLabel) { return NeedsOperands(); }
	return Super::IsPinUsedByNodeExecution(InPin);
}

TArray<FPCGPinProperties> UPCGExCavalierProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	if (NeedsOperands()) { PCGEX_PIN_POINTS(PCGExCavalier::Labels::SourceOperandsLabel, "Operands", Required) }
	else { PCGEX_PIN_POINTS(PCGExCavalier::Labels::SourceOperandsLabel, "Operands", Advanced) }

	return PinProperties;
}

bool UPCGExCavalierProcessorSettings::NeedsOperands() const
{
	return false;
}

FPCGExGeo2DProjectionDetails UPCGExCavalierProcessorSettings::GetProjectionDetails() const
{
	return FPCGExGeo2DProjectionDetails();
}

TSharedPtr<PCGExData::FFacade> FPCGExCavalierProcessorContext::FindSourceFacade(const PCGExCavalier::FPolyline& Polyline, const TMap<int32, PCGExCavalier::FRootPath>* RootPathsMapOverride) const
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

TSharedPtr<PCGExData::FPointIO> FPCGExCavalierProcessorContext::OutputPolyline(
	PCGExCavalier::FPolyline& Polyline, bool bIsNegativeSpace,
	const FPCGExGeo2DProjectionDetails& InProjectionDetails,
	const TMap<int32, PCGExCavalier::FRootPath>* RootPathsMapOverride) const
{
	const UPCGExCavalierProcessorSettings* Settings = GetInputSettings<UPCGExCavalierProcessorSettings>();
	check(Settings);

	if (!Settings->bOutputNegativeSpace && bIsNegativeSpace) { return nullptr; }
	if (Settings->bTessellateArcs) { Polyline = Polyline.Tessellated(Settings->ArcTessellationSettings); }

	const int32 NumVertices = Polyline.VertexCount();
	if (NumVertices < 3) { return nullptr; }

	// Find a source IO for metadata inheritance
	TSharedPtr<PCGExData::FFacade> SourceFacade = FindSourceFacade(Polyline, RootPathsMapOverride);
	if (!SourceFacade) { return nullptr; }

	// Create output
	const TSharedPtr<PCGExData::FPointIO> PathIO = MainPoints->Emplace_GetRef(SourceFacade->Source, PCGExData::EIOInit::New);
	if (!PathIO) { return nullptr; }

	// Convert back to 3D using source tracking	
	//PCGExCavalier::Utils::InterpolateMissingSources(Polyline);
	PCGExCavalier::FContourResult3D Result3D = PCGExCavalier::FContourUtils::ConvertTo3D(Polyline, RootPathsMapOverride ? *RootPathsMapOverride : RootPathsMap, Settings->bBlendTransforms);

	EPCGPointNativeProperties Allocations = SourceFacade->GetAllocations() | EPCGPointNativeProperties::Transform;
	PCGExPointArrayDataHelpers::SetNumPointsAllocated(PathIO->GetOut(), NumVertices, Allocations);
	//TArray<int32>& IdxMapping = PathIO->GetIdxMapping(NumVertices);

	int32 SafeIndex = 0;
	TPCGValueRange<FTransform> OutTransforms = PathIO->GetOut()->GetTransformValueRange();
	for (int32 i = 0; i < NumVertices; ++i)
	{
		const int32 SourceIndex = Result3D.GetPointIndex(i);
		if (SourceIndex != INDEX_NONE) { SafeIndex = SourceIndex; }

		//IdxMapping[i] = FMath::Max(0, SafeIndex); // Ensure valid index for mapping

		// Full transform with proper Z, rotation, and scale from source
		OutTransforms[i] = InProjectionDetails.Restore(Result3D.Transforms[i], SourceIndex);
	}

	EnumRemoveFlags(Allocations, EPCGPointNativeProperties::Transform);
	//PathIO->ConsumeIdxMapping(Allocations);

	PCGExPaths::Helpers::SetClosedLoop(PathIO, Polyline.IsClosed());

	// Tag negative space outputs
	if (bIsNegativeSpace) { PathIO->Tags->AddRaw(Settings->NegativeSpaceTag); }

	return PathIO;
}

bool FPCGExCavalierProcessorElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CavalierProcessor)

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

bool FPCGExCavalierProcessorElement::WantsRootPathsFromMainInput() const
{
	return true;
}

void FPCGExCavalierProcessorElement::BuildRootPathsFromCollection(
	FPCGExCavalierProcessorContext* Context,
	const UPCGExCavalierProcessorSettings* Settings,
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

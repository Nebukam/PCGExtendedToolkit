// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathSplineMesh.h"

#include "PCGExHelpers.h"
#include "PCGExManagedResource.h"
#include "AssetSelectors/PCGExInternalCollection.h"
#include "Paths/PCGExPaths.h"
#include "Paths/Tangents/PCGExZeroTangents.h"

#define LOCTEXT_NAMESPACE "PCGExPathSplineMeshElement"
#define PCGEX_NAMESPACE BuildCustomGraph

PCGEX_INITIALIZE_ELEMENT(PathSplineMesh)

TArray<FPCGPinProperties> UPCGExPathSplineMeshSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	if (CollectionSource == EPCGExCollectionSource::AttributeSet)
	{
		PCGEX_PIN_PARAM(PCGExAssetCollection::SourceAssetCollection, "Attribute set to be used as collection.", Required, {})
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExPathSplineMeshSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	// No output
	return PinProperties;
}

PCGExData::EInit UPCGExPathSplineMeshSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

FPCGExPathSplineMeshContext::~FPCGExPathSplineMeshContext()
{
	PCGEX_TERMINATE_ASYNC
}

void FPCGExPathSplineMeshContext::RegisterAssetDependencies()
{
	FPCGExPathProcessorContext::RegisterAssetDependencies();

	PCGEX_SETTINGS_LOCAL(PathSplineMesh)

	if (Settings->CollectionSource == EPCGExCollectionSource::Asset)
	{
		MainCollection = Settings->AssetCollection.LoadSynchronous();
	}
	else
	{
		// Only load assets for internal collections
		// since we need them for staging
		MainCollection = GetDefault<UPCGExInternalCollection>()->GetCollectionFromAttributeSet(
			this, PCGExAssetCollection::SourceAssetCollection,
			Settings->AttributeSetDetails, false);
	}

	if (MainCollection) { MainCollection->GetAssetPaths(RequiredAssets, PCGExAssetCollection::ELoadingFlags::Recursive); }
}

bool FPCGExPathSplineMeshElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathSplineMesh)

	if (Settings->bApplyCustomTangents)
	{
		PCGEX_VALIDATE_NAME(Settings->ArriveTangentAttribute)
		PCGEX_VALIDATE_NAME(Settings->LeaveTangentAttribute)
	}

	if (Settings->CollectionSource == EPCGExCollectionSource::Asset)
	{
		Context->MainCollection = Settings->AssetCollection.LoadSynchronous();
	}
	else
	{
		if (Context->MainCollection)
		{
			// Internal collection, assets have been loaded at this point
			Context->MainCollection->RebuildStagingData(true);
		}
	}

	if (!Context->MainCollection)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing asset collection."));
		return false;
	}

	Context->MainCollection->LoadCache(); // Make sure to load the stuff

	return true;
}

bool FPCGExPathSplineMeshElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathSplineMeshElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathSplineMesh)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPathSplineMesh::FProcessor>>(
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
			[&](PCGExPointsMT::TBatch<PCGExPathSplineMesh::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Could not find any paths to write tangents to."));
			return true;
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainBatch->Output();

	// Execute PostProcess Functions
	if (!Context->NotifyActors.IsEmpty())
	{
		TArray<AActor*> NotifyActors = Context->NotifyActors.Array();
		for (AActor* TargetActor : NotifyActors)
		{
			for (UFunction* Function : PCGExHelpers::FindUserFunctions(TargetActor->GetClass(), Settings->PostProcessFunctionNames, {UPCGExFunctionPrototypes::GetPrototypeWithNoParams()}, Context))
			{
				TargetActor->ProcessEvent(Function, nullptr);
			}
		}
	}

	return Context->TryComplete();
}

namespace PCGExPathSplineMesh
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(Helper)
		//for (USplineMeshComponent* SMC : SplineMeshComponents) { PCGEX_DELETE_UOBJECT(SMC) }
		//SplineMeshComponents.Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathSplineMesh)

		// Must be set before process for filters
		PointDataFacade->bSupportsDynamic = true;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalSettings = Settings;
		LocalTypedContext = TypedContext;

		bClosedPath = Settings->bClosedPath;

		Helper = new PCGExAssetCollection::FDistributionHelper(LocalTypedContext->MainCollection, Settings->DistributionSettings);
		if (!Helper->Init(Context, PointDataFacade)) { return false; }

		if (Settings->bApplyCustomTangents)
		{
			ArriveReader = PointDataFacade->GetReader<FVector>(Settings->ArriveTangentAttribute);
			if (!ArriveReader)
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Could not fetch tangent' Arrive attribute on some inputs."));
				return false;
			}

			LeaveReader = PointDataFacade->GetReader<FVector>(Settings->LeaveTangentAttribute);
			if (!ArriveReader)
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Could not fetch tangent' Leave attribute on some inputs."));
				return false;
			}
		}

		LastIndex = PointIO->GetNum() - 1;

		PCGEX_SET_NUM_UNINITIALIZED(Segments, bClosedPath ? LastIndex + 1 : LastIndex)
		//PCGEX_SET_NUM_UNINITIALIZED(SplineMeshComponents, LastIndex)

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		if (Index == LastIndex && !bClosedPath) { return; } // Ignore last index, only used for maths reasons

		const FPCGExAssetStagingData* StagingData = nullptr;

		const int32 Seed = PCGExRandom::GetSeedFromPoint(
			Helper->Details.SeedComponents, Point,
			Helper->Details.LocalSeed, LocalSettings, LocalTypedContext->SourceComponent.Get());

		Helper->GetStaging(StagingData, Index, Seed);

		Segments[Index] = PCGExPaths::FSplineMeshSegment();
		PCGExPaths::FSplineMeshSegment& Segment = Segments[Index];
		Segment.AssetStaging = StagingData;

		if (!StagingData) { return; }

		const int32 NextIndex = Index + 1 > LastIndex ? 0 : Index + 1;

		const FPCGPoint& NextPoint = PointIO->GetInPoint(NextIndex);

		FVector Scale = Point.Transform.GetScale3D();
		Segment.Params.StartPos = Point.Transform.GetLocation();
		Segment.Params.StartScale = FVector2D(Scale.Y, Scale.Z);
		Segment.Params.StartRoll = Point.Transform.GetRotation().Rotator().Roll;

		Scale = NextPoint.Transform.GetScale3D();
		Segment.Params.EndPos = NextPoint.Transform.GetLocation();
		Segment.Params.EndScale = FVector2D(Scale.Y, Scale.Z);
		Segment.Params.EndRoll = NextPoint.Transform.GetRotation().Rotator().Roll;

		if (LocalSettings->bApplyCustomTangents)
		{
			Segment.Params.StartTangent = LeaveReader->Values[Index];
			Segment.Params.EndTangent = ArriveReader->Values[NextIndex];
		}
	}

	void FProcessor::CompleteWork()
	{
	}

	void FProcessor::Output()
	{
		// TODO : Resolve per-point target actor...? irk.
		AActor* TargetActor = LocalSettings->TargetActor.Get() ? LocalSettings->TargetActor.Get() : Context->GetTargetActor(nullptr);

		if (!TargetActor)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Invalid target actor."));
			return;
		}

		UPCGComponent* Comp = Context->SourceComponent.Get();

		for (const PCGExPaths::FSplineMeshSegment& Segment : Segments)
		{
			if (!Segment.AssetStaging) { continue; }

			USplineMeshComponent* SMC = UPCGExManagedSplineMeshComponent::CreateComponentOnly(TargetActor, Context->SourceComponent.Get(), Segment);
			if (!SMC) { continue; }

			if (!Segment.ApplyMesh(SMC))
			{
				PCGEX_DELETE_UOBJECT(SMC)
				continue;
			}

			SMC->ClearInternalFlags(EInternalObjectFlags::Async);
			UPCGExManagedSplineMeshComponent::RegisterAndAttachComponent(TargetActor, SMC, Comp, LocalSettings->UID);
			LocalTypedContext->NotifyActors.Add(TargetActor);
		}

		/*
		for (int i = 0; i < SplineMeshComponents.Num(); i++)
		{
			USplineMeshComponent* SMC = SplineMeshComponents[i];
			if (!SMC) { continue; }

			const PCGExPaths::FSplineMeshSegment& Params = Segments[i];

			if (!Params.ApplyMesh(SMC))
			{
				SplineMeshComponents[i] = nullptr;
				PCGEX_DELETE_UOBJECT(SMC)
			}

			SMC->ClearInternalFlags(EInternalObjectFlags::Async);
			UPCGExManagedSplineMeshComponent::RegisterAndAttachComponent(TargetActor, SMC, Comp, LocalSettings->UID);
			LocalTypedContext->NotifyActors.Add(TargetActor);
		}

		SplineMeshComponents.Empty();
		*/
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

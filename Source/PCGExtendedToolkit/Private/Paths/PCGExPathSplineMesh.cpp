// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathSplineMesh.h"

#include "PCGExHelpers.h"
#include "Paths/PCGExPaths.h"

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

PCGExData::EInit UPCGExPathSplineMeshSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

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
		if (!Context->MainCollection)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Missing asset collection."));
			return false;
		}
	}
	else
	{
		Context->MainCollection = Cast<UPCGExMeshCollection>(Settings->AttributeSetDetails.TryBuildCollection(Context, PCGExAssetCollection::SourceAssetCollection, false));
		if (!Context->MainCollection)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to build collection from attribute set."));
			return false;
		}
	}


	PCGEX_VALIDATE_NAME(Settings->AssetPathAttributeName)

	if (Settings->WeightToAttribute == EPCGExWeightOutputMode::Raw || Settings->WeightToAttribute == EPCGExWeightOutputMode::Normalized)
	{
		PCGEX_VALIDATE_NAME(Settings->WeightAttributeName)
	}

	return true;
}

void FPCGExPathSplineMeshContext::RegisterAssetDependencies()
{
	FPCGExPathProcessorContext::RegisterAssetDependencies();

	PCGEX_SETTINGS_LOCAL(PathSplineMesh)

	MainCollection->GetAssetPaths(RequiredAssets, PCGExAssetCollection::ELoadingFlags::Recursive);
}

void FPCGExPathSplineMeshElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	PCGEX_CONTEXT_AND_SETTINGS(PathSplineMesh)
	if (Settings->CollectionSource == EPCGExCollectionSource::AttributeSet)
	{
		// Internal collection, assets have been loaded at this point, rebuilding stage data
		Context->MainCollection->RebuildStagingData(true);
	}

	FPCGExPathProcessorElement::PostLoadAssetsDependencies(InContext);
}

bool FPCGExPathSplineMeshElement::PostBoot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::PostBoot(InContext)) { return false; }

	PCGEX_CONTEXT(PathSplineMesh)

	Context->MainCollection->LoadCache(); // Make sure to load the stuff
	return true;
}

bool FPCGExPathSplineMeshElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathSplineMeshElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathSplineMesh)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		bool bInvalidInputs = false;

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPathSplineMesh::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bInvalidInputs = true;
					Entry->InitializeOutput(Context, PCGExData::EInit::Forward);
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExPathSplineMesh::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to write tangents to."));
		}

		if (bInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 2 points and won't be processed."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

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

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExPathSplineMesh
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

#if PCGEX_ENGINE_VERSION > 503
		bIsPreviewMode = ExecutionContext->SourceComponent.Get()->IsInPreviewMode();
#endif

		Justification = Settings->Justification;
		Justification.Init(ExecutionContext, PointDataFacade);

		bClosedLoop = Context->ClosedLoop.IsClosedLoop(PointDataFacade->Source);
		bApplyScaleToFit = Settings->ScaleToFit.ScaleToFitMode != EPCGExFitMode::None;
		bUseTags = Settings->TaggingDetails.IsEnabled();

		Helper = MakeUnique<PCGExAssetCollection::TDistributionHelper<UPCGExMeshCollection, FPCGExMeshCollectionEntry>>(Context->MainCollection, Settings->DistributionSettings);
		if (!Helper->Init(ExecutionContext, PointDataFacade)) { return false; }

		if (Settings->bApplyCustomTangents)
		{
			ArriveReader = PointDataFacade->GetReadable<FVector>(Settings->ArriveTangentAttribute);
			if (!ArriveReader)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Could not fetch tangent' Arrive attribute on some inputs."));
				return false;
			}

			LeaveReader = PointDataFacade->GetReadable<FVector>(Settings->LeaveTangentAttribute);
			if (!ArriveReader)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Could not fetch tangent' Leave attribute on some inputs."));
				return false;
			}
		}

		LastIndex = PointDataFacade->GetNum() - 1;

		PCGEx::InitArray(Segments, bClosedLoop ? LastIndex + 1 : LastIndex);

		switch (Settings->SplineMeshAxisConstant)
		{
		default:
		case EPCGExMinimalAxis::None:
		case EPCGExMinimalAxis::X:
			SplineMeshAxisConstant = ESplineMeshAxis::X;
			break;
		case EPCGExMinimalAxis::Y:
			C1 = 0;
			C2 = 2;
			SplineMeshAxisConstant = ESplineMeshAxis::Y;
			break;
		case EPCGExMinimalAxis::Z:
			C1 = 1;
			C2 = 0;
			SplineMeshAxisConstant = ESplineMeshAxis::Z;
			break;
		}

		bOutputWeight = Settings->WeightToAttribute != EPCGExWeightOutputMode::NoOutput;
		bNormalizedWeight = Settings->WeightToAttribute != EPCGExWeightOutputMode::Raw;
		bOneMinusWeight = Settings->WeightToAttribute == EPCGExWeightOutputMode::NormalizedInverted || Settings->WeightToAttribute == EPCGExWeightOutputMode::NormalizedInvertedToDensity;

		if (Settings->WeightToAttribute == EPCGExWeightOutputMode::Raw)
		{
			WeightWriter = PointDataFacade->GetWritable<int32>(Settings->WeightAttributeName, true);
		}
		else if (Settings->WeightToAttribute == EPCGExWeightOutputMode::Normalized)
		{
			NormalizedWeightWriter = PointDataFacade->GetWritable<double>(Settings->WeightAttributeName, true);
		}

#if PCGEX_ENGINE_VERSION > 503
		PathWriter = PointDataFacade->GetWritable<FSoftObjectPath>(Settings->AssetPathAttributeName, true);
#else
		PathWriter = PointDataFacade->GetWritable<FString>(Settings->AssetPathAttributeName, true);
#endif

		DataTags = PointDataFacade->Source->Tags->ToFNameList();

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
		FilterScope(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		auto InvalidPoint = [&]()
		{
#if PCGEX_ENGINE_VERSION > 503
			PathWriter->GetMutable(Index) = FSoftObjectPath{};
#else
			PathWriter->GetMutable(Index) = TEXT("");
#endif

			if (bOutputWeight)
			{
				if (WeightWriter) { WeightWriter->GetMutable(Index) = -1; }
				else if (NormalizedWeightWriter) { NormalizedWeightWriter->GetMutable(Index) = -1; }
			}
		};

		if (Index == LastIndex && !bClosedLoop)
		{
			// Ignore last index, only used for maths reasons
			InvalidPoint();
			return;
		}

		if (!PointFilterCache[Index])
		{
			Segments[Index] = PCGExPaths::FSplineMeshSegment();
			InvalidPoint();
			return;
		}

		const FPCGExMeshCollectionEntry* MeshEntry = nullptr;

		const int32 Seed = PCGExRandom::GetSeedFromPoint(
			Helper->Details.SeedComponents, Point,
			Helper->Details.LocalSeed, Settings, Context->SourceComponent.Get());


		Segments[Index] = PCGExPaths::FSplineMeshSegment();
		PCGExPaths::FSplineMeshSegment& Segment = Segments[Index];

		if (bUseTags) { Helper->GetEntry(MeshEntry, Index, Seed, Settings->TaggingDetails.GrabTags, Segment.Tags); }
		else { Helper->GetEntry(MeshEntry, Index, Seed); }

		Segment.MeshEntry = MeshEntry;

		if (!MeshEntry)
		{
			InvalidPoint();
			return;
		}

		//

		if (bOutputWeight)
		{
			double Weight = bNormalizedWeight ? static_cast<double>(MeshEntry->Weight) / static_cast<double>(Context->MainCollection->LoadCache()->WeightSum) : MeshEntry->Weight;
			if (bOneMinusWeight) { Weight = 1 - Weight; }
			if (WeightWriter) { WeightWriter->GetMutable(Index) = Weight; }
			else if (NormalizedWeightWriter) { NormalizedWeightWriter->GetMutable(Index) = Weight; }
			else { Point.Density = Weight; }
		}

#if PCGEX_ENGINE_VERSION > 503
		PathWriter->GetMutable(Index) = MeshEntry->Staging.Path;
#else
		PathWriter->GetMutable(Index) = MeshEntry->Staging.Path.ToString();
#endif

		//

		Segment.SplineMeshAxis = SplineMeshAxisConstant;

		const int32 NextIndex = Index + 1 > LastIndex ? 0 : Index + 1;
		const FPCGPoint& NextPoint = PointDataFacade->Source->GetInPoint(NextIndex);

		//

		const FBox& StBox = MeshEntry->Staging.Bounds;
		FVector OutScale = Point.Transform.GetScale3D();
		const FBox InBounds = FBox(Point.BoundsMin * OutScale, Point.BoundsMax * OutScale);
		FBox OutBounds = StBox;

		Settings->ScaleToFit.Process(Point, MeshEntry->Staging.Bounds, OutScale, OutBounds);

		FVector OutTranslation = FVector::ZeroVector;
		OutBounds = FBox(OutBounds.Min * OutScale, OutBounds.Max * OutScale);

		Justification.Process(Index, InBounds, OutBounds, OutTranslation);

		//

		Segment.Params.StartPos = Point.Transform.GetLocation();
		Segment.Params.StartScale = FVector2D(OutScale[C1], OutScale[C2]);
		Segment.Params.StartRoll = Point.Transform.GetRotation().Rotator().Roll;

		const FVector Scale = bApplyScaleToFit ? OutScale : NextPoint.Transform.GetScale3D();
		Segment.Params.EndPos = NextPoint.Transform.GetLocation();
		Segment.Params.EndScale = FVector2D(Scale[C1], Scale[C2]);
		Segment.Params.EndRoll = NextPoint.Transform.GetRotation().Rotator().Roll;

		Segment.Params.StartOffset = FVector2D(OutTranslation[C1], OutTranslation[C2]);
		Segment.Params.EndOffset = FVector2D(OutTranslation[C1], OutTranslation[C2]);

		if (Settings->bApplyCustomTangents)
		{
			Segment.Params.StartTangent = LeaveReader->Read(Index);
			Segment.Params.EndTangent = ArriveReader->Read(NextIndex);
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}

	void FProcessor::Output()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExPathSplineMesh::FProcessor::Output);

		// TODO : Resolve per-point target actor...? irk.
		AActor* TargetActor = Settings->TargetActor.Get() ? Settings->TargetActor.Get() : ExecutionContext->GetTargetActor(nullptr);

		if (!TargetActor)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Invalid target actor."));
			return;
		}

		for (int i = 0; i < Segments.Num(); i++)
		{
			const PCGExPaths::FSplineMeshSegment& Segment = Segments[i];
			if (!Segment.MeshEntry) { continue; }

			const FString ComponentName = TEXT("PCGSplineMeshComponent_") + Segment.MeshEntry->Staging.Path.GetAssetName();
			const EObjectFlags ObjectFlags = (bIsPreviewMode ? RF_Transient : RF_NoFlags);
			USplineMeshComponent* SplineMeshComponent = NewObject<USplineMeshComponent>(TargetActor, MakeUniqueObjectName(TargetActor, USplineMeshComponent::StaticClass(), FName(ComponentName)), ObjectFlags);

			SplineMeshComponent->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
			SplineMeshComponent->SetMobility(EComponentMobility::Static);
			SplineMeshComponent->SetSimulatePhysics(false);
			SplineMeshComponent->SetMassOverrideInKg(NAME_None, 0.0f);
			SplineMeshComponent->SetUseCCD(false);
			SplineMeshComponent->CanCharacterStepUpOn = ECB_No;
			SplineMeshComponent->bUseDefaultCollision = false;
			SplineMeshComponent->bNavigationRelevant = false;
			SplineMeshComponent->SetbNeverNeedsCookedCollisionData(true);

			Segment.ApplySettings(SplineMeshComponent); // Init Component

			if (!Segment.ApplyMesh(SplineMeshComponent))
			{
				SplineMeshComponent->MarkAsGarbage();
				continue;
			}

			if (Settings->TaggingDetails.bForwardInputDataTags) { SplineMeshComponent->ComponentTags.Append(DataTags); }
			if (!Segment.Tags.IsEmpty()) { SplineMeshComponent->ComponentTags.Append(Segment.Tags.Array()); }

			if (Settings->bForceDefaultDescriptor) { Settings->DefaultDescriptor.InitComponent(SplineMeshComponent); }
			else { Segment.MeshEntry->SMDescriptor.InitComponent(SplineMeshComponent); }

			Context->AttachManageComponent(
				TargetActor, SplineMeshComponent,
				FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));

			Context->NotifyActors.Add(TargetActor);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

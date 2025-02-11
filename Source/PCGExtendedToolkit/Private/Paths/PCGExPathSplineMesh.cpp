// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathSplineMesh.h"

#include "PCGExHelpers.h"
#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExPathSplineMeshElement"
#define PCGEX_NAMESPACE BuildCustomGraph

PCGEX_INITIALIZE_ELEMENT(PathSplineMesh)

UPCGExPathSplineMeshSettings::UPCGExPathSplineMeshSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (SplineMeshUpVectorAttribute.GetName() == FName("@Last")) { SplineMeshUpVectorAttribute.Update(TEXT("$Rotation.Up")); }
}

TArray<FPCGPinProperties> UPCGExPathSplineMeshSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	if (CollectionSource == EPCGExCollectionSource::AttributeSet)
	{
		PCGEX_PIN_PARAM(PCGExAssetCollection::SourceAssetCollection, "Attribute set to be used as collection.", Required, {})
	}

	return PinProperties;
}

PCGExData::EIOInit UPCGExPathSplineMeshSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

bool FPCGExPathSplineMeshElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathSplineMesh)

	if (Settings->bApplyCustomTangents)
	{
		PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->ArriveTangentAttribute)
		PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->LeaveTangentAttribute)
	}

	if (Settings->CollectionSource == EPCGExCollectionSource::Asset)
	{
		Context->MainCollection = PCGExHelpers::LoadBlocking_AnyThread(Settings->AssetCollection);
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

	PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->AssetPathAttributeName)

	if (Settings->WeightToAttribute == EPCGExWeightOutputMode::Raw ||
		Settings->WeightToAttribute == EPCGExWeightOutputMode::Normalized)
	{
		PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->WeightAttributeName)
	}

	return true;
}

void FPCGExPathSplineMeshContext::RegisterAssetDependencies()
{
	FPCGExPathProcessorContext::RegisterAssetDependencies();

	PCGEX_SETTINGS_LOCAL(PathSplineMesh)

	MainCollection->GetAssetPaths(GetRequiredAssets(), PCGExAssetCollection::ELoadingFlags::Recursive);
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
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPathSplineMesh::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bHasInvalidInputs = true;
					Entry->InitializeOutput(PCGExData::EIOInit::Forward);
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
			ArriveGetter = PointDataFacade->GetReadable<FVector>(Settings->ArriveTangentAttribute);
			if (!ArriveGetter)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Could not fetch tangent' Arrive attribute on some inputs."));
				return false;
			}

			LeaveGetter = PointDataFacade->GetReadable<FVector>(Settings->LeaveTangentAttribute);
			if (!ArriveGetter)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Could not fetch tangent' Leave attribute on some inputs."));
				return false;
			}
		}

		if (Settings->SplineMeshUpMode == EPCGExSplineMeshUpMode::Attribute)
		{
			UpGetter = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->SplineMeshUpVectorAttribute);

			if (!UpGetter)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Mesh Up Vector attribute is missing on some inputs."));
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
			WeightWriter = PointDataFacade->GetWritable<int32>(Settings->WeightAttributeName, PCGExData::EBufferInit::New);
		}
		else if (Settings->WeightToAttribute == EPCGExWeightOutputMode::Normalized)
		{
			NormalizedWeightWriter = PointDataFacade->GetWritable<double>(Settings->WeightAttributeName, PCGExData::EBufferInit::New);
		}

#if PCGEX_ENGINE_VERSION > 503
		PathWriter = PointDataFacade->GetWritable<FSoftObjectPath>(Settings->AssetPathAttributeName, PCGExData::EBufferInit::New);
#else
		PathWriter = PointDataFacade->GetWritable<FString>(Settings->AssetPathAttributeName, PCGExData::EBufferInit::New);
#endif

		DataTags = PointDataFacade->Source->Tags->FlattenToArrayOfNames();

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope)
	{
		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
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
		const UPCGExAssetCollection* EntryHost = nullptr;

		const int32 Seed = PCGExRandom::GetSeedFromPoint(
			Helper->Details.SeedComponents, Point,
			Helper->Details.LocalSeed, Settings, Context->SourceComponent.Get());


		Segments[Index] = PCGExPaths::FSplineMeshSegment();
		PCGExPaths::FSplineMeshSegment& Segment = Segments[Index];

		if (bUseTags) { Helper->GetEntry(MeshEntry, Index, Seed, Settings->TaggingDetails.GrabTags, Segment.Tags, EntryHost); }
		else { Helper->GetEntry(MeshEntry, Index, Seed, EntryHost); }

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
			Segment.Params.StartTangent = LeaveGetter->Read(Index);
			Segment.Params.EndTangent = ArriveGetter->Read(NextIndex);
		}

		if (UpGetter) { Segment.UpVector = UpGetter->Read(Index); }
		else if (Settings->SplineMeshUpMode == EPCGExSplineMeshUpMode::Constant) { Segment.UpVector = Settings->SplineMeshUpVector; }
		else { Segment.ComputeUpVectorFromTangents(); }
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

			const EObjectFlags ObjectFlags = (bIsPreviewMode ? RF_Transient : RF_NoFlags);
			USplineMeshComponent* SplineMeshComponent = NewObject<USplineMeshComponent>(
				TargetActor, MakeUniqueObjectName(
					TargetActor, USplineMeshComponent::StaticClass(),
					Context->UniqueNameGenerator->Get(TEXT("PCGSplineMeshComponent_") + Segment.MeshEntry->Staging.Path.GetAssetName())), ObjectFlags);

			/*
			SplineMeshComponent->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
			SplineMeshComponent->SetMobility(EComponentMobility::Static);
			SplineMeshComponent->SetSimulatePhysics(false);
			SplineMeshComponent->SetMassOverrideInKg(NAME_None, 0.0f);
			SplineMeshComponent->SetUseCCD(false);
			SplineMeshComponent->CanCharacterStepUpOn = ECB_No;
			SplineMeshComponent->bUseDefaultCollision = false;
			SplineMeshComponent->bNavigationRelevant = false;
			SplineMeshComponent->SetbNeverNeedsCookedCollisionData(true);
			*/

			Segment.ApplySettings(SplineMeshComponent); // Init Component

			if (Settings->bForceDefaultDescriptor || Settings->CollectionSource == EPCGExCollectionSource::AttributeSet) { Settings->DefaultDescriptor.InitComponent(SplineMeshComponent); }
			else { Segment.MeshEntry->SMDescriptor.InitComponent(SplineMeshComponent); }

			if (!Segment.ApplyMesh(SplineMeshComponent))
			{
				SplineMeshComponent->MarkAsGarbage();
				continue;
			}

			if (Settings->TaggingDetails.bForwardInputDataTags) { SplineMeshComponent->ComponentTags.Append(DataTags); }
			if (!Segment.Tags.IsEmpty()) { SplineMeshComponent->ComponentTags.Append(Segment.Tags.Array()); }

			Context->AttachManagedComponent(
				TargetActor, SplineMeshComponent,
				FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));

			Context->NotifyActors.Add(TargetActor);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

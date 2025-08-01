// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathSplineMesh.h"

#include "PCGComponent.h"
#include "Components/SplineMeshComponent.h"

#include "PCGExHelpers.h"
#include "PCGExRandom.h"


#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExPathSplineMeshElement"
#define PCGEX_NAMESPACE BuildCustomGraph

#if WITH_EDITOR
void UPCGExPathSplineMeshSettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	if (SplineMeshAxisConstant_DEPRECATED != EPCGExMinimalAxis::None && DefaultDescriptor.SplineMeshAxis == EPCGExSplineMeshAxis::Default)
	{
		DefaultDescriptor.SplineMeshAxis = static_cast<EPCGExSplineMeshAxis>(SplineMeshAxisConstant_DEPRECATED);
	}

	Tangents.ApplyDeprecation(bApplyCustomTangents_DEPRECATED, ArriveTangentAttribute_DEPRECATED, LeaveTangentAttribute_DEPRECATED);

	Super::ApplyDeprecation(InOutNode);
}
#endif

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

bool FPCGExPathSplineMeshElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathSplineMesh)

	if (!Context->Tangents.Init(Context, Settings->Tangents)) { return false; }

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

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainBatch->Output();

	Context->MainPoints->StageOutputs();
	Context->ExecuteOnNotifyActors(Settings->PostProcessFunctionNames);

	return Context->TryComplete();
}

namespace PCGExPathSplineMesh
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		bIsPreviewMode = ExecutionContext->GetComponent()->IsInPreviewMode();

		Justification = Settings->Justification;
		Justification.Init(ExecutionContext, PointDataFacade);

		SegmentMutationDetails = Settings->MutationDetails;
		if (!SegmentMutationDetails.Init(PointDataFacade)) { return false; }

		bClosedLoop = PCGExPaths::GetClosedLoop(PointDataFacade->GetIn());
		bApplyScaleToFit = Settings->ScaleToFit.ScaleToFitMode != EPCGExFitMode::None;
		bUseTags = Settings->TaggingDetails.IsEnabled();

		TangentsHandler = MakeShared<PCGExTangents::FTangentsHandler>(bClosedLoop);
		if (!TangentsHandler->Init(Context, Context->Tangents, PointDataFacade)) { return false; }

		Helper = MakeUnique<PCGExAssetCollection::TDistributionHelper<UPCGExMeshCollection, FPCGExMeshCollectionEntry>>(Context->MainCollection, Settings->DistributionSettings);
		if (!Helper->Init(ExecutionContext, PointDataFacade)) { return false; }

		if (Settings->SplineMeshUpMode == EPCGExSplineMeshUpMode::Attribute)
		{
			UpGetter = PointDataFacade->GetBroadcaster<FVector>(Settings->SplineMeshUpVectorAttribute, true);

			if (!UpGetter)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Mesh Up Vector attribute is missing on some inputs."));
				return false;
			}
		}

		LastIndex = PointDataFacade->GetNum() - 1;

		PCGEx::InitArray(Segments, bClosedLoop ? LastIndex + 1 : LastIndex);

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

		PathWriter = PointDataFacade->GetWritable<FSoftObjectPath>(Settings->AssetPathAttributeName, PCGExData::EBufferInit::New);
		DataTags = PointDataFacade->Source->Tags->FlattenToArrayOfNames();

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
		ScopedMaterials = MakeShared<PCGExMT::TScopedSet<FSoftObjectPath>>(Loops, 0);
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::PathSplineMesh::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		const UPCGBasePointData* InPointData = PointDataFacade->GetIn();

		TConstPCGValueRange<int32> Seeds = InPointData->GetConstSeedValueRange();
		TConstPCGValueRange<FTransform> Transforms = InPointData->GetConstTransformValueRange();
		TConstPCGValueRange<FVector> BoundsMin = InPointData->GetConstBoundsMinValueRange();
		TConstPCGValueRange<FVector> BoundsMax = InPointData->GetConstBoundsMaxValueRange();

		TPCGValueRange<float> Density = PointDataFacade->GetOut()->GetDensityValueRange(bOutputWeight && !WeightWriter && !NormalizedWeightWriter);

		auto InvalidPoint = [&](const int32 Index)
		{
			PathWriter->SetValue(Index, FSoftObjectPath{});

			if (bOutputWeight)
			{
				if (WeightWriter) { WeightWriter->SetValue(Index, -1); }
				else if (NormalizedWeightWriter) { NormalizedWeightWriter->SetValue(Index, -1); }
				else { Density[Index] = 0; }
			}
		};

		PCGEX_SCOPE_LOOP(Index)
		{
			if (Index == LastIndex && !bClosedLoop)
			{
				// Ignore last index, only used for maths reasons
				InvalidPoint(Index);
				continue;
			}

			if (!PointFilterCache[Index])
			{
				Segments[Index] = PCGExPaths::FSplineMeshSegment();
				InvalidPoint(Index);
				continue;
			}

			const FPCGExMeshCollectionEntry* MeshEntry = nullptr;
			const UPCGExAssetCollection* EntryHost = nullptr;

			const int32 Seed = PCGExRandom::GetSeed(
				Seeds[Index], Helper->Details.SeedComponents,
				Helper->Details.LocalSeed, Settings, Context->GetComponent());

			Segments[Index] = PCGExPaths::FSplineMeshSegment();
			PCGExPaths::FSplineMeshSegment& Segment = Segments[Index];

			if (bUseTags) { Helper->GetEntry(MeshEntry, Index, Seed, Settings->TaggingDetails.GrabTags, Segment.Tags, EntryHost); }
			else { Helper->GetEntry(MeshEntry, Index, Seed, EntryHost); }

			Segment.MeshEntry = MeshEntry;

			if (!MeshEntry)
			{
				InvalidPoint(Index);
				continue;
			}

			if (MeshEntry->MacroCache && MeshEntry->MacroCache->GetType() == PCGExAssetCollection::EType::Mesh)
			{
				Segment.MaterialPick = StaticCastSharedPtr<PCGExMeshCollection::FMacroCache>(MeshEntry->MacroCache)->GetPickRandomWeighted(Seed);
				if (Segment.MaterialPick != -1) { MeshEntry->GetMaterialPaths(Segment.MaterialPick, *ScopedMaterials->Get(Scope)); }
			}

			if (bOutputWeight)
			{
				double Weight = bNormalizedWeight ? static_cast<double>(MeshEntry->Weight) / static_cast<double>(Context->MainCollection->LoadCache()->WeightSum) : MeshEntry->Weight;
				if (bOneMinusWeight) { Weight = 1 - Weight; }
				if (WeightWriter) { WeightWriter->SetValue(Index, Weight); }
				else if (NormalizedWeightWriter) { NormalizedWeightWriter->SetValue(Index, Weight); }
				else { Density[Index] = Weight; }
			}

			PathWriter->SetValue(Index, MeshEntry->Staging.Path);

			//

			const int32 NextIndex = Index + 1 > LastIndex ? 0 : Index + 1;

			//

			const FBox& StBox = MeshEntry->Staging.Bounds;
			FVector OutScale = Transforms[Index].GetScale3D();
			const FBox InBounds = FBox(BoundsMin[Index] * OutScale, BoundsMax[Index] * OutScale);
			FBox OutBounds = StBox;

			Settings->ScaleToFit.Process(PCGExData::FConstPoint(InPointData, Index), MeshEntry->Staging.Bounds, OutScale, OutBounds);

			FVector OutTranslation = FVector::ZeroVector;
			OutBounds = FBox(OutBounds.Min * OutScale, OutBounds.Max * OutScale);

			Justification.Process(Index, InBounds, OutBounds, OutTranslation);

			//

			int32 C1 = 1;
			int32 C2 = 2;
			PCGExPaths::GetAxisForEntry(MeshEntry->SMDescriptor, Segment.SplineMeshAxis, C1, C2, Settings->DefaultDescriptor.SplineMeshAxis);

			Segment.Params.StartPos = Transforms[Index].GetLocation();
			Segment.Params.StartScale = FVector2D(OutScale[C1], OutScale[C2]);
			Segment.Params.StartRoll = Transforms[Index].GetRotation().Rotator().Roll;

			const FVector Scale = bApplyScaleToFit ? OutScale : Transforms[NextIndex].GetScale3D();
			Segment.Params.EndPos = Transforms[NextIndex].GetLocation();
			Segment.Params.EndScale = FVector2D(Scale[C1], Scale[C2]);
			Segment.Params.EndRoll = Transforms[NextIndex].GetRotation().Rotator().Roll;

			Segment.Params.StartOffset = FVector2D(OutTranslation[C1], OutTranslation[C2]);
			Segment.Params.EndOffset = FVector2D(OutTranslation[C1], OutTranslation[C2]);

			if (TangentsHandler->IsEnabled())
			{
				TangentsHandler->GetSegmentTangents(Index, Segment.Params.StartTangent, Segment.Params.EndTangent);
			}
			else
			{
				Segment.Params.StartTangent = Transforms[Index].GetRotation().GetForwardVector();
				Segment.Params.EndTangent = Transforms[NextIndex].GetRotation().GetForwardVector();
			}

			if (UpGetter) { Segment.UpVector = UpGetter->Read(Index); }
			else if (Settings->SplineMeshUpMode == EPCGExSplineMeshUpMode::Constant) { Segment.UpVector = Settings->SplineMeshUpVector; }
			else { Segment.ComputeUpVectorFromTangents(); }

			SegmentMutationDetails.Mutate(Index, Segment);
		}
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		PCGEX_MAKE_SHARED(MaterialPaths, TSet<FSoftObjectPath>)
		ScopedMaterials->Collapse(*MaterialPaths.Get());
		if (!MaterialPaths->IsEmpty()) { PCGExHelpers::LoadBlocking_AnyThread(MaterialPaths); } // TODO : Refactor this atrocity
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->WriteFastest(AsyncManager);
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

			Segment.ApplySettings(SplineMeshComponent); // Init Component

			if (Settings->bForceDefaultDescriptor || Settings->CollectionSource == EPCGExCollectionSource::AttributeSet) { Settings->DefaultDescriptor.InitComponent(SplineMeshComponent); }
			else { Segment.MeshEntry->SMDescriptor.InitComponent(SplineMeshComponent); }

			if (!Segment.ApplyMesh(SplineMeshComponent)) { continue; }

			if (Settings->TaggingDetails.bForwardInputDataTags) { SplineMeshComponent->ComponentTags.Append(DataTags); }
			if (!Segment.Tags.IsEmpty()) { SplineMeshComponent->ComponentTags.Append(Segment.Tags.Array()); }

			Context->AttachManagedComponent(
				TargetActor, SplineMeshComponent,
				FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));

			Context->AddNotifyActor(TargetActor);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

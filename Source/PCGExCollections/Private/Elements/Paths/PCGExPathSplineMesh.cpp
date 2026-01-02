// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Paths/PCGExPathSplineMesh.h"

#include "PCGComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Helpers/PCGExRandomHelpers.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "PCGExVersion.h"
#include "PCGParamData.h"
#include "Containers/PCGExScopedContainers.h"
#include "Helpers/PCGExCollectionsHelpers.h"
#include "Metadata/PCGObjectPropertyOverride.h"
#include "Paths/PCGExPath.h"
#include "Paths/PCGExPathsHelpers.h"
#include "Utils/PCGExUniqueNameGenerator.h"

#define LOCTEXT_NAMESPACE "PCGExPathSplineMeshElement"
#define PCGEX_NAMESPACE BuildCustomGraph

#if WITH_EDITOR
void UPCGExPathSplineMeshSettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	PCGEX_UPDATE_TO_DATA_VERSION(1, 70, 11)
	{
		DefaultDescriptor.SplineMeshAxis = static_cast<EPCGExSplineMeshAxis>(SplineMeshAxisConstant_DEPRECATED);
		Tangents.ApplyDeprecation(bApplyCustomTangents_DEPRECATED, ArriveTangentAttribute_DEPRECATED, LeaveTangentAttribute_DEPRECATED);
	}

	Super::ApplyDeprecation(InOutNode);
}
#endif

PCGEX_INITIALIZE_ELEMENT(PathSplineMesh)
PCGEX_ELEMENT_BATCH_POINT_IMPL(PathSplineMesh)

UPCGExPathSplineMeshSettings::UPCGExPathSplineMeshSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (SplineMeshUpVectorAttribute.GetName() == FName("@Last")) { SplineMeshUpVectorAttribute.Update(TEXT("$Rotation.Up")); }
}

TArray<FPCGPinProperties> UPCGExPathSplineMeshSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	if (CollectionSource == EPCGExCollectionSource::AttributeSet)
	{
		PCGEX_PIN_PARAM(PCGExCollections::Labels::SourceAssetCollection, "Attribute set to be used as collection.", Required)
	}

	return PinProperties;
}

PCGExData::EIOInit UPCGExPathSplineMeshSettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::Duplicate; }

bool FPCGExPathSplineMeshElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathSplineMesh)

	if (!Context->Tangents.Init(Context, Settings->Tangents)) { return false; }

	if (Settings->CollectionSource == EPCGExCollectionSource::Asset)
	{
		PCGExHelpers::LoadBlocking_AnyThreadTpl(Settings->AssetCollection, Context);
		Context->MainCollection = Settings->AssetCollection.Get();
		if (!Context->MainCollection)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Missing asset collection."));
			return false;
		}
	}
	else if (Settings->CollectionSource == EPCGExCollectionSource::AttributeSet)
	{
		Context->MainCollection = Cast<UPCGExMeshCollection>(Settings->AttributeSetDetails.TryBuildCollection(Context, PCGExCollections::Labels::SourceAssetCollection, false));
		if (!Context->MainCollection)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to build collection from attribute set."));
			return false;
		}
	}
	else
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Per-point collection is not supported with Spline Mesh (yet)"));
		return false;
	}

	PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->AssetPathAttributeName)

	if (Settings->WeightToAttribute == EPCGExWeightOutputMode::Raw || Settings->WeightToAttribute == EPCGExWeightOutputMode::Normalized)
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

bool FPCGExPathSplineMeshElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathSplineMeshElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathSplineMesh)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bHasInvalidInputs = true;
					Entry->InitializeOutput(PCGExData::EIOInit::Forward);
					return false;
				}
				return true;
			}, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to write tangents to."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->MainPoints->StageOutputs();
	Context->ExecuteOnNotifyActors(Settings->PostProcessFunctionNames);

	return Context->TryComplete();
}

namespace PCGExPathSplineMesh
{
	void FSplineMeshSegment::ApplySettings(USplineMeshComponent* Component) const
	{
		PCGExPaths::FSplineMeshSegment::ApplySettings(Component);
		if (bSetMeshWithSettings) { ApplyMesh(Component); }
	}

	bool FSplineMeshSegment::ApplyMesh(USplineMeshComponent* Component) const
	{
		check(Component)
		UStaticMesh* StaticMesh = MeshEntry->Staging.TryGet<UStaticMesh>(); //LoadSynchronous<UStaticMesh>();

		if (!StaticMesh) { return false; }

		Component->SetStaticMesh(StaticMesh); // Will trigger a force rebuild, so put this last
		MeshEntry->ApplyMaterials(MaterialPick, Component);

		return true;
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		bIsPreviewMode = ExecutionContext->GetComponent()->IsInPreviewMode();

		Justification = Settings->Justification;
		Justification.Init(ExecutionContext, PointDataFacade);

		SegmentMutationDetails = Settings->MutationDetails;
		if (!SegmentMutationDetails.Init(PointDataFacade)) { return false; }

		bClosedLoop = PCGExPaths::Helpers::GetClosedLoop(PointDataFacade->GetIn());
		bApplyScaleToFit = Settings->ScaleToFit.ScaleToFitMode != EPCGExFitMode::None;
		bUseTags = Settings->TaggingDetails.IsEnabled();

		TangentsHandler = MakeShared<PCGExTangents::FTangentsHandler>(bClosedLoop);
		if (!TangentsHandler->Init(Context, Context->Tangents, PointDataFacade)) { return false; }

		Helper = MakeShared<PCGExCollections::FDistributionHelper>(Context->MainCollection, Settings->DistributionSettings);
		if (!Helper->Init(PointDataFacade)) { return false; }

		MicroHelper = MakeShared<PCGExCollections::FMicroDistributionHelper>(Settings->MaterialDistributionSettings);
		if (!MicroHelper->Init(PointDataFacade)) { return false; }

		if (Settings->SplineMeshUpMode == EPCGExSplineMeshUpMode::Attribute)
		{
			UpGetter = PointDataFacade->GetBroadcaster<FVector>(Settings->SplineMeshUpVectorAttribute, true);

			if (!UpGetter)
			{
				PCGEX_LOG_INVALID_SELECTOR_C(Context, Spline Mesh Up Vector, Settings->SplineMeshUpVectorAttribute)
				return false;
			}
		}

		LastIndex = PointDataFacade->GetNum() - 1;

		PCGExArrayHelpers::InitArray(Segments, bClosedLoop ? LastIndex + 1 : LastIndex);

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

		bool bAnyValidSegment = false;

		const FPCGExAssetDistributionDetails& Details = Helper->Details;
		const UPCGComponent* Component = Context->GetComponent();
		const bool bTangentsEnabled = TangentsHandler->IsEnabled();

		PCGEX_SCOPE_LOOP(Index)
		{
			if (!PointFilterCache[Index] || (Index == LastIndex && !bClosedLoop))
			{
				InvalidPoint(Index);
				continue;
			}

			FSplineMeshSegment Segment;
			ON_SCOPE_EXIT { Segments[Index] = Segment; };

			FPCGExEntryAccessResult Result;
			const FPCGExMeshCollectionEntry* MeshEntry = nullptr;

			const int32 Seed = PCGExRandomHelpers::GetSeed(Seeds[Index], Helper->Details.SeedComponents, Helper->Details.LocalSeed, Settings, Component);

			if (bUseTags) { Result = Helper->GetEntry(Index, Seed, Settings->TaggingDetails.GrabTags, Segment.Tags); }
			else { Result = Helper->GetEntry(Index, Seed); }

			MeshEntry = static_cast<const FPCGExMeshCollectionEntry*>(Result.Entry);
			Segment.MeshEntry = MeshEntry;

			if (!MeshEntry)
			{
				InvalidPoint(Index);
				continue;
			}

			const int32 NextIndex = Index + 1 > LastIndex ? 0 : Index + 1;
			const FTransform& CurrentTransform = Transforms[Index];
			const FVector CurrentLocation = CurrentTransform.GetLocation();
			const FQuat CurrentRotation = CurrentTransform.GetRotation();
			const FVector CurrentScale = CurrentTransform.GetScale3D();
			const FTransform& NextTransform = Transforms[NextIndex];
			const FVector NextLocation = NextTransform.GetLocation();
			const FQuat NextRotation = NextTransform.GetRotation();
			const FVector NextScale = NextTransform.GetScale3D();

			if (const PCGExAssetCollection::FMicroCache* MicroCache = MeshEntry->MicroCache.Get();
				MicroHelper && MicroCache && MicroCache->GetTypeId() == PCGExAssetCollection::TypeIds::Mesh)
			{
				const PCGExMeshCollection::FMicroCache* EntryMicroCache = static_cast<const PCGExMeshCollection::FMicroCache*>(MicroCache);
				Segment.MaterialPick = MicroHelper->GetPick(EntryMicroCache, Index, Seed);
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

			const FBox& StBox = MeshEntry->Staging.Bounds;
			FVector OutScale = CurrentScale;
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

			Segment.Params.StartPos = CurrentLocation;
			Segment.Params.StartScale = FVector2D(OutScale[C1], OutScale[C2]);
			Segment.Params.StartRoll = CurrentRotation.Rotator().Roll;

			const FVector Scale = bApplyScaleToFit ? OutScale : NextScale;
			Segment.Params.EndPos = NextLocation;
			Segment.Params.EndScale = FVector2D(Scale[C1], Scale[C2]);
			Segment.Params.EndRoll = NextRotation.Rotator().Roll;

			Segment.Params.StartOffset = FVector2D(OutTranslation[C1], OutTranslation[C2]);
			Segment.Params.EndOffset = FVector2D(OutTranslation[C1], OutTranslation[C2]);

			if (bTangentsEnabled)
			{
				TangentsHandler->GetSegmentTangents(Index, Segment.Params.StartTangent, Segment.Params.EndTangent);
			}
			else
			{
				Segment.Params.StartTangent = CurrentRotation.GetForwardVector();
				Segment.Params.EndTangent = NextRotation.GetForwardVector();
			}

			if (UpGetter) { Segment.UpVector = UpGetter->Read(Index); }
			else if (Settings->SplineMeshUpMode == EPCGExSplineMeshUpMode::Constant) { Segment.UpVector = Settings->SplineMeshUpVector; }
			else { Segment.ComputeUpVectorFromTangents(); }

			SegmentMutationDetails.Mutate(Index, Segment);
			bAnyValidSegment = true;
		}

		if (bAnyValidSegment) { FPlatformAtomics::InterlockedExchange(&bHasValidSegments, 1); }
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		if (!bHasValidSegments)
		{
			bIsProcessorValid = false;
			return;
		}

		PCGEX_MAKE_SHARED(MaterialPaths, TSet<FSoftObjectPath>)
		ScopedMaterials->Collapse(*MaterialPaths.Get());
		if (!MaterialPaths->IsEmpty()) { PCGExHelpers::LoadBlocking_AnyThread(MaterialPaths, Context); } // TODO : Refactor this atrocity

		//


		TargetActor = Settings->TargetActor.Get() ? Settings->TargetActor.Get() : ExecutionContext->GetTargetActor(nullptr);
		ObjectFlags = (bIsPreviewMode ? RF_Transient : RF_NoFlags);

		if (!TargetActor)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Invalid target actor."));
			bIsProcessorValid = false;
			return;
		}

		const int32 FinalNumSegments = Segments.Num();

		if (!FinalNumSegments)
		{
			bIsProcessorValid = false;
			return;
		}

		MainThreadLoop = MakeShared<PCGExMT::FTimeSlicedMainThreadLoop>(FinalNumSegments);
		MainThreadLoop->OnIterationCallback = [&](const int32 Index, const PCGExMT::FScope& Scope) { ProcessSegment(Index); };

		PCGEX_ASYNC_HANDLE_CHKD_VOID(TaskManager, MainThreadLoop)
	}

	void FProcessor::ProcessSegment(const int32 Index)
	{
		const FSplineMeshSegment& Segment = Segments[Index];
		if (!Segment.MeshEntry) { return; }

		USplineMeshComponent* SplineMeshComponent = Context->ManagedObjects->New<USplineMeshComponent>(TargetActor, MakeUniqueObjectName(TargetActor, USplineMeshComponent::StaticClass(), Context->UniqueNameGenerator->Get(TEXT("PCGSplineMeshComponent_") + Segment.MeshEntry->Staging.Path.GetAssetName())), ObjectFlags);

		if (!SplineMeshComponent || !Segment.MeshEntry) { return; }

		Segment.ApplySettings(SplineMeshComponent); // Init Component
		if (Settings->bForceDefaultDescriptor || Settings->CollectionSource == EPCGExCollectionSource::AttributeSet) { Settings->DefaultDescriptor.InitComponent(SplineMeshComponent); }
		else { Segment.MeshEntry->SMDescriptor.InitComponent(SplineMeshComponent); }

		if (Settings->TaggingDetails.bForwardInputDataTags) { SplineMeshComponent->ComponentTags.Append(DataTags); }
		if (!Segment.Tags.IsEmpty()) { SplineMeshComponent->ComponentTags.Append(Segment.Tags.Array()); }

		if (!Settings->PropertyOverrideDescriptions.IsEmpty())
		{
			FPCGObjectOverrides DescriptorOverride(SplineMeshComponent);
			DescriptorOverride.Initialize(Settings->PropertyOverrideDescriptions, SplineMeshComponent, PointDataFacade->Source->GetIn(), Context);
			if (DescriptorOverride.IsValid() && !DescriptorOverride.Apply(Index))
			{
				PCGLog::LogWarningOnGraph(FText::Format(LOCTEXT("FailOverride", "Failed to override descriptor for input {0}"), Index));
			}
		}

		if (!Segment.ApplyMesh(SplineMeshComponent)) { return; }

		Context->AttachManagedComponent(TargetActor, SplineMeshComponent, FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));

		Context->AddNotifyActor(TargetActor);
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->WriteFastest(TaskManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

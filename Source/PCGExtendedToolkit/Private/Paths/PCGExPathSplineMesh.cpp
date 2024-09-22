// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathSplineMesh.h"

#include "PCGExHelpers.h"
#include "PCGExManagedResource.h"
#include "Collections/PCGExInternalCollection.h"
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

FPCGExPathSplineMeshContext::~FPCGExPathSplineMeshContext()
{
	PCGEX_TERMINATE_ASYNC
	UPCGExInternalCollection* InternalCollection = Cast<UPCGExInternalCollection>(MainCollection);
	PCGEX_DELETE_UOBJECT(InternalCollection)
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

	if (!Context->MainCollection)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Missing asset collection."));
		return false;
	}

	if (Settings->CollectionSource == EPCGExCollectionSource::AttributeSet)
	{
		// Internal collection, assets have been loaded at this point, rebuilding stage data
		Context->MainCollection->RebuildStagingData(true);
	}

	Context->MainCollection->LoadCache(); // Make sure to load the stuff

	PCGEX_VALIDATE_NAME(Settings->AssetPathAttributeName)

	if (Settings->WeightToAttribute == EPCGExWeightOutputMode::Raw || Settings->WeightToAttribute == EPCGExWeightOutputMode::Normalized)
	{
		PCGEX_VALIDATE_NAME(Settings->WeightAttributeName)
	}

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

	Context->MainPoints->OutputToContext();
	
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
		PointDataFacade->bSupportsScopedGet = TypedContext->bScopedAttributeGet;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		LocalSettings = Settings;
		LocalTypedContext = TypedContext;

		Justification = Settings->Justification;
		Justification.Init(Context, PointDataFacade);

		bClosedLoop = TypedContext->ClosedLoop.IsClosedLoop(PointIO);
		bApplyScaleToFit = Settings->ScaleToFit.ScaleToFitMode != EPCGExFitMode::None;

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

		PCGEX_SET_NUM_UNINITIALIZED(Segments, bClosedLoop ? LastIndex + 1 : LastIndex)
		//PCGEX_SET_NUM_UNINITIALIZED(SplineMeshComponents, LastIndex)

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
			WeightWriter = PointDataFacade->GetWriter<int32>(Settings->WeightAttributeName, true);
		}
		else if (Settings->WeightToAttribute == EPCGExWeightOutputMode::Normalized)
		{
			NormalizedWeightWriter = PointDataFacade->GetWriter<double>(Settings->WeightAttributeName, true);
		}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
		PathWriter = PointDataFacade->GetWriter<FSoftObjectPath>(Settings->AssetPathAttributeName, false);
#else
		PathWriter = PointDataFacade->GetWriter<FString>(Settings->AssetPathAttributeName, true);
#endif

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
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
			PathWriter->Values[Index] = FSoftObjectPath{};
#else
			PathWriter->Values[Index] = TEXT("");
#endif

			if (bOutputWeight)
			{
				if (WeightWriter) { WeightWriter->Values[Index] = -1; }
				else if (NormalizedWeightWriter) { NormalizedWeightWriter->Values[Index] = -1; }
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

		const FPCGExAssetStagingData* StagingData = nullptr;

		const int32 Seed = PCGExRandom::GetSeedFromPoint(
			Helper->Details.SeedComponents, Point,
			Helper->Details.LocalSeed, LocalSettings, LocalTypedContext->SourceComponent.Get());

		Helper->GetStaging(StagingData, Index, Seed);

		Segments[Index] = PCGExPaths::FSplineMeshSegment();
		PCGExPaths::FSplineMeshSegment& Segment = Segments[Index];
		Segment.AssetStaging = StagingData;

		if (!StagingData)
		{
			InvalidPoint();
			return;
		}

		//

		if (bOutputWeight)
		{
			double Weight = bNormalizedWeight ? static_cast<double>(StagingData->Weight) / static_cast<double>(LocalTypedContext->MainCollection->LoadCache()->WeightSum) : StagingData->Weight;
			if (bOneMinusWeight) { Weight = 1 - Weight; }
			if (WeightWriter) { WeightWriter->Values[Index] = Weight; }
			else if (NormalizedWeightWriter) { NormalizedWeightWriter->Values[Index] = Weight; }
			else { Point.Density = Weight; }
		}

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION > 3
		PathWriter->Values[Index] = StagingData->Path;
#else
		PathWriter->Values[Index] = StagingData->Path.ToString();
#endif

		//

		Segment.SplineMeshAxis = SplineMeshAxisConstant;

		const int32 NextIndex = Index + 1 > LastIndex ? 0 : Index + 1;
		const FPCGPoint& NextPoint = PointIO->GetInPoint(NextIndex);

		//

		const FBox& StBox = StagingData->Bounds;
		FVector OutScale = Point.Transform.GetScale3D();
		const FBox InBounds = FBox(Point.BoundsMin * OutScale, Point.BoundsMax * OutScale);
		FBox OutBounds = StBox;

		LocalSettings->ScaleToFit.Process(Point, StagingData->Bounds, OutScale, OutBounds);

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

		if (LocalSettings->bApplyCustomTangents)
		{
			Segment.Params.StartTangent = LeaveReader->Values[Index];
			Segment.Params.EndTangent = ArriveReader->Values[NextIndex];
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}

	void FProcessor::Output()
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExPathSplineMesh::FProcessor::Output);

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
		for (int i = 0; i < SplineMeshComponents.Num(); ++i)
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

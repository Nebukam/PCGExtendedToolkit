// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathSplineMeshSimple.h"

#include "PCGExHelpers.h"
#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExPathSplineMeshSimpleElement"
#define PCGEX_NAMESPACE BuildCustomGraph

PCGEX_INITIALIZE_ELEMENT(PathSplineMeshSimple)

PCGExData::EIOInit UPCGExPathSplineMeshSimpleSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

bool FPCGExPathSplineMeshSimpleElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathSplineMeshSimple)

	if (Settings->bApplyCustomTangents)
	{
		PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->ArriveTangentAttribute)
		PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->LeaveTangentAttribute)
	}

	PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->AssetPathAttributeName)

	TArray<FName> Names = {Settings->AssetPathAttributeName};
	Context->StaticMeshLoader = MakeShared<PCGEx::TAssetLoader<UStaticMesh>>(Context, Context->MainPoints.ToSharedRef(), Names);
	return true;
}

bool FPCGExPathSplineMeshSimpleElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathSplineMeshSimpleElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathSplineMeshSimple)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StaticMeshLoader->Start(Context->GetAsyncManager(), PCGEx::State_WaitingOnAsyncWork))
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Failed to find any asset to load."));
			return true;
		}

		return false;
	}

	if (!Context->StaticMeshLoader->Execute()) { return false; }

	PCGEX_ON_STATE(PCGEx::State_WaitingOnAsyncWork)
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 2 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExPathSplineMeshSimple::FProcessor>>(
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
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExPathSplineMeshSimple::FProcessor>>& NewBatch)
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

namespace PCGExPathSplineMeshSimple
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		if (Settings->StartOffsetInput == EPCGExInputValueType::Attribute)
		{
			StartOffsetGetter = PointDataFacade->GetScopedBroadcaster<FVector2D>(Settings->StartOffsetAttribute);

			if (!StartOffsetGetter)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("StartOffset attribute is missing on some inputs.."));
				return false;
			}
		}

		if (Settings->EndOffsetInput == EPCGExInputValueType::Attribute)
		{
			EndOffsetGetter = PointDataFacade->GetScopedBroadcaster<FVector2D>(Settings->EndOffsetAttribute);

			if (!EndOffsetGetter)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("EndOffset attribute is missing on some inputs.."));
				return false;
			}
		}

#if PCGEX_ENGINE_VERSION <= 503
		AssetPathReader = PointDataFacade->GetScopedBroadcaster<FString>(Settings->AssetPathAttributeName);
#else
		AssetPathReader = PointDataFacade->GetScopedBroadcaster<FSoftObjectPath>(Settings->AssetPathAttributeName);
#endif

		if (!AssetPathReader)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("AssetPath attribute is missing on some inputs.."));
			return false;
		}

		bClosedLoop = Context->ClosedLoop.IsClosedLoop(PointDataFacade->Source);
		bUseTags = Settings->TaggingDetails.IsEnabled();

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
		Meshes.Init(nullptr, Segments.Num());

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
			// Do nothing
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

		TObjectPtr<UStaticMesh>* SM = Context->StaticMeshLoader->GetAsset(AssetPathReader->Read(Index));
		if (!SM)
		{
			InvalidPoint();
			return;
		}

		Meshes[Index] = *SM;
		Segments[Index] = PCGExPaths::FSplineMeshSegment();
		PCGExPaths::FSplineMeshSegment& Segment = Segments[Index];

		//

		Segment.SplineMeshAxis = SplineMeshAxisConstant;

		const int32 NextIndex = Index + 1 > LastIndex ? 0 : Index + 1;
		const FPCGPoint& NextPoint = PointDataFacade->Source->GetInPoint(NextIndex);

		//

		FVector OutScale = Point.Transform.GetScale3D();

		//

		Segment.Params.StartPos = Point.Transform.GetLocation();
		Segment.Params.StartScale = FVector2D(OutScale[C1], OutScale[C2]);
		Segment.Params.StartRoll = Point.Transform.GetRotation().Rotator().Roll;

		const FVector Scale = NextPoint.Transform.GetScale3D();
		Segment.Params.EndPos = NextPoint.Transform.GetLocation();
		Segment.Params.EndScale = FVector2D(Scale[C1], Scale[C2]);
		Segment.Params.EndRoll = NextPoint.Transform.GetRotation().Rotator().Roll;

		Segment.Params.StartOffset = StartOffsetGetter ? StartOffsetGetter->Read(Index) : Settings->StartOffset;
		Segment.Params.EndOffset = EndOffsetGetter ? EndOffsetGetter->Read(Index) : Settings->EndOffset;

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
		TRACE_CPUPROFILER_EVENT_SCOPE(UPCGExPathSplineMeshSimple::FProcessor::Output);

		// TODO : Resolve per-point target actor...? irk.
		AActor* TargetActor = Settings->TargetActor.Get() ? Settings->TargetActor.Get() : ExecutionContext->GetTargetActor(nullptr);

		if (!TargetActor)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Invalid target actor."));
			return;
		}

		bool bIsPreviewMode = false;
#if PCGEX_ENGINE_VERSION > 503
		bIsPreviewMode = ExecutionContext->SourceComponent.Get()->IsInPreviewMode();
#endif

		TArray<FName> DataTags = PointDataFacade->Source->Tags->FlattenToArrayOfNames();

		for (int i = 0; i < Segments.Num(); i++)
		{
			UStaticMesh* Mesh = Meshes[i];
			if (!Mesh) { continue; }

			const PCGExPaths::FSplineMeshSegment& Segment = Segments[i];

			const EObjectFlags ObjectFlags = (bIsPreviewMode ? RF_Transient : RF_NoFlags);
			USplineMeshComponent* SplineMeshComponent = NewObject<USplineMeshComponent>(
				TargetActor, MakeUniqueObjectName(
					TargetActor, USplineMeshComponent::StaticClass(),
					Context->UniqueNameGenerator->Get(TEXT("PCGSplineMeshComponent_") + Mesh->GetName())), ObjectFlags);

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

			if (Settings->TaggingDetails.bForwardInputDataTags) { SplineMeshComponent->ComponentTags.Append(DataTags); }
			if (!Segment.Tags.IsEmpty()) { SplineMeshComponent->ComponentTags.Append(Segment.Tags.Array()); }

			Settings->StaticMeshDescriptor.InitComponent(SplineMeshComponent);

			SplineMeshComponent->SetStaticMesh(Mesh); // Will trigger a force rebuild, so put this last

			Context->AttachManagedComponent(
				TargetActor, SplineMeshComponent,
				FAttachmentTransformRules(EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));

			Context->NotifyActors.Add(TargetActor);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

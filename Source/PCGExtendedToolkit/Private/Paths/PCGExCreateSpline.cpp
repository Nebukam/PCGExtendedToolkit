// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExCreateSpline.h"

#include "Helpers/PCGHelpers.h"


#define LOCTEXT_NAMESPACE "PCGExCreateSplineElement"
#define PCGEX_NAMESPACE CreateSpline

PCGExData::EIOInit UPCGExCreateSplineSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGEX_INITIALIZE_ELEMENT(CreateSpline)

TArray<FPCGPinProperties> UPCGExCreateSplineSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POLYLINES(GetMainOutputPin(), "Spline data.", Required, {})
	return PinProperties;
}

void FPCGExCreateSplineElement::DisabledPassThroughData(FPCGContext* Context) const
{
	// No passthrough
}

bool FPCGExCreateSplineElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CreateSpline)

	if (Settings->bApplyCustomTangents)
	{
		PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->ArriveTangentAttribute);
		PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->LeaveTangentAttribute);
	}

	return true;
}

bool FPCGExCreateSplineElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCreateSplineElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(CreateSpline)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some input have less than 2 points and will be ignored."))
		if (!Context->StartBatchProcessingPoints<PCGExCreateSpline::FBatch>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bHasInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExCreateSpline::FBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any dataset to generate splines."));
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

	return Context->TryComplete();
}

bool FPCGExCreateSplineElement::IsCacheable(const UPCGSettings* InSettings) const
{
	const UPCGCreateSplineSettings* Settings = Cast<const UPCGCreateSplineSettings>(InSettings);
	return !Settings || Settings->Mode == EPCGCreateSplineMode::CreateDataOnly;
}

namespace PCGExCreateSpline
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCreateSpline::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		bClosedLoop = Context->ClosedLoop.IsClosedLoop(PointDataFacade->Source);

		if (Settings->bApplyCustomTangents)
		{
			ArriveTangent = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->ArriveTangentAttribute);
			LeaveTangent = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->LeaveTangentAttribute);
			if (!ArriveTangent || !LeaveTangent)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Missing tangent attributes"));
				return false;
			}
		}

		if (Settings->bApplyCustomPointType)
		{
			CustomPointType = PointDataFacade->GetScopedBroadcaster<int32>(Settings->PointTypeAttribute);
			if (!CustomPointType)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Missing custom point type attribute"));
				return false;
			}
		}

		PositionOffset = SplineActor->GetTransform().GetLocation();
		SplineData = Context->ManagedObjects->New<UPCGSplineData>();
		PCGEx::InitArray(SplinePoints, PointDataFacade->GetNum());

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope)
	{
		TPointsProcessor<FPCGExCreateSplineContext, UPCGExCreateSplineSettings>::PrepareSingleLoopScopeForPoints(Scope);
		PointDataFacade->Fetch(Scope);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		FVector OutArrive = FVector::ZeroVector;
		FVector OutLeave = FVector::ZeroVector;

		if (Settings->bApplyCustomTangents)
		{
			OutArrive = ArriveTangent->Read(Index);
			OutLeave = LeaveTangent->Read(Index);
		}

		const FTransform& TR = Point.Transform;

		EPCGExSplinePointType PointTypeProxy = Settings->DefaultPointType;
		ESplinePointType::Type PointType = ESplinePointType::Curve;

		if (CustomPointType)
		{
			const int32 Value = CustomPointType->Read(Index);
			if (FMath::IsWithinInclusive(Value, 0, 4)) { PointTypeProxy = static_cast<EPCGExSplinePointType>(static_cast<uint8>(Value)); }
		}

		switch (PointTypeProxy)
		{
		case EPCGExSplinePointType::Linear:
			PointType = ESplinePointType::Linear;
			break;
		case EPCGExSplinePointType::Curve:
			PointType = ESplinePointType::Curve;
			break;
		case EPCGExSplinePointType::Constant:
			PointType = ESplinePointType::Constant;
			break;
		case EPCGExSplinePointType::CurveClamped:
			PointType = ESplinePointType::CurveClamped;
			break;
		case EPCGExSplinePointType::CurveCustomTangent:
			PointType = ESplinePointType::CurveCustomTangent;
			break;
		}

		SplinePoints[Index] = FSplinePoint(
			static_cast<float>(Index),
			TR.GetLocation() - PositionOffset,
			OutArrive,
			OutLeave,
			TR.GetRotation().Rotator(),
			TR.GetScale3D(),
			PointType);
	}

	void FProcessor::Output()
	{
		TPointsProcessor<FPCGExCreateSplineContext, UPCGExCreateSplineSettings>::Output();

		// Output spline data
		SplineData->Initialize(SplinePoints, bClosedLoop, FTransform(PositionOffset));
		Context->StageOutput(Settings->GetMainOutputPin(), SplineData, PointDataFacade->Source->Tags->ToSet(), true, false);

		// Output spline component
		if (Settings->Mode != EPCGCreateSplineMode::CreateDataOnly)
		{
			bool bIsPreviewMode = false;
#if PCGEX_ENGINE_VERSION > 503
			bIsPreviewMode = ExecutionContext->SourceComponent.Get()->IsInPreviewMode();
#endif

			const FString ComponentName = TEXT("PCGSplineComponent");
			const EObjectFlags ObjectFlags = (bIsPreviewMode ? RF_Transient : RF_NoFlags);
			USplineComponent* SplineComponent = NewObject<USplineComponent>(SplineActor, MakeUniqueObjectName(SplineActor, USplineComponent::StaticClass(), FName(ComponentName)), ObjectFlags);

			PointDataFacade->Source->Tags->DumpTo(SplineComponent->ComponentTags);

			SplineData->ApplyTo(SplineComponent);

			Context->AttachManagedComponent(
				SplineActor, SplineComponent,
				FAttachmentTransformRules(EAttachmentRule::KeepRelative, EAttachmentRule::KeepWorld, EAttachmentRule::KeepWorld, false));

			Context->NotifyActors.Add(SplineActor);
		}
	}

	void FProcessor::Cleanup()
	{
		TPointsProcessor<FPCGExCreateSplineContext, UPCGExCreateSplineSettings>::Cleanup();
	}

	bool FBatch::PrepareSingle(const TSharedPtr<FProcessor>& PointsProcessor)
	{
		if (!TargetActor) { return false; }
		if (!TBatch<FProcessor>::PrepareSingle(PointsProcessor)) { return false; }
		PointsProcessor->SplineActor = TargetActor;
		return true;
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

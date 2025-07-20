// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExCreateSpline.h"


#include "Helpers/PCGHelpers.h"


#define LOCTEXT_NAMESPACE "PCGExCreateSplineElement"
#define PCGEX_NAMESPACE CreateSpline

#if WITH_EDITOR
void UPCGExCreateSplineSettings::ApplyPCGExDeprecation()
{
	if (ArriveTangentAttribute_DEPRECATED != PCGEx::DEPRECATED_NAME)
	{
		Tangents.ApplyDeprecation(bApplyCustomTangents_DEPRECATED, ArriveTangentAttribute_DEPRECATED, LeaveTangentAttribute_DEPRECATED);
		ArriveTangentAttribute_DEPRECATED = PCGEx::DEPRECATED_NAME;
		LeaveTangentAttribute_DEPRECATED = PCGEx::DEPRECATED_NAME;
		MarkPackageDirty();
	}

	Super::ApplyPCGExDeprecation();
}
#endif

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

	if (!Context->Tangents.Init(Context, Settings->Tangents)) { return false; }

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
	Context->ExecuteOnNotifyActors(Settings->PostProcessFunctionNames);

	return Context->TryComplete();
}

bool FPCGExCreateSplineElement::IsCacheable(const UPCGSettings* InSettings) const
{
	const UPCGCreateSplineSettings* Settings = Cast<const UPCGCreateSplineSettings>(InSettings);
	return !Settings || Settings->Mode == EPCGCreateSplineMode::CreateDataOnly;
}

namespace PCGExCreateSpline
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCreateSpline::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		bClosedLoop = PCGExPaths::GetClosedLoop(PointDataFacade->GetIn());

		TangentsHandler = MakeShared<PCGExTangents::FTangentsHandler>(bClosedLoop);
		if (!TangentsHandler->Init(Context, Context->Tangents, PointDataFacade)) { return false; }

		if (Settings->bApplyCustomPointType)
		{
			CustomPointType = PointDataFacade->GetBroadcaster<int32>(Settings->PointTypeAttribute, true);
			if (!CustomPointType)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Missing custom point type attribute"));
				return false;
			}
		}

		PositionOffset = SplineActor->GetTransform().GetLocation();
		SplineData = Context->ManagedObjects->New<UPCGSplineData>();
		PCGEx::InitArray(SplinePoints, PointDataFacade->GetNum());

		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::CreateSpline::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		const UPCGBasePointData* InPointData = PointDataFacade->GetIn();
		TConstPCGValueRange<FTransform> InTransforms = InPointData->GetConstTransformValueRange();

		PCGEX_SCOPE_LOOP(Index)
		{
			FVector OutArrive = FVector::ZeroVector;
			FVector OutLeave = FVector::ZeroVector;

			TangentsHandler->GetPointTangents(Index, OutArrive, OutLeave);

			const FTransform& TR = InTransforms[Index];

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
	}

	void FProcessor::Output()
	{
		TProcessor<FPCGExCreateSplineContext, UPCGExCreateSplineSettings>::Output();

		// Output spline data
		SplineData->Initialize(SplinePoints, bClosedLoop, FTransform(PositionOffset));

		FPCGTaggedData& StagedData = Context->StageOutput(SplineData, true, false);
		StagedData.Pin = Settings->GetMainOutputPin();
		PointDataFacade->Source->Tags->DumpTo(StagedData.Tags);

		// Output spline component
		if (Settings->Mode != EPCGCreateSplineMode::CreateDataOnly)
		{
			const bool bIsPreviewMode = ExecutionContext->GetComponent()->IsInPreviewMode();

			const FString ComponentName = TEXT("PCGSplineComponent");
			const EObjectFlags ObjectFlags = (bIsPreviewMode ? RF_Transient : RF_NoFlags);
			USplineComponent* SplineComponent = NewObject<USplineComponent>(SplineActor, MakeUniqueObjectName(SplineActor, USplineComponent::StaticClass(), FName(ComponentName)), ObjectFlags);

			PointDataFacade->Source->Tags->DumpTo(SplineComponent->ComponentTags);

			SplineData->ApplyTo(SplineComponent);

			Context->AttachManagedComponent(SplineActor, SplineComponent, Settings->AttachmentRules.GetRules());
			Context->AddNotifyActor(SplineActor);
		}
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExCreateSplineContext, UPCGExCreateSplineSettings>::Cleanup();
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

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPathDeform.h"


#include "Helpers/PCGHelpers.h"
#include "Paths/PCGExPaths.h"


#define LOCTEXT_NAMESPACE "PCGExPathDeformElement"
#define PCGEX_NAMESPACE PathDeform

PCGEX_INITIALIZE_ELEMENT(PathDeform)

TArray<FPCGPinProperties> UPCGExPathDeformSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_ANY(PCGExPathDeform::SourceDeformersLabel, "Paths or splines to deform along", Required, {})
	return PinProperties;
}

void FPCGExPathDeformElement::DisabledPassThroughData(FPCGContext* Context) const
{
	// No passthrough
}

bool FPCGExPathDeformElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(PathDeform)

	if (!Context->Tangents.Init(Context, Settings->Tangents)) { return false; }

	TArray<FPCGTaggedData> Candidates = Context->InputData.GetSpatialInputsByPin(PCGExPathDeform::SourceDeformersLabel);

	Context->Deformers.Init(nullptr, Candidates.Num());
	Context->DeformersData.Reserve(Candidates.Num());
	Context->DeformersFacades.Reserve(Candidates.Num());

	auto RegisterData = [&](const UPCGSpatialData* InData, const FPCGSplineStruct* InStruct, const TSet<FString>& InTags)
	{
		Context->DeformersData.Add(InData);
		Context->Deformers.Add(InStruct);

		const TSharedPtr<PCGExData::FTags> Tags = MakeShared<PCGExData::FTags>(InTags);
		Context->DeformersTags.Add(Tags);
	};

	for (int i = 0; i < Candidates.Num(); ++i)
	{
		FPCGTaggedData& TaggedData = Candidates[i];

		if (const UPCGBasePointData* PointData = Cast<UPCGBasePointData>(TaggedData.Data))
		{
			if (PointData->GetNumPoints() < 2) { continue; }

			TSharedPtr<PCGExData::FPointIO> PointIO = MakeShared<PCGExData::FPointIO>(Context->GetOrCreateHandle(), PointData);
			const TSharedPtr<PCGExData::FFacade> Facade = MakeShared<PCGExData::FFacade>(PointIO.ToSharedRef());
			const TSharedPtr<FPCGSplineStruct> SplineStruct = MakeShared<FPCGSplineStruct>();

			Facade->Idx = Context->DeformersFacades.Add(Facade);
			Context->LocalDeformers.Add(SplineStruct);

			RegisterData(PointData, SplineStruct.Get(), TaggedData.Tags);
			continue;
		}

		if (const UPCGSplineData* SplineData = Cast<UPCGSplineData>(TaggedData.Data))
		{
			if (SplineData->SplineStruct.GetNumberOfPoints() < 2) { continue; }
			RegisterData(SplineData, &SplineData->SplineStruct, TaggedData.Tags);
			continue;
		}
	}

	if (Context->Deformers.IsEmpty())
	{
		return false;
	}

	return true;
}

bool FPCGExPathDeformElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExPathDeformElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(PathDeform)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		// TODO : First batch-build path splines to deform against

		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some input have less than 2 points and will be ignored."))
		if (!Context->StartBatchProcessingPoints<PCGExPathDeform::FBatch>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bHasInvalidInputs = true;
					return false;
				}
				return true;
			},
			[&](const TSharedPtr<PCGExPathDeform::FBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any dataset to generate splines."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExPathDeform
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExPathDeform::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		// TODO : Find which path should be used

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::New)

		TotalLength = Deformer->GetSplineLength();
		
		// TODO : Normalize point data to deform around around it

		// TODO : Alpha

		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::PathDeform::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		const UPCGBasePointData* InPointData = PointDataFacade->GetIn();
		TConstPCGValueRange<FTransform> InTransforms = InPointData->GetConstTransformValueRange();

		PCGEX_SCOPE_LOOP(Index)
		{
		}
	}
	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExPathDeformContext, UPCGExPathDeformSettings>::Cleanup();
	}

	void FBatch::OnInitialPostProcess()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathDeform)

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, BuildSplines)

		BuildSplines->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->OnSplineBuildingComplete();
			};

		BuildSplines->OnIterationCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const int32 Index, const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				This->BuildSpline(Index);
			};

		BuildSplines->StartIterations(Context->DeformersFacades.Num(), 1);
	}

	void FBatch::BuildSpline(const int32 InSplineIndex) const
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(PathDeform)

		TSharedPtr<FPCGSplineStruct> SplineStruct = Context->LocalDeformers[InSplineIndex];
		if (!SplineStruct) { return; }

		TSharedPtr<PCGExData::FFacade> PathFacade = Context->DeformersFacades[InSplineIndex];
		PathFacade->bSupportsScopedGet = false;

		const bool bClosedLoop = PCGExPaths::GetClosedLoop(PathFacade->GetIn());

		TSharedPtr<PCGExTangents::FTangentsHandler> TangentsHandler = MakeShared<PCGExTangents::FTangentsHandler>(bClosedLoop);
		if (!TangentsHandler->Init(Context, Context->Tangents, PathFacade)) { return; }

		TSharedPtr<PCGExData::TBuffer<int32>> CustomPointType;

		if (Settings->bApplyCustomPointType)
		{
			CustomPointType = PathFacade->GetBroadcaster<int32>(Settings->PointTypeAttribute, true);
			if (!CustomPointType)
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("Missing custom point type attribute"));
				return;
			}
		}

		const int32 NumPoints = PathFacade->GetNum();
		TArray<FSplinePoint> SplinePoints;
		SplinePoints.Reserve(NumPoints);

		const UPCGBasePointData* InPointData = PathFacade->GetIn();
		TConstPCGValueRange<FTransform> InTransforms = InPointData->GetConstTransformValueRange();

		for (int i = 0; i < NumPoints; i++)
		{
			FVector OutArrive = FVector::ZeroVector;
			FVector OutLeave = FVector::ZeroVector;

			TangentsHandler->GetSegmentTangents(i, OutArrive, OutLeave);

			const FTransform& TR = InTransforms[i];

			EPCGExSplinePointType PointTypeProxy = Settings->DefaultPointType;
			ESplinePointType::Type PointType = ESplinePointType::Curve;

			if (CustomPointType)
			{
				const int32 Value = CustomPointType->Read(i);
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

			SplinePoints.Emplace(
				static_cast<float>(i),
				TR.GetLocation(),
				OutArrive,
				OutLeave,
				TR.GetRotation().Rotator(),
				TR.GetScale3D(),
				PointType);
		}

		SplineStruct->Initialize(SplinePoints, bClosedLoop, FTransform::Identity);
	}

	void FBatch::OnSplineBuildingComplete()
	{
		TBatch<FProcessor>::OnInitialPostProcess();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

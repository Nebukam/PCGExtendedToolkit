// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestPoint.h"

#include "PCGExFactoryProvider.h"
#include "PCGExPointsProcessor.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNearestPointElement"
#define PCGEX_NAMESPACE SampleNearestPoint

UPCGExSampleNearestPointSettings::UPCGExSampleNearestPointSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (LookAtUpSource.GetName() == FName("@Last")) { LookAtUpSource.Update(TEXT("$Transform.Up")); }
	if (!WeightOverDistance) { WeightOverDistance = PCGEx::WeightDistributionLinearInv; }
}

TArray<FPCGPinProperties> UPCGExSampleNearestPointSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceTargetsLabel, "The point data set to check against.", Required, {})
	PCGEX_PIN_PARAMS(PCGEx::SourcePointFilters, "Filter which points will be processed.", Advanced, {})
	PCGEX_PIN_PARAMS(PCGEx::SourceUseValueIfFilters, "Filter which points values will be processed.", Advanced, {})
	return PinProperties;
}

PCGExData::EInit UPCGExSampleNearestPointSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

int32 UPCGExSampleNearestPointSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_L; }

PCGEX_INITIALIZE_ELEMENT(SampleNearestPoint)

FPCGExSampleNearestPointContext::~FPCGExSampleNearestPointContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE(ValueFilterManager)
	ValueFilterFactories.Empty();

	PCGEX_DELETE(Targets)
}

bool FPCGExSampleNearestPointElement::Boot(FPCGContext* InContext) const
{
	if (!FPCGExPointsProcessorElementBase::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestPoint)

	Context->Targets = Context->TryGetSingleInput(PCGEx::SourceTargetsLabel, true);
	if (!Context->Targets) { return false; }

	TSet<FName> MissingTargetAttributes;
	PCGExDataBlending::AssembleBlendingSettings(
		Settings->bBlendPointProperties ? Settings->PointPropertiesBlendingSettings : FPCGExPropertiesBlendingSettings(EPCGExDataBlendingType::None),
		Settings->TargetAttributes, *Context->Targets, Context->BlendingSettings, MissingTargetAttributes);

	for (const FName Id : MissingTargetAttributes) { PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Missing source attribute on edges: {0}."), FText::FromName(Id))); }

	Context->WeightCurve = Settings->WeightOverDistance.LoadSynchronous();

	if (!Context->WeightCurve)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Weight Curve asset could not be loaded."));
		return false;
	}

	PCGExFactories::GetInputFactories(InContext, PCGEx::SourceUseValueIfFilters, Context->ValueFilterFactories, {PCGExFactories::EType::Filter}, false);

	return true;
}

bool FPCGExSampleNearestPointElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestPointElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestPoint)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		Context->Targets->CreateInKeys();

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleNearestPoints::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExSampleNearestPoints::FProcessor>* NewBatch)
			{
				NewBatch->SetPointsFilterData(&Context->FilterFactories);
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any paths to split."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	if (Context->IsDone())
	{
		Context->OutputMainPoints();
		Context->PostProcessOutputs();
	}

	return Context->IsDone();
}

namespace PCGExSampleNearestPoints
{
	FProcessor::FProcessor(PCGExData::FPointIO* InPoints)
		: FPointsProcessor(InPoints)
	{
		DefaultPointFilterValue = true;
	}

	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(Blender)

		PCGEX_DELETE(RangeMinGetter)
		PCGEX_DELETE(RangeMaxGetter)
		PCGEX_DELETE(LookAtUpGetter)

		PCGEX_FOREACH_FIELD_NEARESTPOINT(PCGEX_OUTPUT_DELETE)
	}

	bool FProcessor::Process(FPCGExAsyncManager* AsyncManager)
	{
		const FPCGExSampleNearestPointContext* TypedContext = GetContext<FPCGExSampleNearestPointContext>();
		PCGEX_SETTINGS(SampleNearestPoint)

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		if (!TypedContext->BlendingSettings.FilteredAttributes.IsEmpty() ||
			!TypedContext->BlendingSettings.GetPropertiesBlendingSettings().HasNoBlending())
		{
			Blender = new PCGExDataBlending::FMetadataBlender(&TypedContext->BlendingSettings);
			Blender->PrepareForData(*PointIO, *TypedContext->Targets);
		}

		RangeMinGetter = new PCGEx::FLocalSingleFieldGetter();
		RangeMinGetter->Capture(Settings->LocalRangeMin);

		RangeMaxGetter = new PCGEx::FLocalSingleFieldGetter();
		RangeMaxGetter->Capture(Settings->LocalRangeMax);

		PCGEX_FOREACH_FIELD_NEARESTPOINT(PCGEX_OUTPUT_FWD)
		PCGEX_FOREACH_FIELD_NEARESTPOINT(PCGEX_OUTPUT_VALIDATE_NAME)

		LookAtUpGetter = new PCGEx::FLocalVectorGetter();
		LookAtUpGetter->Capture(Settings->LookAtUpSource);

		if (Settings->bWriteLookAtTransform && Settings->LookAtUpSelection != EPCGExSampleSource::Constant)
		{
			if (Settings->LookAtUpSelection == EPCGExSampleSource::Target)
			{
				if (!LookAtUpGetter->Grab(*TypedContext->Targets)) { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("LookUp is invalid on target.")); }
			}
		}

		if (Settings->bUseLocalRangeMin)
		{
			if (RangeMinGetter->Grab(*PointIO)) { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("RangeMin metadata missing")); }
		}

		if (Settings->bUseLocalRangeMax)
		{
			if (RangeMaxGetter->Grab(*PointIO)) { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("RangeMax metadata missing")); }
		}

		if (Settings->bWriteLookAtTransform)
		{
			if (Settings->LookAtUpSelection == EPCGExSampleSource::Source &&
				!LookAtUpGetter->Grab(*PointIO))
			{
				PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("LookUp is invalid on source."));
			}
		}

		PCGEX_FOREACH_FIELD_NEARESTPOINT(PCGEX_OUTPUT_ACCESSOR_INIT_PTR)

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessSinglePoint(int32 Index, FPCGPoint& Point)
	{
		if (!PointFilterCache[Index]) { return; }

		const FPCGExSampleNearestPointContext* TypedContext = GetContext<FPCGExSampleNearestPointContext>();
		PCGEX_SETTINGS(SampleNearestPoint)

		const bool bSingleSample = (Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget || Settings->SampleMethod == EPCGExSampleMethod::FarthestTarget);

		const TArray<FPCGPoint>& TargetPoints = TypedContext->Targets->GetIn()->GetPoints();
		const int32 NumTargets = TargetPoints.Num();
		const FVector SourceCenter = Point.Transform.GetLocation();

		double RangeMin = FMath::Pow(RangeMinGetter->SafeGet(Index, Settings->RangeMin), 2);
		double RangeMax = FMath::Pow(RangeMaxGetter->SafeGet(Index, Settings->RangeMax), 2);

		if (RangeMin > RangeMax) { std::swap(RangeMin, RangeMax); }

		TArray<PCGExNearestPoint::FTargetInfos> TargetsInfos;
		TargetsInfos.Reserve(TypedContext->Targets->GetNum());

		PCGExNearestPoint::FTargetsCompoundInfos TargetsCompoundInfos;
		auto ProcessTarget = [&](const int32 PointIndex, const FPCGPoint& Target)
		{
			//if (Context->ValueFilterManager && !Context->ValueFilterManager->Results[PointIndex]) { return; } // TODO : Implement

			FVector A;
			FVector B;

			Settings->DistanceSettings.GetCenters(Point, Target, A, B);

			const double Dist = FVector::DistSquared(A, B);

			if (RangeMax > 0 && (Dist < RangeMin || Dist > RangeMax)) { return; }

			if (Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget ||
				Settings->SampleMethod == EPCGExSampleMethod::FarthestTarget)
			{
				TargetsCompoundInfos.UpdateCompound(PCGExNearestPoint::FTargetInfos(PointIndex, Dist));
			}
			else
			{
				const PCGExNearestPoint::FTargetInfos& Infos = TargetsInfos.Emplace_GetRef(PointIndex, Dist);
				TargetsCompoundInfos.UpdateCompound(Infos);
			}
		};

		if (RangeMax > 0)
		{
			const FBox Box = FBoxCenterAndExtent(SourceCenter, FVector(FMath::Sqrt(RangeMax))).GetBox();
			auto ProcessNeighbor = [&](const FPCGPointRef& InPointRef)
			{
				const ptrdiff_t PointIndex = InPointRef.Point - TargetPoints.GetData();
				if (!TargetPoints.IsValidIndex(PointIndex)) { return; }

				ProcessTarget(PointIndex, TargetPoints[PointIndex]);
			};

			const UPCGPointData::PointOctree& Octree = TypedContext->Targets->GetIn()->GetOctree();
			Octree.FindElementsWithBoundsTest(Box, ProcessNeighbor);
		}
		else
		{
			for (int i = 0; i < NumTargets; i++) { ProcessTarget(i, TargetPoints[i]); }
		}

		// Compound never got updated, meaning we couldn't find target in range
		if (TargetsCompoundInfos.UpdateCount <= 0)
		{
			double FailSafeDist = FMath::Sqrt(RangeMax);
			PCGEX_OUTPUT_VALUE(Success, Index, false)
			PCGEX_OUTPUT_VALUE(Transform, Index, Point.Transform)
			PCGEX_OUTPUT_VALUE(LookAtTransform, Index, Point.Transform)
			PCGEX_OUTPUT_VALUE(Distance, Index, FailSafeDist)
			PCGEX_OUTPUT_VALUE(SignedDistance, Index, FailSafeDist)
			PCGEX_OUTPUT_VALUE(NumSamples, Index, 0)
			return;
		}

		// Compute individual target weight
		if (Settings->WeightMethod == EPCGExRangeType::FullRange && RangeMax > 0)
		{
			// Reset compounded infos to full range
			TargetsCompoundInfos.SampledRangeMin = RangeMin;
			TargetsCompoundInfos.SampledRangeMax = RangeMax;
			TargetsCompoundInfos.SampledRangeWidth = RangeMax - RangeMin;
		}

		FTransform WeightedTransform = FTransform::Identity;
		WeightedTransform.SetScale3D(FVector::ZeroVector);
		FVector WeightedUp = Settings->LookAtUpSelection == EPCGExSampleSource::Source ?
			                     LookAtUpGetter->SafeGet(Index, SafeUpVector) : SafeUpVector;
		FVector WeightedSignAxis = FVector::Zero();
		FVector WeightedAngleAxis = FVector::Zero();
		double TotalWeight = 0;


		auto ProcessTargetInfos = [&]
			(const PCGExNearestPoint::FTargetInfos& TargetInfos, const double Weight)
		{
			const FPCGPoint& Target = TypedContext->Targets->GetInPoint(TargetInfos.Index);

			const FTransform TargetTransform = Target.Transform;
			const FQuat TargetRotation = TargetTransform.GetRotation();

			WeightedTransform.SetRotation(WeightedTransform.GetRotation() + (TargetRotation * Weight));
			WeightedTransform.SetScale3D(WeightedTransform.GetScale3D() + (TargetTransform.GetScale3D() * Weight));
			WeightedTransform.SetLocation(WeightedTransform.GetLocation() + (TargetTransform.GetLocation() * Weight));

			if (Settings->LookAtUpSelection == EPCGExSampleSource::Target) { WeightedUp += LookAtUpGetter->SafeGet(TargetInfos.Index, SafeUpVector) * Weight; }

			WeightedSignAxis += PCGExMath::GetDirection(TargetRotation, Settings->SignAxis) * Weight;
			WeightedAngleAxis += PCGExMath::GetDirection(TargetRotation, Settings->AngleAxis) * Weight;

			TotalWeight += Weight;

			if (Blender) { Blender->Blend(Index, TargetInfos.Index, Index, Weight); }
		};

		if (Blender) { Blender->PrepareForBlending(Index, &Point); }

		if (bSingleSample)
		{
			const PCGExNearestPoint::FTargetInfos& TargetInfos = Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget ? TargetsCompoundInfos.Closest : TargetsCompoundInfos.Farthest;
			const double Weight = TypedContext->WeightCurve->GetFloatValue(TargetsCompoundInfos.GetRangeRatio(TargetInfos.Distance));
			ProcessTargetInfos(TargetInfos, Weight);
		}
		else
		{
			for (PCGExNearestPoint::FTargetInfos& TargetInfos : TargetsInfos)
			{
				const double Weight = TypedContext->WeightCurve->GetFloatValue(TargetsCompoundInfos.GetRangeRatio(TargetInfos.Distance));
				if (Weight == 0) { continue; }
				ProcessTargetInfos(TargetInfos, Weight);
			}
		}

		double Count = bSingleSample ? 1 : TargetsInfos.Num();

		if (Blender) { Blender->CompleteBlending(Index, Count, TotalWeight); }

		if (TotalWeight != 0) // Dodge NaN
		{
			WeightedUp /= TotalWeight;

			WeightedTransform.SetRotation(WeightedTransform.GetRotation() / TotalWeight);
			WeightedTransform.SetScale3D(WeightedTransform.GetScale3D() / TotalWeight);
			WeightedTransform.SetLocation(WeightedTransform.GetLocation() / TotalWeight);
		}

		WeightedUp.Normalize();

		FVector LookAt = (Point.Transform.GetLocation() - WeightedTransform.GetLocation()).GetSafeNormal();
		const double WeightedDistance = FVector::Dist(Point.Transform.GetLocation(), WeightedTransform.GetLocation());

		PCGEX_OUTPUT_VALUE(Success, Index, TargetsCompoundInfos.IsValid())
		PCGEX_OUTPUT_VALUE(Transform, Index, WeightedTransform)
		PCGEX_OUTPUT_VALUE(LookAtTransform, Index, PCGExMath::MakeLookAtTransform(LookAt, WeightedUp, Settings->LookAtAxisAlign))
		PCGEX_OUTPUT_VALUE(Distance, Index, WeightedDistance)
		PCGEX_OUTPUT_VALUE(SignedDistance, Index, FMath::Sign(WeightedSignAxis.Dot(LookAt)) * WeightedDistance)
		PCGEX_OUTPUT_VALUE(Angle, Index, PCGExSampling::GetAngle(Settings->AngleRange, WeightedAngleAxis, LookAt))
		PCGEX_OUTPUT_VALUE(NumSamples, Index, Count)
	}

	void FProcessor::CompleteWork()
	{
		FPointsProcessor::CompleteWork();
		PCGEX_FOREACH_FIELD_NEARESTPOINT(PCGEX_OUTPUT_WRITE)
		if (Blender) { Blender->Write(); }
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

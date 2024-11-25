// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestPoint.h"


#include "PCGExPointsProcessor.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Misc/PCGExSortPoints.h"


#define LOCTEXT_NAMESPACE "PCGExSampleNearestPointElement"
#define PCGEX_NAMESPACE SampleNearestPoint

namespace PCGExNearestPoint
{
	void FSamplesStats::Update(const FSample& InSample)
	{
		UpdateCount++;

		if (InSample.Distance < SampledRangeMin)
		{
			Closest = InSample;
			SampledRangeMin = InSample.Distance;
		}

		if (InSample.Distance > SampledRangeMax)
		{
			Farthest = InSample;
			SampledRangeMax = InSample.Distance;
		}

		SampledRangeWidth = SampledRangeMax - SampledRangeMin;
	}

	void FSamplesStats::Replace(const FSample& InSample)
	{
		UpdateCount++;

		Closest = InSample;
		SampledRangeMin = InSample.Distance;
		Farthest = InSample;
		SampledRangeMax = InSample.Distance;

		SampledRangeWidth = SampledRangeMax - SampledRangeMin;
	}
}

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
	if (SampleMethod == EPCGExSampleMethod::BestCandidate)
	{
		PCGEX_PIN_PARAMS(PCGExSorting::SourceSortingRules, "Plug sorting rules here. Order is defined by each rule' priority value, in ascending order.", Required, {})
	}
	PCGEX_PIN_PARAMS(PCGEx::SourceUseValueIfFilters, "Filter which points values will be processed.", Advanced, {})
	return PinProperties;
}

PCGExData::EIOInit UPCGExSampleNearestPointSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

int32 UPCGExSampleNearestPointSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_L; }

PCGEX_INITIALIZE_ELEMENT(SampleNearestPoint)

bool FPCGExSampleNearestPointElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestPoint)

	Context->TargetsFacade = PCGExData::TryGetSingleFacade(Context, PCGEx::SourceTargetsLabel, true);
	if (!Context->TargetsFacade) { return false; }

	Context->TargetsPreloader = MakeShared<PCGExData::FFacadePreloader>();

	TSet<FName> MissingTargetAttributes;
	PCGExDataBlending::AssembleBlendingDetails(
		Settings->bBlendPointProperties ? Settings->PointPropertiesBlendingSettings : FPCGExPropertiesBlendingDetails(EPCGExDataBlendingType::None),
		Settings->TargetAttributes, Context->TargetsFacade->Source, Context->BlendingDetails, MissingTargetAttributes);

	for (const FName Id : MissingTargetAttributes) { PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Missing source attribute on targets: {0}."), FText::FromName(Id))); }


	PCGEX_FOREACH_FIELD_NEARESTPOINT(PCGEX_OUTPUT_VALIDATE_NAME)

	Context->WeightCurve = Settings->WeightOverDistance.LoadSynchronous();
	if (!Context->WeightCurve)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Weight Curve asset could not be loaded."));
		return false;
	}

	Context->DistanceDetails = Settings->DistanceDetails.MakeDistances();
	Context->TargetPoints = &Context->TargetsFacade->Source->GetIn()->GetPoints();
	Context->NumTargets = Context->TargetPoints->Num();

	Context->TargetOctree = &Context->TargetsFacade->Source->GetIn()->GetOctree();

	if (Settings->WeightMode != EPCGExSampleWeightMode::Distance)
	{
		Context->TargetWeights = Context->TargetsFacade->GetBroadcaster<double>(Settings->WeightAttribute);
		if (!Context->TargetWeights)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Weight Attribute on Targets is invalid."));
			return false;
		}
	}

	if (Settings->SampleMethod == EPCGExSampleMethod::BestCandidate)
	{
		Context->Sorter = MakeShared<PCGExSorting::PointSorter<false>>(Context, Context->TargetsFacade.ToSharedRef(), PCGExSorting::GetSortingRules(Context, PCGExSorting::SourceSortingRules));
		Context->Sorter->SortDirection = Settings->SortDirection;
		Context->Sorter->RegisterBuffersDependencies(*Context->TargetsPreloader);
	}

	Context->BlendingDetails.RegisterBuffersDependencies(Context, Context->TargetsFacade, *Context->TargetsPreloader);

	return true;
}

bool FPCGExSampleNearestPointElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestPointElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestPoint)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->SetAsyncState(PCGEx::State_FacadePreloading);
		Context->TargetsPreloader->OnCompleteCallback = [Settings, Context]()
		{
			if (Context->Sorter && !Context->Sorter->Init())
			{
				Context->CancelExecution(TEXT("Invalid sort rules"));
				return;
			}

			if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleNearestPoints::FProcessor>>(
				[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
				[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSampleNearestPoints::FProcessor>>& NewBatch)
				{
					if (Settings->bPruneFailedSamples) { NewBatch->bRequiresWriteStep = true; }
				}))
			{
				Context->CancelExecution(TEXT("Could not find any points to sample."));
			}
		};

		Context->TargetsPreloader->StartLoading(Context->GetAsyncManager(), Context->TargetsFacade.ToSharedRef());
		return false;
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSampleNearestPoints
{
	FProcessor::~FProcessor()
	{
	}

	void FProcessor::SamplingFailed(const int32 Index, const FPCGPoint& Point)
	{
		SampleState[Index] = false;
		
		const double FailSafeDist = RangeMaxGetter ? FMath::Sqrt(RangeMaxGetter->Read(Index)) : Settings->RangeMax;
		PCGEX_OUTPUT_VALUE(Success, Index, false)
		PCGEX_OUTPUT_VALUE(Transform, Index, Point.Transform)
		PCGEX_OUTPUT_VALUE(LookAtTransform, Index, Point.Transform)
		PCGEX_OUTPUT_VALUE(Distance, Index, FailSafeDist)
		PCGEX_OUTPUT_VALUE(SignedDistance, Index, FailSafeDist)
		PCGEX_OUTPUT_VALUE(NumSamples, Index, 0)
		PCGEX_OUTPUT_VALUE(SampledIndex, Index, -1)
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNearestPoints::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		SampleState.SetNumUninitialized(PointDataFacade->GetNum());
		
		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_NEARESTPOINT(PCGEX_OUTPUT_INIT)
		}

		if (!Context->BlendingDetails.FilteredAttributes.IsEmpty() ||
			!Context->BlendingDetails.GetPropertiesBlendingDetails().HasNoBlending())
		{
			Blender = MakeShared<PCGExDataBlending::FMetadataBlender>(&Context->BlendingDetails);
			Blender->PrepareForData(PointDataFacade, Context->TargetsFacade.ToSharedRef());
		}

		if (Settings->bWriteLookAtTransform && Settings->LookAtUpSelection != EPCGExSampleSource::Constant)
		{
			if (Settings->LookAtUpSelection == EPCGExSampleSource::Target) { LookAtUpGetter = Context->TargetsFacade->GetScopedBroadcaster<FVector>(Settings->LookAtUpSource); }
			else { LookAtUpGetter = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->LookAtUpSource); }

			if (!LookAtUpGetter) { PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("LookAtUp is invalid.")); }
		}

		if (Settings->bUseLocalRangeMin)
		{
			RangeMinGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->LocalRangeMin);
			if (!RangeMinGetter) { PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("RangeMin metadata missing")); }
		}
		if (Settings->bUseLocalRangeMax)
		{
			RangeMaxGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->LocalRangeMax);
			if (!RangeMaxGetter) { PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("RangeMax metadata missing")); }
		}

		bSingleSample = Settings->SampleMethod != EPCGExSampleMethod::WithinRange;
		bSampleClosest = Settings->SampleMethod == EPCGExSampleMethod::ClosestTarget || Settings->SampleMethod == EPCGExSampleMethod::BestCandidate;

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
		if (!PointFilterCache[Index])
		{
			if (Settings->bProcessFilteredOutAsFails) { SamplingFailed(Index, Point); }
			return;
		}

		const FVector SourceCenter = Point.Transform.GetLocation();

		double RangeMin = FMath::Square(RangeMaxGetter ? RangeMinGetter->Read(Index) : Settings->RangeMin);
		double RangeMax = FMath::Square(RangeMaxGetter ? RangeMaxGetter->Read(Index) : Settings->RangeMax);

		if (RangeMin > RangeMax) { std::swap(RangeMin, RangeMax); }

		TArray<PCGExNearestPoint::FSample> Samples;
		PCGExNearestPoint::FSamplesStats Stats;

		auto SampleTarget = [&](const int32 TargetPtIndex, const FPCGPoint& Target)
		{
			//if (Context->ValueFilterManager && !Context->ValueFilterManager->Results[PointIndex]) { return; } // TODO : Implement

			FVector A;
			FVector B;

			Context->DistanceDetails->GetCenters(Point, Target, A, B);

			double Dist = FVector::DistSquared(A, B);

			if (RangeMax > 0 && (Dist < RangeMin || Dist > RangeMax)) { return; }

			if (Settings->WeightMode == EPCGExSampleWeightMode::Attribute) { Dist = Context->TargetWeights->Read(TargetPtIndex); }
			else if (Settings->WeightMode == EPCGExSampleWeightMode::AttributeMult) { Dist *= Context->TargetWeights->Read(TargetPtIndex); }

			if (bSingleSample)
			{
				if (Settings->SampleMethod == EPCGExSampleMethod::BestCandidate && Stats.IsValid())
				{
					if (!Context->Sorter->Sort(TargetPtIndex, Stats.Closest.Index)) { return; }
					Stats.Replace(PCGExNearestPoint::FSample(TargetPtIndex, Dist));
				}
				else
				{
					Stats.Update(PCGExNearestPoint::FSample(TargetPtIndex, Dist));
				}
			}
			else
			{
				const PCGExNearestPoint::FSample& Infos = Samples.Emplace_GetRef(TargetPtIndex, Dist);
				Stats.Update(Infos);
			}
		};

		if (RangeMax > 0)
		{
			const FBox Box = FBoxCenterAndExtent(SourceCenter, FVector(FMath::Sqrt(RangeMax))).GetBox();
			auto ProcessNeighbor = [&](const FPCGPointRef& InPointRef)
			{
				const ptrdiff_t TargetPtIndex = InPointRef.Point - Context->TargetPoints->GetData();
				SampleTarget(TargetPtIndex, *(Context->TargetPoints->GetData() + TargetPtIndex));
			};

			Context->TargetOctree->FindElementsWithBoundsTest(Box, ProcessNeighbor);
		}
		else
		{
			Samples.Reserve(Context->NumTargets);
			for (int i = 0; i < Context->NumTargets; i++) { SampleTarget(i, *(Context->TargetPoints->GetData() + i)); }
		}

		// Compound never got updated, meaning we couldn't find target in range
		if (Stats.UpdateCount <= 0)
		{
			SamplingFailed(Index, Point);
			return;
		}

		// Compute individual target weight
		if (Settings->WeightMethod == EPCGExRangeType::FullRange && RangeMax > 0)
		{
			// Reset compounded infos to full range
			Stats.SampledRangeMin = RangeMin;
			Stats.SampledRangeMax = FMath::Max(Stats.SampledRangeMax, RangeMax);
			Stats.SampledRangeWidth = RangeMax - RangeMin;
		}

		FTransform WeightedTransform = FTransform::Identity;
		WeightedTransform.SetScale3D(FVector::ZeroVector);
		FVector WeightedUp = SafeUpVector;
		if (Settings->LookAtUpSelection == EPCGExSampleSource::Source && LookAtUpGetter) { WeightedUp = LookAtUpGetter->Read(Index); }

		FVector WeightedSignAxis = FVector::Zero();
		FVector WeightedAngleAxis = FVector::Zero();
		double TotalWeight = 0;
		double TotalSamples = 0;

		auto ProcessTargetInfos = [&]
			(const PCGExNearestPoint::FSample& TargetInfos, const double Weight)
		{
			const FPCGPoint& Target = Context->TargetsFacade->Source->GetInPoint(TargetInfos.Index);

			const FTransform TargetTransform = Target.Transform;
			const FQuat TargetRotation = TargetTransform.GetRotation();

			WeightedTransform.SetRotation(WeightedTransform.GetRotation() + (TargetRotation * Weight));
			WeightedTransform.SetScale3D(WeightedTransform.GetScale3D() + (TargetTransform.GetScale3D() * Weight));
			WeightedTransform.SetLocation(WeightedTransform.GetLocation() + (TargetTransform.GetLocation() * Weight));

			if (Settings->LookAtUpSelection == EPCGExSampleSource::Target) { WeightedUp += (LookAtUpGetter ? LookAtUpGetter->Read(TargetInfos.Index) : SafeUpVector) * Weight; }

			WeightedSignAxis += PCGExMath::GetDirection(TargetRotation, Settings->SignAxis) * Weight;
			WeightedAngleAxis += PCGExMath::GetDirection(TargetRotation, Settings->AngleAxis) * Weight;

			TotalWeight += Weight;
			TotalSamples++;

			if (Blender) { Blender->Blend(Index, TargetInfos.Index, Index, Weight); }
		};

		if (Blender) { Blender->PrepareForBlending(Index, &Point); }

		if (bSingleSample)
		{
			const PCGExNearestPoint::FSample& TargetInfos = bSampleClosest ? Stats.Closest : Stats.Farthest;
			const double Weight = Context->WeightCurve->GetFloatValue(Stats.GetRangeRatio(TargetInfos.Distance));
			ProcessTargetInfos(TargetInfos, Weight);
		}
		else
		{
			for (PCGExNearestPoint::FSample& TargetInfos : Samples)
			{
				const double Weight = Context->WeightCurve->GetFloatValue(Stats.GetRangeRatio(TargetInfos.Distance));
				if (Weight == 0) { continue; }
				ProcessTargetInfos(TargetInfos, Weight);
			}
		}

		if (Blender) { Blender->CompleteBlending(Index, TotalSamples, TotalWeight); }

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

		SampleState[Index] = Stats.IsValid();
		PCGEX_OUTPUT_VALUE(Success, Index, Stats.IsValid())
		PCGEX_OUTPUT_VALUE(Transform, Index, WeightedTransform)
		PCGEX_OUTPUT_VALUE(LookAtTransform, Index, PCGExMath::MakeLookAtTransform(LookAt, WeightedUp, Settings->LookAtAxisAlign))
		PCGEX_OUTPUT_VALUE(Distance, Index, WeightedDistance)
		PCGEX_OUTPUT_VALUE(SignedDistance, Index, FMath::Sign(WeightedSignAxis.Dot(LookAt)) * WeightedDistance)
		PCGEX_OUTPUT_VALUE(Angle, Index, PCGExSampling::GetAngle(Settings->AngleRange, WeightedAngleAxis, LookAt))
		PCGEX_OUTPUT_VALUE(NumSamples, Index, TotalSamples)
		PCGEX_OUTPUT_VALUE(SampledIndex, Index, Stats.IsValid() ? Stats.Closest.Index : -1)

		FPlatformAtomics::InterlockedExchange(&bAnySuccess, 1);
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);

		if (Settings->bTagIfHasSuccesses && bAnySuccess) { PointDataFacade->Source->Tags->Add(Settings->HasSuccessesTag); }
		if (Settings->bTagIfHasNoSuccesses && !bAnySuccess) { PointDataFacade->Source->Tags->Add(Settings->HasNoSuccessesTag); }
	}

	void FProcessor::Write()
	{
		PCGExSampling::PruneFailedSamples(PointDataFacade->GetMutablePoints(), SampleState);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

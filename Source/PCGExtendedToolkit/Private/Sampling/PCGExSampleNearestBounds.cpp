// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestBounds.h"

#include "PCGExPointsProcessor.h"
#include "Data/Blending/PCGExMetadataBlender.h"
#include "Graph/PCGExCluster.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNearestBoundsElement"
#define PCGEX_NAMESPACE SampleNearestBounds

UPCGExSampleNearestBoundsSettings::UPCGExSampleNearestBoundsSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (LookAtUpSource.GetName() == FName("@Last")) { LookAtUpSource.Update(TEXT("$Transform.Up")); }
	if (!WeightRemap) { WeightRemap = PCGEx::WeightDistributionLinearInv; }
}

TArray<FPCGPinProperties> UPCGExSampleNearestBoundsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_POINT(PCGEx::SourceBoundsLabel, "The bounds data set to check against.", Required, {})
	PCGEX_PIN_PARAMS(PCGEx::SourcePointFilters, "Filter which points will be processed.", Advanced, {})
	PCGEX_PIN_PARAMS(PCGEx::SourceUseValueIfFilters, "Filter which points values will be processed.", Advanced, {})
	return PinProperties;
}

PCGExData::EInit UPCGExSampleNearestBoundsSettings::GetMainOutputInitMode() const { return PCGExData::EInit::DuplicateInput; }

int32 UPCGExSampleNearestBoundsSettings::GetPreferredChunkSize() const { return PCGExMT::GAsyncLoop_L; }

FName UPCGExSampleNearestBoundsSettings::GetPointFilterLabel() const { return PCGExPointFilter::SourceFiltersLabel; }

PCGEX_INITIALIZE_ELEMENT(SampleNearestBounds)

FPCGExSampleNearestBoundsContext::~FPCGExSampleNearestBoundsContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_CLEAN_SP(WeightCurve)

	if (BoundsFacade)
	{
		PCGEX_DELETE(BoundsFacade->Source)
		PCGEX_DELETE(BoundsFacade)
	}
}

bool FPCGExSampleNearestBoundsElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestBounds)

	PCGExData::FPointIO* Bounds = PCGExData::TryGetSingleInput(Context, PCGEx::SourceBoundsLabel, true);
	if (!Bounds)
	{
		PCGEX_DELETE(Bounds)
		return false;
	}

	Context->BoundsFacade = new PCGExData::FFacade(Bounds);

	TSet<FName> MissingTargetAttributes;
	PCGExDataBlending::AssembleBlendingDetails(
		Settings->bBlendPointProperties ? Settings->PointPropertiesBlendingSettings : FPCGExPropertiesBlendingDetails(EPCGExDataBlendingType::None),
		Settings->TargetAttributes, Context->BoundsFacade->Source, Context->BlendingDetails, MissingTargetAttributes);

	for (const FName Id : MissingTargetAttributes) { PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(FTEXT("Missing source attribute on targets: {0}."), FText::FromName(Id))); }


	PCGEX_FOREACH_FIELD_NEARESTBOUNDS(PCGEX_OUTPUT_VALIDATE_NAME)

	Context->WeightCurve = Settings->WeightRemap.LoadSynchronous();

	if (!Context->WeightCurve)
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Weight Curve asset could not be loaded."));
		return false;
	}

	Context->BoundsPoints = &Context->BoundsFacade->Source->GetIn()->GetPoints();

	return true;
}

bool FPCGExSampleNearestBoundsElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestBoundsElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestBounds)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		Context->BoundsFacade->Source->CreateInKeys();

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleNearestBounds::FProcessor>>(
			[&](PCGExData::FPointIO* Entry) { return true; },
			[&](PCGExPointsMT::TBatch<PCGExSampleNearestBounds::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any points to sample."));
			return true;
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExSampleNearestBounds
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE(Blender)
	}

	void FProcessor::SamplingFailed(const int32 Index, const FPCGPoint& Point) const
	{
		constexpr double FailSafeDist = -1;
		PCGEX_OUTPUT_VALUE(Success, Index, false)
		PCGEX_OUTPUT_VALUE(Transform, Index, Point.Transform)
		PCGEX_OUTPUT_VALUE(LookAtTransform, Index, Point.Transform)
		PCGEX_OUTPUT_VALUE(Distance, Index, FailSafeDist)
		PCGEX_OUTPUT_VALUE(SignedDistance, Index, FailSafeDist)
		PCGEX_OUTPUT_VALUE(NumSamples, Index, 0)
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNearestBounds::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(SampleNearestBounds)

		LocalTypedContext = TypedContext;
		LocalSettings = Settings;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		{
			PCGExData::FFacade* OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_NEARESTBOUNDS(PCGEX_OUTPUT_INIT)
		}

		if (!TypedContext->BlendingDetails.FilteredAttributes.IsEmpty() ||
			!TypedContext->BlendingDetails.GetPropertiesBlendingDetails().HasNoBlending())
		{
			Blender = new PCGExDataBlending::FMetadataBlender(&TypedContext->BlendingDetails);
			Blender->PrepareForData(PointDataFacade, TypedContext->BoundsFacade);
		}

		if (Settings->bWriteLookAtTransform && Settings->LookAtUpSelection != EPCGExSampleSource::Constant)
		{
			if (Settings->LookAtUpSelection == EPCGExSampleSource::Target) { LookAtUpGetter = TypedContext->BoundsFacade->GetScopedBroadcaster<FVector>(Settings->LookAtUpSource); }
			else { LookAtUpGetter = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->LookAtUpSource); }

			if (!LookAtUpGetter) { PCGE_LOG_C(Warning, GraphAndLog, Context, FTEXT("LookAtUp is invalid.")); }
		}

		bSingleSample = LocalSettings->SampleMethod != EPCGExBoundsSampleMethod::WithinRange;

		Cloud = LocalTypedContext->BoundsFacade->GetCloud(Settings->BoundsSource);
		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 Count)
	{
		if (!PointFilterCache[Index])
		{
			SamplingFailed(Index, Point);
			return;
		}

		TArray<PCGExNearestBounds::FTargetInfos> TargetsInfos;
		//TargetsInfos.Reserve(TypedContext->Targets->GetNum());


		PCGExNearestBounds::FTargetsCompoundInfos TargetsCompoundInfos;
		PCGExGeo::FSample CurrentSample;

		const FBoxCenterAndExtent BCAE = FBoxCenterAndExtent(Point.Transform.GetLocation(), PCGExMath::GetLocalBounds(Point, BoundsSource).GetExtent());
		Cloud->GetOctree()->FindElementsWithBoundsTest(
			BCAE, [&](const PCGExGeo::FPointBox* NearbyBox)
			{
				NearbyBox->Sample(Point, CurrentSample);
				CurrentSample.Weight = LocalTypedContext->WeightCurve->GetFloatValue(CurrentSample.Weight);

				if (!CurrentSample.bIsInside) { return; }

				if (bSingleSample)
				{
					TargetsCompoundInfos.UpdateCompound(PCGExNearestBounds::FTargetInfos(CurrentSample, NearbyBox->Len));
				}
				else
				{
					const PCGExNearestBounds::FTargetInfos& Infos = TargetsInfos.Emplace_GetRef(CurrentSample, NearbyBox->Len);
					TargetsCompoundInfos.UpdateCompound(Infos);
				}
			});

		// Compound never got updated, meaning we couldn't find target in range
		if (TargetsCompoundInfos.UpdateCount <= 0)
		{
			SamplingFailed(Index, Point);
			return;
		}

		FTransform WeightedTransform = FTransform::Identity;
		WeightedTransform.SetScale3D(FVector::ZeroVector);
		FVector WeightedUp = SafeUpVector;
		if (LocalSettings->LookAtUpSelection == EPCGExSampleSource::Source && LookAtUpGetter) { WeightedUp = LookAtUpGetter->Values[Index]; }

		FVector WeightedSignAxis = FVector::Zero();
		FVector WeightedAngleAxis = FVector::Zero();
		double TotalWeight = 0;
		double TotalSamples = 0;

		auto ProcessTargetInfos = [&]
			(const PCGExNearestBounds::FTargetInfos& TargetInfos)
		{
			const double Weight = TargetInfos.Weight;
			const FPCGPoint& Target = LocalTypedContext->BoundsFacade->Source->GetInPoint(TargetInfos.Index);

			const FTransform TargetTransform = Target.Transform;
			const FQuat TargetRotation = TargetTransform.GetRotation();

			WeightedTransform.SetRotation(WeightedTransform.GetRotation() + (TargetRotation * Weight));
			WeightedTransform.SetScale3D(WeightedTransform.GetScale3D() + (TargetTransform.GetScale3D() * Weight));
			WeightedTransform.SetLocation(WeightedTransform.GetLocation() + (TargetTransform.GetLocation() * Weight));

			if (LocalSettings->LookAtUpSelection == EPCGExSampleSource::Target) { WeightedUp += (LookAtUpGetter ? LookAtUpGetter->Values[TargetInfos.Index] : SafeUpVector) * Weight; }

			WeightedSignAxis += PCGExMath::GetDirection(TargetRotation, LocalSettings->SignAxis) * Weight;
			WeightedAngleAxis += PCGExMath::GetDirection(TargetRotation, LocalSettings->AngleAxis) * Weight;

			TotalWeight += Weight;
			TotalSamples++;

			if (Blender) { Blender->Blend(Index, TargetInfos.Index, Index, Weight); }
		};

		if (Blender) { Blender->PrepareForBlending(Index, &Point); }

		if (bSingleSample)
		{
			switch (LocalSettings->SampleMethod)
			{
			default: ;
			case EPCGExBoundsSampleMethod::WithinRange:
				// Ignore
				break;
			case EPCGExBoundsSampleMethod::ClosestBounds:
				ProcessTargetInfos(TargetsCompoundInfos.Closest);
				break;
			case EPCGExBoundsSampleMethod::FarthestBounds:
				ProcessTargetInfos(TargetsCompoundInfos.Farthest);
				break;
			case EPCGExBoundsSampleMethod::LargestBounds:
				ProcessTargetInfos(TargetsCompoundInfos.Largest);
				break;
			case EPCGExBoundsSampleMethod::SmallestBounds:
				ProcessTargetInfos(TargetsCompoundInfos.Smallest);
				break;
			}
		}
		else
		{
			for (PCGExNearestBounds::FTargetInfos& TargetInfos : TargetsInfos)
			{
				if (TargetInfos.Weight == 0) { continue; }
				ProcessTargetInfos(TargetInfos);
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

		PCGEX_OUTPUT_VALUE(Success, Index, TargetsCompoundInfos.IsValid())
		PCGEX_OUTPUT_VALUE(Transform, Index, WeightedTransform)
		PCGEX_OUTPUT_VALUE(LookAtTransform, Index, PCGExMath::MakeLookAtTransform(LookAt, WeightedUp, LocalSettings->LookAtAxisAlign))
		PCGEX_OUTPUT_VALUE(Distance, Index, WeightedDistance)
		PCGEX_OUTPUT_VALUE(SignedDistance, Index, FMath::Sign(WeightedSignAxis.Dot(LookAt)) * WeightedDistance)
		PCGEX_OUTPUT_VALUE(Angle, Index, PCGExSampling::GetAngle(LocalSettings->AngleRange, WeightedAngleAxis, LookAt))
		PCGEX_OUTPUT_VALUE(NumSamples, Index, TotalSamples)
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManagerPtr, true);
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

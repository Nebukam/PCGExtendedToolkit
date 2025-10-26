﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleNearestSurface.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Components/PrimitiveComponent.h"
#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointIO.h"

#define LOCTEXT_NAMESPACE "PCGExSampleNearestSurfaceElement"
#define PCGEX_NAMESPACE SampleNearestSurface

TArray<FPCGPinProperties> UPCGExSampleNearestSurfaceSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (SurfaceSource == EPCGExSurfaceSource::ActorReferences) { PCGEX_PIN_POINT(PCGExSampling::SourceActorReferencesLabel, "Points with actor reference paths.", Required) }
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(SampleNearestSurface)

PCGExData::EIOInit UPCGExSampleNearestSurfaceSettings::GetIOPreInitForMainPoints() const{ return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(SampleNearestSurface)

bool FPCGExSampleNearestSurfaceElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestSurface)

	PCGEX_FWD(ApplySampling)
	Context->ApplySampling.Init();

	PCGEX_FOREACH_FIELD_NEARESTSURFACE(PCGEX_OUTPUT_VALIDATE_NAME)

	Context->bUseInclude = Settings->SurfaceSource == EPCGExSurfaceSource::ActorReferences;
	if (Context->bUseInclude)
	{
		PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->ActorReference)

		Context->ActorReferenceDataFacade = PCGExData::TryGetSingleFacade(Context, PCGExSampling::SourceActorReferencesLabel, false, true);
		if (!Context->ActorReferenceDataFacade) { return false; }

		if (!PCGExSampling::GetIncludedActors(
			Context, Context->ActorReferenceDataFacade.ToSharedRef(),
			Settings->ActorReference, Context->IncludedActors))
		{
			return false;
		}

		TSet<UPrimitiveComponent*> IncludedPrimitiveSet;
		for (const TPair<AActor*, int32> Pair : Context->IncludedActors)
		{
			TArray<UPrimitiveComponent*> FoundPrimitives;
			Pair.Key->GetComponents(UPrimitiveComponent::StaticClass(), FoundPrimitives);
			for (UPrimitiveComponent* Primitive : FoundPrimitives) { IncludedPrimitiveSet.Add(Primitive); }
		}

		if (IncludedPrimitiveSet.IsEmpty())
		{
			// TODO : Throw
			return false;
		}

		Context->IncludedPrimitives.Reserve(IncludedPrimitiveSet.Num());
		Context->IncludedPrimitives.Append(IncludedPrimitiveSet.Array());
	}

	Context->CollisionSettings = Settings->CollisionSettings;
	Context->CollisionSettings.Init(Context);

	return true;
}

bool FPCGExSampleNearestSurfaceElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleNearestSurfaceElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleNearestSurface)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				if (Settings->bPruneFailedSamples) { NewBatch->bRequiresWriteStep = true; }
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to sample."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSampleNearestSurface
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleNearestSurface::Process);


		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		// Allocate edge native properties

		EPCGPointNativeProperties AllocateFor = EPCGPointNativeProperties::None;

		if (Context->ApplySampling.WantsApply())
		{
			AllocateFor |= EPCGPointNativeProperties::Transform;
		}

		PointDataFacade->GetOut()->AllocateProperties(AllocateFor);

		SurfacesForward = Context->ActorReferenceDataFacade ? Settings->AttributesForwarding.TryGetHandler(Context->ActorReferenceDataFacade, PointDataFacade, false) : nullptr;

		SamplingMask.SetNumUninitialized(PointDataFacade->GetNum());

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_NEARESTSURFACE(PCGEX_OUTPUT_INIT)
		}

		if (Settings->bUseLocalMaxDistance)
		{
			MaxDistanceGetter = PointDataFacade->GetBroadcaster<double>(Settings->LocalMaxDistance, true);
			if (!MaxDistanceGetter)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("LocalMaxDistance missing"));
				return false;
			}
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
		TProcessor<FPCGExSampleNearestSurfaceContext, UPCGExSampleNearestSurfaceSettings>::PrepareLoopScopesForPoints(Loops);
		MaxDistanceValue = MakeShared<PCGExMT::TScopedNumericValue<double>>(Loops, 0);
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::SampleNearestSurface::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		UPCGBasePointData* OutPointData = PointDataFacade->GetOut();

		TConstPCGValueRange<FTransform> InTransforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		auto SamplingFailed = [&](const int32 Index, const double MaxDistance)
		{
			SamplingMask[Index] = false;

			const FVector Direction = FVector::UpVector;
			PCGEX_OUTPUT_VALUE(Location, Index, InTransforms[Index].GetLocation())
			PCGEX_OUTPUT_VALUE(Normal, Index, Direction*-1) // TODO: expose "precise normal" in which case we line trace to location
			PCGEX_OUTPUT_VALUE(LookAt, Index, Direction)
			PCGEX_OUTPUT_VALUE(Distance, Index, MaxDistance)
			//PCGEX_OUTPUT_VALUE(IsInside, Index, false)
			//PCGEX_OUTPUT_VALUE(Success, Index, false)
			//PCGEX_OUTPUT_VALUE(ActorReference, Index, TEXT(""))
			//PCGEX_OUTPUT_VALUE(PhysMat, Index, TEXT(""))
		};


		PCGEX_SCOPE_LOOP(Index)
		{
			const double MaxDistance = MaxDistanceGetter ? MaxDistanceGetter->Read(Index) : Settings->MaxDistance;

			if (!PointFilterCache[Index])
			{
				if (Settings->bProcessFilteredOutAsFails) { SamplingFailed(Index, MaxDistance); }
				continue;
			}

			const FVector Origin = InTransforms[Index].GetLocation();

			FCollisionQueryParams CollisionParams;
			Context->CollisionSettings.Update(CollisionParams);

			const FCollisionShape CollisionShape = FCollisionShape::MakeSphere(MaxDistance);

			FVector HitLocation;
			const int32* HitIndex = nullptr;
			bool bSuccess = false;
			TArray<FOverlapResult> OutOverlaps;

			auto ProcessOverlapResults = [&]()
			{
				float MinDist = MAX_FLT;
				UPrimitiveComponent* HitComp = nullptr;
				for (const FOverlapResult& Overlap : OutOverlaps)
				{
					//if (!Overlap.bBlockingHit) { continue; }
					if (Context->bUseInclude && !Context->IncludedActors.Contains(Overlap.GetActor())) { continue; }

					FVector OutClosestLocation;
					const float Distance = Overlap.Component->GetClosestPointOnCollision(Origin, OutClosestLocation);

					if (Distance < 0) { continue; }

					if (Distance < MinDist)
					{
						HitIndex = Context->IncludedActors.Find(Overlap.GetActor());
						MinDist = Distance;
						HitLocation = OutClosestLocation;
						bSuccess = true;
						HitComp = Overlap.Component.Get();
					}
				}

				if (bSuccess)
				{
					const FVector Direction = (HitLocation - Origin).GetSafeNormal();

					PCGEX_OUTPUT_VALUE(LookAt, Index, Direction)

					FVector HitNormal = Direction * -1;
					bool bIsInside = MinDist == 0;

					if (SurfacesForward && HitIndex) { SurfacesForward->Forward(*HitIndex, Index); }

					if (HitComp)
					{
						if (Context->CollisionSettings.bTraceComplex)
						{
							FCollisionQueryParams PreciseCollisionParams;
							PreciseCollisionParams.bTraceComplex = true;
							PreciseCollisionParams.bReturnPhysicalMaterial = PhysMatWriter ? true : false;

							FHitResult HitResult;
							if (HitComp->LineTraceComponent(HitResult, HitLocation - Direction, HitLocation + Direction, PreciseCollisionParams))
							{
								HitNormal = HitResult.ImpactNormal;
								HitLocation = HitResult.Location;
								bIsInside = IsInsideWriter ? FVector::DotProduct(Direction, HitResult.ImpactNormal) > 0 : false;

								if (const AActor* HitActor = HitResult.GetActor()) { PCGEX_OUTPUT_VALUE(ActorReference, Index, FSoftObjectPath(HitActor->GetPathName())) }
								if (const UPhysicalMaterial* PhysMat = HitResult.PhysMaterial.Get()) { PCGEX_OUTPUT_VALUE(PhysMat, Index, FSoftObjectPath(PhysMat->GetPathName())) }
							}
						}
						else
						{
							UPhysicalMaterial* PhysMat = HitComp->GetBodyInstance()->GetSimplePhysicalMaterial();

							PCGEX_OUTPUT_VALUE(ActorReference, Index, FSoftObjectPath(HitComp->GetOwner()->GetPathName()))
							if (PhysMat) { PCGEX_OUTPUT_VALUE(PhysMat, Index, FSoftObjectPath(PhysMat->GetPathName())) }
						}
					}

					PCGEX_OUTPUT_VALUE(Location, Index, HitLocation)
					PCGEX_OUTPUT_VALUE(Normal, Index, HitNormal)
					PCGEX_OUTPUT_VALUE(IsInside, Index, bIsInside)
					PCGEX_OUTPUT_VALUE(Distance, Index, MinDist)
					PCGEX_OUTPUT_VALUE(Success, Index, true)

					SamplingMask[Index] = ((!bIsInside || !Settings->bProcessInsideAsFailedSamples) && (bIsInside || !Settings->bProcessOutsideAsFailedSamples));

					if (Context->ApplySampling.WantsApply())
					{
						PCGExData::FMutablePoint MutablePoint(OutPointData, Index);
						const FTransform OutTransform = FTransform(FRotationMatrix::MakeFromX(Direction).ToQuat(), HitLocation, FVector::OneVector);
						Context->ApplySampling.Apply(MutablePoint, OutTransform, OutTransform);
					}

					MaxDistanceValue->Set(Scope, FMath::Max(MaxDistanceValue->Get(Scope), MinDist));

					FPlatformAtomics::InterlockedExchange(&bAnySuccess, 1);
				}
				else
				{
					SamplingFailed(Index, MaxDistance);
				}
			};


			if (Settings->SurfaceSource == EPCGExSurfaceSource::ActorReferences)
			{
				for (const UPrimitiveComponent* Primitive : Context->IncludedPrimitives)
				{
					if (!IsValid(Primitive)) { continue; }
					if (TArray<FOverlapResult> TempOverlaps;
						Primitive->OverlapComponentWithResult(Origin, FQuat::Identity, CollisionShape, TempOverlaps))
					{
						OutOverlaps.Append(TempOverlaps);
					}
				}
				if (OutOverlaps.IsEmpty()) { SamplingFailed(Index, MaxDistance); }
				else { ProcessOverlapResults(); }
			}
			else
			{
				const UWorld* World = Context->GetWorld();

				switch (Context->CollisionSettings.CollisionType)
				{
				case EPCGExCollisionFilterType::Channel:
					if (World->OverlapMultiByChannel(OutOverlaps, Origin, FQuat::Identity, Context->CollisionSettings.CollisionChannel, CollisionShape, CollisionParams))
					{
						ProcessOverlapResults();
					}
					else { SamplingFailed(Index, MaxDistance); }
					break;
				case EPCGExCollisionFilterType::ObjectType:
					if (World->OverlapMultiByObjectType(OutOverlaps, Origin, FQuat::Identity, FCollisionObjectQueryParams(Context->CollisionSettings.CollisionObjectType), CollisionShape, CollisionParams))
					{
						ProcessOverlapResults();
					}
					else { SamplingFailed(Index, MaxDistance); }
					break;
				case EPCGExCollisionFilterType::Profile:
					if (World->OverlapMultiByProfile(OutOverlaps, Origin, FQuat::Identity, Context->CollisionSettings.CollisionProfileName, CollisionShape, CollisionParams))
					{
						ProcessOverlapResults();
					}
					else { SamplingFailed(Index, MaxDistance); }
					break;
				default:
					SamplingFailed(Index, MaxDistance);
					break;
				}
			}
		}
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		if (!Settings->bOutputNormalizedDistance || !DistanceWriter) { return; }

		const int32 NumPoints = PointDataFacade->GetNum();

		if (Settings->bOutputOneMinusDistance)
		{
			for (int i = 0; i < NumPoints; i++)
			{
				const double D = DistanceWriter->GetValue(i);
				DistanceWriter->SetValue(i, (1 - (D / MaxSampledDistance)) * Settings->DistanceScale);
			}
		}
		else
		{
			for (int i = 0; i < NumPoints; i++)
			{
				const double D = DistanceWriter->GetValue(i);
				DistanceWriter->SetValue(i, (D / MaxSampledDistance) * Settings->DistanceScale);
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->WriteFastest(AsyncManager);

		if (Settings->bTagIfHasSuccesses && bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasSuccessesTag); }
		if (Settings->bTagIfHasNoSuccesses && !bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasNoSuccessesTag); }
	}

	void FProcessor::Write()
	{
		if (Settings->bPruneFailedSamples) { (void)PointDataFacade->Source->Gather(SamplingMask); }
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

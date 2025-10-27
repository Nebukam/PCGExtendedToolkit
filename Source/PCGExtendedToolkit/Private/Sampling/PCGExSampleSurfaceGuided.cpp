﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleSurfaceGuided.h"

#include "Data/PCGExDataTag.h"
#include "Data/PCGExPointIO.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Sampling/PCGExTexParamFactoryProvider.h"


#define LOCTEXT_NAMESPACE "PCGExSampleSurfaceGuidedElement"
#define PCGEX_NAMESPACE SampleSurfaceGuided

UPCGExSampleSurfaceGuidedSettings::UPCGExSampleSurfaceGuidedSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Origin.Update(TEXT("$Position"));
}

TArray<FPCGPinProperties> UPCGExSampleSurfaceGuidedSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (SurfaceSource == EPCGExSurfaceSource::ActorReferences) { PCGEX_PIN_POINT(PCGExSampling::SourceActorReferencesLabel, "Points with actor reference paths.", Required) }
	if (bWriteRenderMat && bExtractTextureParameters) { PCGEX_PIN_FACTORIES(PCGExTexture::SourceTexLabel, "External texture params definitions.", Required, FPCGExDataTypeInfoTexParam::AsId()) }
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(SampleSurfaceGuided)

PCGExData::EIOInit UPCGExSampleSurfaceGuidedSettings::GetMainDataInitializationPolicy() const{ return PCGExData::EIOInit::Duplicate; }

PCGEX_ELEMENT_BATCH_POINT_IMPL(SampleSurfaceGuided)

bool FPCGExSampleSurfaceGuidedElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleSurfaceGuided)

	PCGEX_FWD(ApplySampling)
	Context->ApplySampling.Init();

	PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_VALIDATE_NAME)

	if (Settings->bWriteRenderMat && Settings->bExtractTextureParameters)
	{
		Context->bExtractTextureParams = true;

		if (!PCGExFactories::GetInputFactories(
			InContext, PCGExTexture::SourceTexLabel, Context->TexParamsFactories,
			{PCGExFactories::EType::TexParam}, true))
		{
			return false;
		}

		for (const TObjectPtr<const UPCGExTexParamFactoryData>& Factory : Context->TexParamsFactories) { PCGEX_VALIDATE_NAME_C(InContext, Factory->Config.TextureIDAttributeName) }
	}

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
	}

	Context->bSupportsUVQuery = UPhysicsSettings::Get()->bSupportUVFromHitResults;
	if (Settings->bWriteUVCoords && !Context->bSupportsUVQuery)
	{
		if (!Settings->bQuietUVSettingsWarning)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("`Project Settings->Physics->Support UV From Hit Results' must set to true for UV Query to work."));
		}
		Context->bWriteUVCoords = false;
	}

	Context->CollisionSettings = Settings->CollisionSettings;
	Context->CollisionSettings.Init(Context);

	return true;
}

bool FPCGExSampleSurfaceGuidedElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleSurfaceGuidedElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleSurfaceGuided)
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

namespace PCGExSampleSurfaceGuided
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleSurfaceGuided::Process);

		SurfacesForward = Context->bUseInclude ? Settings->AttributesForwarding.TryGetHandler(Context->ActorReferenceDataFacade, PointDataFacade, false) : nullptr;

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

		SamplingMask.SetNumUninitialized(PointDataFacade->GetNum());

		OriginGetter = PointDataFacade->GetBroadcaster<FVector>(Settings->Origin, true);

		if (!OriginGetter)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Some inputs don't have the required Origin data."));
			return false;
		}

		DirectionGetter = PointDataFacade->GetBroadcaster<FVector>(Settings->Direction, true);

		if (!DirectionGetter)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Some inputs don't have the required Direction data."));
			return false;
		}

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_INIT)
		}

		// So texture params are registered last, otherwise they're first in the list and it's confusing
		TexParamLookup = MakeShared<PCGExTexture::FLookup>();
		if (!TexParamLookup->BuildFrom(Context->TexParamsFactories)) { TexParamLookup.Reset(); }
		else { TexParamLookup->PrepareForWrite(Context, PointDataFacade); }

		if (Settings->DistanceInput == EPCGExTraceSampleDistanceInput::Attribute)
		{
			MaxDistanceGetter = PointDataFacade->GetBroadcaster<double>(Settings->LocalMaxDistance, true);
			if (!MaxDistanceGetter)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("LocalMaxDistance missing"));
				return false;
			}
		}

		World = Context->GetWorld();
		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareLoopScopesForPoints(const TArray<PCGExMT::FScope>& Loops)
	{
		TProcessor<FPCGExSampleSurfaceGuidedContext, UPCGExSampleSurfaceGuidedSettings>::PrepareLoopScopesForPoints(Loops);
		MaxDistanceValue = MakeShared<PCGExMT::TScopedNumericValue<double>>(Loops, 0);
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::SampleSurfaceGuided::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		UPCGBasePointData* OutPointData = PointDataFacade->GetOut();

		TConstPCGValueRange<FTransform> InTransforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		double DirMult = Settings->bInvertDirection ? -1 : 1;

		PCGEX_SCOPE_LOOP(Index)
		{
			const FVector Direction = DirectionGetter->Read(Index).GetSafeNormal() * DirMult;
			const FVector Origin = OriginGetter->Read(Index);
			const double MaxDistance = MaxDistanceGetter ? MaxDistanceGetter->Read(Index) : Settings->DistanceInput == EPCGExTraceSampleDistanceInput::Constant ? Settings->MaxDistance : Direction.Length();

			auto SamplingFailed = [&]()
			{
				SamplingMask[Index] = false;

				PCGEX_OUTPUT_VALUE(Location, Index, InTransforms[Index].GetLocation())
				PCGEX_OUTPUT_VALUE(Normal, Index, Direction*-1)
				PCGEX_OUTPUT_VALUE(LookAt, Index, Direction)
				PCGEX_OUTPUT_VALUE(Distance, Index, MaxDistance)
				//PCGEX_OUTPUT_VALUE(UVCoords, Index, FVector2D::ZeroVector)
				//PCGEX_OUTPUT_VALUE(FaceIndex, Index, -1)
				//PCGEX_OUTPUT_VALUE(RenderMat, Index, -1)
				//PCGEX_OUTPUT_VALUE(IsInside, Index, false)
				//PCGEX_OUTPUT_VALUE(Success, Index, false)
				//PCGEX_OUTPUT_VALUE(ActorReference, Index, TEXT(""))
				//PCGEX_OUTPUT_VALUE(PhysMat, Index, TEXT(""))
				if (TexParamLookup) { TexParamLookup->ExtractParams(Index, nullptr); }
			};


			if (!PointFilterCache[Index])
			{
				if (Settings->bProcessFilteredOutAsFails) { SamplingFailed(); }
				continue;
			}

			FCollisionQueryParams CollisionParams;
			Context->CollisionSettings.Update(CollisionParams);
			CollisionParams.bReturnPhysicalMaterial = Settings->bWritePhysMat;
			CollisionParams.bReturnFaceIndex = Settings->bWriteFaceIndex || Settings->bWriteUVCoords;

			const FVector Trace = Direction * MaxDistance;
			const FVector End = Origin + Trace;

			bool bSuccess = false;
			int32* HitIndex = nullptr;
			FHitResult HitResult;
			TArray<FHitResult> HitResults;

			auto ProcessTraceResult = [&]()
			{
				bSuccess = true;

				const double HitDistance = FVector::Distance(HitResult.ImpactPoint, Origin);
				PCGEX_OUTPUT_VALUE(Location, Index, HitResult.ImpactPoint)
				PCGEX_OUTPUT_VALUE(LookAt, Index, Direction)
				PCGEX_OUTPUT_VALUE(Normal, Index, HitResult.ImpactNormal)
				PCGEX_OUTPUT_VALUE(Distance, Index, HitDistance)
				PCGEX_OUTPUT_VALUE(IsInside, Index, FVector::DotProduct(Direction, HitResult.Normal) > 0)
				PCGEX_OUTPUT_VALUE(Success, Index, bSuccess)

				MaxDistanceValue->Set(Scope, FMath::Max(MaxDistanceValue->Get(Scope), HitDistance));

				if (Context->ApplySampling.WantsApply())
				{
					PCGExData::FMutablePoint MutablePoint(OutPointData, Index);
					const FTransform OutTransform = FTransform(FRotationMatrix::MakeFromX(Direction).ToQuat(), HitResult.ImpactPoint, FVector::OneVector);
					Context->ApplySampling.Apply(MutablePoint, OutTransform, OutTransform);
				}

				if (Settings->bWriteUVCoords)
				{
					FVector2D UVCoords = FVector2D::ZeroVector;
					if (UGameplayStatics::FindCollisionUV(HitResult, Settings->UVChannel, UVCoords)) { PCGEX_OUTPUT_VALUE(UVCoords, Index, UVCoords) }
					else { PCGEX_OUTPUT_VALUE(UVCoords, Index, FVector2D::ZeroVector) }
				}

				PCGEX_OUTPUT_VALUE(FaceIndex, Index, HitResult.FaceIndex)

				SamplingMask[Index] = bSuccess;

				if (const AActor* HitActor = HitResult.GetActor())
				{
					HitIndex = Context->IncludedActors.Find(HitActor);
					PCGEX_OUTPUT_VALUE(ActorReference, Index, FSoftObjectPath(HitActor->GetPathName()))
				}

				if (const UPhysicalMaterial* PhysMat = HitResult.PhysMaterial.Get()) { PCGEX_OUTPUT_VALUE(PhysMat, Index, FSoftObjectPath(PhysMat->GetPathName())) }
				if (const UPrimitiveComponent* HitComponent = HitResult.GetComponent())
				{
					PCGEX_OUTPUT_VALUE(HitComponentReference, Index, FSoftObjectPath(HitComponent->GetPathName()))
					UMaterialInterface* RenderMat = HitComponent->GetMaterial(Settings->RenderMaterialIndex);
					PCGEX_OUTPUT_VALUE(RenderMat, Index, FSoftObjectPath(RenderMat ? RenderMat->GetPathName() : TEXT("")))
					if (TexParamLookup) { TexParamLookup->ExtractParams(Index, RenderMat); }
				}

				if (SurfacesForward && HitIndex) { SurfacesForward->Forward(*HitIndex, Index); }

				FPlatformAtomics::InterlockedExchange(&bAnySuccess, 1);
			};

			auto ProcessMultipleTraceResult = [&]()
			{
				for (const FHitResult& Hit : HitResults)
				{
					if (Context->IncludedActors.Contains(Hit.GetActor()))
					{
						HitResult = Hit;
						ProcessTraceResult();
						return;
					}
				}
			};

			switch (Context->CollisionSettings.CollisionType)
			{
			case EPCGExCollisionFilterType::Channel:
				if (Context->bUseInclude)
				{
					if (World->LineTraceMultiByChannel(
						HitResults, Origin, End,
						Context->CollisionSettings.CollisionChannel, CollisionParams))
					{
						ProcessMultipleTraceResult();
					}
				}
				else
				{
					if (World->LineTraceSingleByChannel(
						HitResult, Origin, End,
						Context->CollisionSettings.CollisionChannel, CollisionParams))
					{
						ProcessTraceResult();
					}
				}
				break;
			case EPCGExCollisionFilterType::ObjectType:
				if (Context->bUseInclude)
				{
					if (World->LineTraceMultiByObjectType(
						HitResults, Origin, End,
						FCollisionObjectQueryParams(Context->CollisionSettings.CollisionObjectType), CollisionParams))
					{
						ProcessMultipleTraceResult();
					}
				}
				else
				{
					if (World->LineTraceSingleByObjectType(
						HitResult, Origin, End,
						FCollisionObjectQueryParams(Context->CollisionSettings.CollisionObjectType), CollisionParams))
					{
						ProcessTraceResult();
					}
				}
				break;
			case EPCGExCollisionFilterType::Profile:
				if (Context->bUseInclude)
				{
					if (World->LineTraceMultiByProfile(
						HitResults, Origin, End,
						Context->CollisionSettings.CollisionProfileName, CollisionParams))
					{
						ProcessMultipleTraceResult();
					}
				}
				else
				{
					if (World->LineTraceSingleByProfile(
						HitResult, Origin, End,
						Context->CollisionSettings.CollisionProfileName, CollisionParams))
					{
						ProcessTraceResult();
					}
				}
				break;
			default:
				break;
			}

			if (!bSuccess) { SamplingFailed(); }
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

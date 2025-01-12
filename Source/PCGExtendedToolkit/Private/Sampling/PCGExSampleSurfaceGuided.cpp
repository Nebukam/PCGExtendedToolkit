// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleSurfaceGuided.h"

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
	if (SurfaceSource == EPCGExSurfaceSource::ActorReferences) { PCGEX_PIN_POINT(PCGExSampling::SourceActorReferencesLabel, "Points with actor reference paths.", Required, {}) }
	if (bWriteRenderMat && bExtractTextureParameters) { PCGEX_PIN_PARAMS(PCGExTexture::SourceTexLabel, "External texture params definitions.", Required, {}) }
	return PinProperties;
}

PCGExData::EIOInit UPCGExSampleSurfaceGuidedSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_INITIALIZE_ELEMENT(SampleSurfaceGuided)

bool FPCGExSampleSurfaceGuidedElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleSurfaceGuided)

	PCGEX_FOREACH_FIELD_SURFACEGUIDED(PCGEX_OUTPUT_VALIDATE_NAME)

	if (Settings->bWriteRenderMat && Settings->bExtractTextureParameters)
	{
		Context->bExtractTextureParams = true;

		if (!PCGExFactories::GetInputFactories(InContext, PCGExTexture::SourceTexLabel, Context->TexParamsFactories, {PCGExFactories::EType::TexParam}, true)) { return false; }
		for (const TObjectPtr<const UPCGExTexParamFactoryData>& Factory : Context->TexParamsFactories) { PCGEX_VALIDATE_NAME_C(InContext, Factory->Config.TextureIDAttributeName) }
	}

	Context->bUseInclude = Settings->SurfaceSource == EPCGExSurfaceSource::ActorReferences;
	if (Context->bUseInclude)
	{
		PCGEX_VALIDATE_NAME_CONSUMABLE(Settings->ActorReference)
		Context->ActorReferenceDataFacade = PCGExData::TryGetSingleFacade(Context, PCGExSampling::SourceActorReferencesLabel, true);
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
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleSurfaceGuided::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSampleSurfaceGuided::FProcessor>>& NewBatch)
			{
				if (Settings->bPruneFailedSamples) { NewBatch->bRequiresWriteStep = true; }
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to sample."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExSampleSurfaceGuided
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleSurfaceGuided::Process);

		SurfacesForward = Context->bUseInclude ? Settings->AttributesForwarding.TryGetHandler(Context->ActorReferenceDataFacade, PointDataFacade) : nullptr;

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		SampleState.SetNumUninitialized(PointDataFacade->GetNum());

		OriginGetter = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->Origin);

		if (!OriginGetter)
		{
			PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Some inputs don't have the required Origin data."));
			return false;
		}

		DirectionGetter = PointDataFacade->GetScopedBroadcaster<FVector>(Settings->Direction);

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
			MaxDistanceGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->LocalMaxDistance);
			if (!MaxDistanceGetter)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("LocalMaxDistance missing"));
				return false;
			}
		}

		World = Context->SourceComponent->GetWorld();
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
		const FVector Direction = DirectionGetter->Read(Index).GetSafeNormal();
		const FVector Origin = OriginGetter->Read(Index);
		const double MaxDistance = MaxDistanceGetter ? MaxDistanceGetter->Read(Index) : Settings->DistanceInput == EPCGExTraceSampleDistanceInput::Constant ? Settings->MaxDistance : Direction.Length();

		auto SamplingFailed = [&]()
		{
			SampleState[Index] = false;

			PCGEX_OUTPUT_VALUE(Location, Index, Point.Transform.GetLocation())
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
			return;
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

			PCGEX_OUTPUT_VALUE(Location, Index, HitResult.ImpactPoint)
			PCGEX_OUTPUT_VALUE(LookAt, Index, Direction)
			PCGEX_OUTPUT_VALUE(Normal, Index, HitResult.ImpactNormal)
			PCGEX_OUTPUT_VALUE(Distance, Index, FVector::Distance(HitResult.ImpactPoint, Origin))
			PCGEX_OUTPUT_VALUE(IsInside, Index, FVector::DotProduct(Direction, HitResult.Normal) > 0)
			PCGEX_OUTPUT_VALUE(Success, Index, bSuccess)

			if (Settings->bWriteUVCoords)
			{
				FVector2D UVCoords = FVector2D::ZeroVector;
				if (UGameplayStatics::FindCollisionUV(HitResult, Settings->UVChannel, UVCoords)) { PCGEX_OUTPUT_VALUE(UVCoords, Index, UVCoords) }
				else { PCGEX_OUTPUT_VALUE(UVCoords, Index, FVector2D::ZeroVector) }
			}

			PCGEX_OUTPUT_VALUE(FaceIndex, Index, HitResult.FaceIndex)

			SampleState[Index] = bSuccess;

#if PCGEX_ENGINE_VERSION <= 503
			if (const AActor* HitActor = HitResult.GetActor())
			{
				HitIndex = Context->IncludedActors.Find(HitActor);
				PCGEX_OUTPUT_VALUE(ActorReference, Index, HitActor->GetPathName())
			}

			if (const UPhysicalMaterial* PhysMat = HitResult.PhysMaterial.Get()) { PCGEX_OUTPUT_VALUE(PhysMat, Index, PhysMat->GetPathName()) }
			if (const UPrimitiveComponent* HitComponent = HitResult.GetComponent())
			{
				PCGEX_OUTPUT_VALUE(HitComponentReference, Index, HitComponent->GetPathName())
				UMaterialInterface* RenderMat = HitComponent->GetMaterial(Settings->RenderMaterialIndex);
				PCGEX_OUTPUT_VALUE(RenderMat, Index, RenderMat ? RenderMat->GetPathName() : TEXT(""))
				if(TexParamLookup){TexParamLookup->ExtractParams(Index, RenderMat);}
			}
#else
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

#endif

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
					FCollisionObjectQueryParams(Context->CollisionSettings.CollisionObjectType), CollisionParams)) { ProcessTraceResult(); }
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
					Context->CollisionSettings.CollisionProfileName, CollisionParams)) { ProcessTraceResult(); }
			}
			break;
		default:
			break;
		}

		if (!bSuccess) { SamplingFailed(); }
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);

		if (Settings->bTagIfHasSuccesses && bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasSuccessesTag); }
		if (Settings->bTagIfHasNoSuccesses && !bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasNoSuccessesTag); }
	}

	void FProcessor::Write()
	{
		PCGExSampling::PruneFailedSamples(PointDataFacade->GetMutablePoints(), SampleState);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

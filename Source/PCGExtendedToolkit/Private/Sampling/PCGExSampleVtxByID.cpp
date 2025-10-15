﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleVtxByID.h"

#include "PCGExMath.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExDataTag.h"
#include "Data/Blending/PCGExBlendOpFactoryProvider.h"
#include "Data/Blending/PCGExBlendOpsManager.h"
#include "Data/Blending/PCGExUnionOpsManager.h"
#include "Details/PCGExDetailsSettings.h"
#include "Graph/PCGExGraph.h"


#include "Misc/PCGExSortPoints.h"


#define LOCTEXT_NAMESPACE "PCGExSampleVtxByIDElement"
#define PCGEX_NAMESPACE SampleVtxByID

PCGEX_SETTING_VALUE_IMPL(UPCGExSampleVtxByIDSettings, LookAtUp, FVector, LookAtUpInput, LookAtUpSource, LookAtUpConstant)

UPCGExSampleVtxByIDSettings::UPCGExSampleVtxByIDSettings(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TArray<FPCGPinProperties> UPCGExSampleVtxByIDSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();

	PCGEX_PIN_POINTS(PCGExGraph::SourceVerticesLabel, "The point data set to check against.", Required)
	PCGExDataBlending::DeclareBlendOpsInputs(PinProperties, EPCGPinStatus::Normal);

	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(SampleVtxByID)
PCGEX_ELEMENT_BATCH_POINT_IMPL(SampleVtxByID)

bool FPCGExSampleVtxByIDElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleVtxByID)

	PCGEX_VALIDATE_NAME(Settings->VtxIdSource)

	PCGEX_FWD(ApplySampling)
	Context->ApplySampling.Init();

	PCGExFactories::GetInputFactories<UPCGExBlendOpFactory>(
		Context, PCGExDataBlending::SourceBlendingLabel, Context->BlendingFactories,
		{PCGExFactories::EType::Blending}, false);

	FBox OctreeBounds = FBox(ForceInit);

	TSharedPtr<PCGExData::FPointIOCollection> Targets = MakeShared<PCGExData::FPointIOCollection>(
		Context, PCGExGraph::SourceVerticesLabel, PCGExData::EIOInit::NoInit, true);

	if (Targets->IsEmpty())
	{
		PCGEX_LOG_MISSING_INPUT(Context, FTEXT("No targets (empty datasets)"))
		return false;
	}

	for (const TSharedPtr<PCGExData::FPointIO>& IO : Targets->Pairs)
	{
		if (!IO->FindConstAttribute<int64>(PCGExGraph::Attr_PCGExVtxIdx)) { continue; }

		TSharedPtr<PCGExData::FFacade> TargetFacade = MakeShared<PCGExData::FFacade>(IO.ToSharedRef());
		TargetFacade->Idx = Context->TargetFacades.Num();

		Context->TargetFacades.Add(TargetFacade.ToSharedRef());
	}

	Context->TargetsPreloader = MakeShared<PCGExData::FMultiFacadePreloader>(Context->TargetFacades);

	Context->TargetsPreloader->ForEach(
		[&](PCGExData::FFacadePreloader& Preloader)
		{
			Preloader.Register<int64>(Context, PCGExGraph::Attr_PCGExVtxIdx);
			PCGExDataBlending::RegisterBuffersDependencies_SourceA(Context, Preloader, Context->BlendingFactories);
		});

	return true;
}

void FPCGExSampleVtxByIDElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	FPCGExPointsProcessorElement::PostLoadAssetsDependencies(InContext);

	PCGEX_CONTEXT_AND_SETTINGS(SampleVtxByID)
}

bool FPCGExSampleVtxByIDElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleVtxByIDElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleVtxByID)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->SetAsyncState(PCGExCommon::State_FacadePreloading);

		TWeakPtr<FPCGContextHandle> WeakHandle = Context->GetOrCreateHandle();
		Context->TargetsPreloader->OnCompleteCallback = [Settings, Context, WeakHandle]()
		{
			// TODO : Need to revisit this, it's likely way too slow
			for (const TSharedRef<PCGExData::FFacade>& TargetFacade : Context->TargetFacades)
			{
				const TConstPCGValueRange<int64> MetadataEntries = TargetFacade->GetIn()->GetConstMetadataEntryValueRange();
				const FPCGMetadataAttribute<int64>* Attr = TargetFacade->FindConstAttribute<int64>(PCGExGraph::Attr_PCGExVtxIdx);

				for (int i = 0; i < MetadataEntries.Num(); i++)
				{
					uint32 VtxID = PCGEx::H64A(Attr->GetValueFromItemKey(MetadataEntries[i]));
					Context->VtxLookup.Add(VtxID, PCGEx::H64(i, TargetFacade->Idx));
				}
			}

			PCGEX_SHARED_CONTEXT_VOID(WeakHandle)

			if (!Context->StartBatchProcessingPoints(
				[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
				[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
				{
					NewBatch->bRequiresWriteStep = Settings->bPruneFailedSamples;
				}))
			{
				Context->CancelExecution(TEXT("Could not find any points to sample."));
			}
		};

		Context->TargetsPreloader->StartLoading(Context->GetAsyncManager());
		return false;
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

bool FPCGExSampleVtxByIDElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
	return Context ? Context->CurrentPhase == EPCGExecutionPhase::PrepareData : false;
}

namespace PCGExSampleVtxByID
{
	FProcessor::~FProcessor()
	{
	}

	void FProcessor::SamplingFailed(const int32 Index)
	{
		SamplingMask[Index] = false;
		const TConstPCGValueRange<FTransform> Transforms = PointDataFacade->GetIn()->GetConstTransformValueRange();
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleVtxByID::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		// Allocate edge native properties

		EPCGPointNativeProperties AllocateFor = EPCGPointNativeProperties::None;
		if (Context->ApplySampling.WantsApply()) { AllocateFor |= EPCGPointNativeProperties::Transform; }
		PointDataFacade->GetOut()->AllocateProperties(AllocateFor);

		LookAtUpGetter = Settings->GetValueSettingLookAtUp();
		if (!LookAtUpGetter->Init(PointDataFacade)) { return false; }

		VtxID32Getter = PointDataFacade->GetReadable<int32>(Settings->VtxIdSource, PCGExData::EIOSide::In, true);
		if (!VtxID32Getter)
		{
			VtxID64Getter = PointDataFacade->GetReadable<int64>(Settings->VtxIdSource, PCGExData::EIOSide::In, true);
		}

		if (!VtxID32Getter && !VtxID64Getter)
		{
			PCGEX_LOG_INVALID_ATTR_C(Context, VtxId, Settings->VtxIdSource)
			return false;
		}


		SamplingMask.SetNumUninitialized(PointDataFacade->GetNum());

		if (!Context->BlendingFactories.IsEmpty())
		{
			UnionBlendOpsManager = MakeShared<PCGExDataBlending::FUnionOpsManager>(&Context->BlendingFactories, Context->DistanceDetails);
			if (!UnionBlendOpsManager->Init(Context, PointDataFacade, Context->TargetFacades)) { return false; }
			DataBlender = UnionBlendOpsManager;
		}

		if (!DataBlender)
		{
			TSharedPtr<PCGExDataBlending::FDummyUnionBlender> DummyUnionBlender = MakeShared<PCGExDataBlending::FDummyUnionBlender>();
			DummyUnionBlender->Init(PointDataFacade, Context->TargetFacades);
			DataBlender = DummyUnionBlender;
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::SampleVtxByID::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		bool bLocalAnySuccess = false;

		TArray<PCGExData::FWeightedPoint> OutWeightedPoints;
		TArray<PCGEx::FOpStats> Trackers;
		DataBlender->InitTrackers(Trackers);

		UPCGBasePointData* OutPointData = PointDataFacade->GetOut();
		TConstPCGValueRange<FTransform> Transforms = PointDataFacade->GetIn()->GetConstTransformValueRange();

		const TSharedPtr<PCGExSampling::FSampingUnionData> Union = MakeShared<PCGExSampling::FSampingUnionData>();
		Union->IOSet.Reserve(Context->TargetFacades.Num());

		PCGEX_SCOPE_LOOP(Index)
		{
			Union->Reset();

			if (!PointFilterCache[Index])
			{
				if (Settings->bProcessFilteredOutAsFails) { SamplingFailed(Index); }
				continue;
			}

			const uint64* Hash = Context->VtxLookup.Find(VtxID32Getter ? VtxID32Getter->Read(Index) : PCGEx::H64A(VtxID64Getter->Read(Index)));
			if (!Hash)
			{
				SamplingFailed(Index);
				continue;
			}

			PCGExData::FElement Element(PCGEx::H64A(*Hash), PCGEx::H64B(*Hash));
			Union->AddWeighted_Unsafe(Element, 1);

			const FVector Origin = Transforms[Index].GetLocation();
			const FVector LookAtUp = LookAtUpGetter->Read(Index).GetSafeNormal();

			DataBlender->ComputeWeights(Index, Union, OutWeightedPoints);

			FTransform VtxTransform = Context->TargetFacades[Element.IO]->GetIn()->GetTransform(Element.Index);
			double Distance = FVector::Dist(Origin, VtxTransform.GetLocation());

			// Blend using updated weighted points
			DataBlender->Blend(Index, OutWeightedPoints, Trackers);

			const FVector CWDistance = Origin - VtxTransform.GetLocation();
			FVector LookAt = CWDistance.GetSafeNormal();

			FTransform LookAtTransform = PCGExMath::MakeLookAtTransform(LookAt, LookAtUp, Settings->LookAtAxisAlign);
			if (Context->ApplySampling.WantsApply())
			{
				PCGExData::FMutablePoint MutablePoint(OutPointData, Index);
				Context->ApplySampling.Apply(MutablePoint, VtxTransform, LookAtTransform);
			}

			SamplingMask[Index] = !Union->IsEmpty();
			bLocalAnySuccess = true;
		}

		if (bLocalAnySuccess) { FPlatformAtomics::InterlockedExchange(&bAnySuccess, 1); }
	}

	void FProcessor::CompleteWork()
	{
		if (UnionBlendOpsManager) { UnionBlendOpsManager->Cleanup(Context); }
		PointDataFacade->WriteFastest(AsyncManager);

		if (Settings->bTagIfHasSuccesses && bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasSuccessesTag); }
		if (Settings->bTagIfHasNoSuccesses && !bAnySuccess) { PointDataFacade->Source->Tags->AddRaw(Settings->HasNoSuccessesTag); }
	}

	void FProcessor::Write()
	{
		(void)PointDataFacade->Source->Gather(SamplingMask);
	}

	void FProcessor::Cleanup()
	{
		TProcessor<FPCGExSampleVtxByIDContext, UPCGExSampleVtxByIDSettings>::Cleanup();
		UnionBlendOpsManager.Reset();
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

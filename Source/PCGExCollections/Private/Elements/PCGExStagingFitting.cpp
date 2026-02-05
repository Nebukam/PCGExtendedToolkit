// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/PCGExStagingFitting.h"

#include "PCGParamData.h"
#include "Core/PCGExAssetCollection.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGExRandomHelpers.h"


#define LOCTEXT_NAMESPACE "PCGExStagingFittingElement"
#define PCGEX_NAMESPACE StagingFitting

#pragma region UPCGExStagingFittingSettings

PCGExData::EIOInit UPCGExStagingFittingSettings::GetMainDataInitializationPolicy() const
{
	if (StealData == EPCGExOptionState::Enabled && !bPruneEmptyPoints) { return PCGExData::EIOInit::Forward; }
	return PCGExData::EIOInit::Duplicate;
}

PCGEX_INITIALIZE_ELEMENT(StagingFitting)
PCGEX_ELEMENT_BATCH_POINT_IMPL(StagingFitting)

TArray<FPCGPinProperties> UPCGExStagingFittingSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAM(PCGExCollections::Labels::SourceCollectionMapLabel, "Collection map information from, or merged from, Staging nodes.", Required)
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExStagingFittingSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	return PinProperties;
}

#pragma endregion

#pragma region FPCGExStagingFittingElement

bool FPCGExStagingFittingElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(StagingFitting)

	Context->CollectionPickUnpacker = MakeShared<PCGExCollections::FPickUnpacker>();
	Context->CollectionPickUnpacker->UnpackPin(InContext, PCGExCollections::Labels::SourceCollectionMapLabel);

	if (!Context->CollectionPickUnpacker->HasValidMapping())
	{
		PCGE_LOG(Error, GraphAndLog, FTEXT("Could not rebuild a valid asset mapping from the provided map."));
		return false;
	}

	return true;
}

bool FPCGExStagingFittingElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExStagingFittingElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(StagingFitting)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = Settings->bPruneEmptyPoints;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->MainPoints->StageOutputs();
	return Context->TryComplete();
}

#pragma endregion

#pragma region PCGExStagingFitting::FProcessor

namespace PCGExStagingFitting
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExStagingFitting::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InTaskManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, Settings->GetMainDataInitializationPolicy())

		EntryHashGetter = PointDataFacade->GetReadable<int64>(PCGExCollections::Labels::Tag_EntryIdx, PCGExData::EIOSide::In, true);
		if (!EntryHashGetter) { return false; }

		FittingHandler.ScaleToFit = Settings->ScaleToFit;
		FittingHandler.Justification = Settings->Justification;

		if (!FittingHandler.Init(ExecutionContext, PointDataFacade)) { return false; }

		Variations = Settings->Variations;
		Variations.Init(Settings->Seed);

		if (Settings->bWriteTranslation)
		{
			TranslationWriter = PointDataFacade->GetWritable<FVector>(Settings->TranslationAttributeName, FVector::ZeroVector, true, PCGExData::EBufferInit::Inherit);
		}

		EPCGPointNativeProperties AllocateFor = EPCGPointNativeProperties::None;
		AllocateFor |= EPCGPointNativeProperties::BoundsMin;
		AllocateFor |= EPCGPointNativeProperties::BoundsMax;
		AllocateFor |= EPCGPointNativeProperties::Transform;

		PointDataFacade->GetOut()->AllocateProperties(AllocateFor);

		if (Settings->bPruneEmptyPoints) { Mask.Init(1, PointDataFacade->GetNum()); }

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::StagingFitting::ProcessPoints);

		PointDataFacade->Fetch(Scope);
		FilterScope(Scope);

		UPCGBasePointData* OutPointData = PointDataFacade->GetOut();

		const TPCGValueRange<FTransform> OutTransforms = OutPointData->GetTransformValueRange(false);
		const TPCGValueRange<FVector> OutBoundsMin = OutPointData->GetBoundsMinValueRange(false);
		const TPCGValueRange<FVector> OutBoundsMax = OutPointData->GetBoundsMaxValueRange(false);
		const TConstPCGValueRange<int32> Seeds = OutPointData->GetConstSeedValueRange();

		int32 LocalNumInvalid = 0;
		int16 MaterialPick = 0;
		FRandomStream RandomSource;

		PCGEX_SCOPE_LOOP(Index)
		{
			if (!PointFilterCache[Index])
			{
				if (Settings->bPruneEmptyPoints)
				{
					Mask[Index] = 0;
					LocalNumInvalid++;
				}
				continue;
			}

			const uint64 Hash = EntryHashGetter->Read(Index);

			FPCGExEntryAccessResult Result = Context->CollectionPickUnpacker->ResolveEntry(Hash, MaterialPick);

			if (!Result.IsValid() || !Result.Entry->Staging.Bounds.IsValid)
			{
				if (Settings->bPruneEmptyPoints)
				{
					Mask[Index] = 0;
					LocalNumInvalid++;
				}
				continue;
			}

			const FPCGExAssetCollectionEntry* Entry = Result.Entry;
			const UPCGExAssetCollection* EntryHost = Result.Host;

			FTransform& OutTransform = OutTransforms[Index];
			FVector OutTranslation = FVector::ZeroVector;
			FBox OutBounds = Entry->Staging.Bounds;

			const FPCGExFittingVariations& EntryVariations = Entry->GetVariations(EntryHost);

			RandomSource.Initialize(PCGExRandomHelpers::GetSeed(Seeds[Index], Variations.Seed));

			if (Variations.bEnabledBefore)
			{
				FTransform LocalXForm = FTransform::Identity;
				Variations.Apply(RandomSource, LocalXForm, EntryVariations, EPCGExVariationMode::Before);
				FittingHandler.ComputeLocalTransform(Index, LocalXForm, OutTransform, OutBounds, OutTranslation);
			}
			else
			{
				FittingHandler.ComputeTransform(Index, OutTransform, OutBounds, OutTranslation);
			}

			if (TranslationWriter) { TranslationWriter->SetValue(Index, OutTranslation); }

			OutBoundsMin[Index] = OutBounds.Min;
			OutBoundsMax[Index] = OutBounds.Max;

			if (Variations.bEnabledAfter)
			{
				Variations.Apply(RandomSource, OutTransform, EntryVariations, EPCGExVariationMode::After);
			}
		}

		FPlatformAtomics::InterlockedAdd(&NumInvalid, LocalNumInvalid);
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		PointDataFacade->WriteBuffers(
			TaskManager,
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->Write();
			});
	}

	void FProcessor::Write()
	{
		if (Settings->bPruneEmptyPoints)
		{
			(void)PointDataFacade->Source->Gather(Mask);
		}
	}
}

#pragma endregion

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

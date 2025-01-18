// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Sampling/PCGExSampleTexture.h"

#include "Sampling/PCGExTexParamFactoryProvider.h"


#define LOCTEXT_NAMESPACE "PCGExSampleTextureElement"
#define PCGEX_NAMESPACE SampleTexture

UPCGExSampleTextureSettings::UPCGExSampleTextureSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	UVSource.Update("UVCoords");
}

TArray<FPCGPinProperties> UPCGExSampleTextureSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_TEXTURES(PCGExTexture::SourceTextureDataLabel, "Texture objects referenced by input points.", Required, {})
	PCGEX_PIN_FACTORIES(PCGExTexture::SourceTexLabel, "Texture params to extract from reference materials.", Required, {})
	return PinProperties;
}

PCGExData::EIOInit UPCGExSampleTextureSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_INITIALIZE_ELEMENT(SampleTexture)

bool FPCGExSampleTextureElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(SampleTexture)

	if (!PCGExFactories::GetInputFactories(InContext, PCGExTexture::SourceTexLabel, Context->TexParamsFactories, {PCGExFactories::EType::TexParam}, true)) { return false; }

	TSet<FName> UniqueSampleNames;
	for (const TObjectPtr<const UPCGExTexParamFactoryData>& Factory : Context->TexParamsFactories)
	{
		PCGEX_VALIDATE_NAME_C(InContext, Factory->Config.TextureIDAttributeName)
		PCGEX_VALIDATE_NAME_C(InContext, Factory->Config.SampleAttributeName)

		bool bAlreadySet = false;
		if (Factory->Config.OutputType == EPCGExTexSampleAttributeType::Invalid)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("A Texture Config with sample name \"{0}\" has invalid sample settings and will be ignored."), FText::FromName(Factory->Config.SampleAttributeName)));
			continue;
		}

		UniqueSampleNames.Add(Factory->Config.SampleAttributeName, &bAlreadySet);
		if (bAlreadySet && !Settings->bQuietDuplicateSampleNamesWarning)
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("Sample output attribute name \"{0}\" is used multiple times. If this is intended, you can quiet this warning in the settings."), FText::FromName(Factory->Config.SampleAttributeName)));
		}
	}

	Context->TextureMap = MakeShared<PCGExTexture::FLookup>();
	Context->TextureMap->BuildMapFrom(Context, PCGExTexture::SourceTextureDataLabel);

	return true;
}

bool FPCGExSampleTextureElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExSampleTextureElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(SampleTexture)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExSampleTexture::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExSampleTexture::FProcessor>>& NewBatch)
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

namespace PCGExSampleTexture
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExSampleTexture::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		SampleState.Init(false, PointDataFacade->GetNum());

		UVGetter = PointDataFacade->GetScopedBroadcaster<FVector2D>(Settings->UVSource);

		if (!UVGetter)
		{
			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(FTEXT("UV attribute : \"{0}\" does not exists."), FText::FromName(Settings->UVSource.GetName())));
			return false;
		}

		for (const TObjectPtr<const UPCGExTexParamFactoryData>& Factory : Context->TexParamsFactories)
		{
			if (Factory->Config.OutputType == EPCGExTexSampleAttributeType::Invalid) { continue; }

			PCGEx::ExecuteWithRightType(
				Factory->Config.MetadataType, [&](auto DummyValue)
				{
					using T = decltype(DummyValue);

					TSharedPtr<TSampler<T>> Sampler = MakeShared<TSampler<T>>(Factory->Config, Context->TextureMap, PointDataFacade);
					if (!Sampler->IsValid())
					{
						PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(FTEXT("Some inputs are missing the ID attribute : \"{0}\"."), FText::FromName(Factory->Config.TextureIDAttributeName)));
						return;
					}

					Samplers.Add(Sampler.ToSharedRef());
				});
		}

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
		auto SamplingFailed = [&]()
		{
			SampleState[Index] = false;
		};

		if (!PointFilterCache[Index])
		{
			if (Settings->bProcessFilteredOutAsFails) { SamplingFailed(); }
			return;
		}

		bool bSuccess = false;

		const FVector2D UV = UVGetter->Read(Index);

		for (const TSharedRef<FSampler>& Sampler : Samplers)
		{
			if (Sampler->Sample(Index, Point, UV)) { bSuccess = true; }
		}

		if (!bSuccess)
		{
			SamplingFailed();
			return;
		}

		SampleState[Index] = bSuccess;
		FPlatformAtomics::InterlockedExchange(&bAnySuccess, 1);
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

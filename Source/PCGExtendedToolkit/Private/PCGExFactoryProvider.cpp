// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExFactoryProvider.h"

#include "PCGExContext.h"
#include "PCGExPointsProcessor.h"
#include "PCGPin.h"
#include "Tasks/Task.h"

#define LOCTEXT_NAMESPACE "PCGExFactoryProvider"
#define PCGEX_NAMESPACE PCGExFactoryProvider

void UPCGExParamDataBase::OutputConfigToMetadata()
{
}

bool UPCGExFactoryData::RegisterConsumableAttributes(FPCGExContext* InContext) const
{
	return bCleanupConsumableAttributes;
}

bool UPCGExFactoryData::RegisterConsumableAttributesWithData(FPCGExContext* InContext, const UPCGData* InData) const
{
	return bCleanupConsumableAttributes;
}

void UPCGExFactoryData::RegisterAssetDependencies(FPCGExContext* InContext) const
{
}

void UPCGExFactoryData::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
}

void UPCGExFactoryData::AddDataDependency(const UPCGData* InData)
{
	bool bAlreadyInSet = false;
	UPCGData* MutableData = const_cast<UPCGData*>(InData);
	DataDependencies.Add(MutableData, &bAlreadyInSet);
	if (!bAlreadyInSet) { MutableData->AddToRoot(); }
}

void UPCGExFactoryData::BeginDestroy()
{
	for (UPCGData* DataDependency : DataDependencies) { DataDependency->RemoveFromRoot(); }
	Super::BeginDestroy();
}

#if WITH_EDITOR
void UPCGExFactoryProviderSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	InternalCacheInvalidator++;
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

TArray<FPCGPinProperties> UPCGExFactoryProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExFactoryProviderSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_FACTORY(GetMainOutputPin(), GetMainOutputPin().ToString(), Required, {})
	return PinProperties;
}

FPCGElementPtr UPCGExFactoryProviderSettings::CreateElement() const
{
	return MakeShared<FPCGExFactoryProviderElement>();
}

#if WITH_EDITOR
FString UPCGExFactoryProviderSettings::GetDisplayName() const { return TEXT(""); }

#ifndef PCGEX_CUSTOM_PIN_DECL
#define PCGEX_CUSTOM_PIN_DECL
#define PCGEX_CUSTOM_PIN_ICON(_LABEL, _ICON, _TOOLTIP) if(PinLabel == _LABEL){ OutExtraIcon = TEXT("PCGEx.Pin." # _ICON); OutTooltip = FTEXT(_TOOLTIP); return true; }
#endif

bool UPCGExFactoryProviderSettings::GetPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip) const
{
	if (!GetDefault<UPCGExGlobalSettings>()->GetPinExtraIcon(InPin, OutExtraIcon, OutTooltip, true))
	{
		return GetDefault<UPCGExGlobalSettings>()->GetPinExtraIcon(InPin, OutExtraIcon, OutTooltip, false);
	}
	return true;
}


void UPCGExFactoryProviderSettings::EDITOR_OpenNodeDocumentation() const
{
	const FString URL = PCGEx::META_PCGExDocNodeLibraryBaseURL + GetClass()->GetMetaData(*PCGEx::META_PCGExDocURL);
	FPlatformProcess::LaunchURL(*URL, nullptr, nullptr);
}
#endif

bool UPCGExFactoryProviderSettings::ShouldCache() const
{
	if (!IsCacheable()) { return false; }
	PCGEX_GET_OPTION_STATE(CachingBehavior, bDefaultCacheNodeOutput)
}

FPCGExFactoryProviderContext::~FPCGExFactoryProviderContext()
{
	for (const TSharedPtr<PCGExMT::FDeferredCallbackHandle>& Task : DeferredTasks) { CancelDeferredCallback(Task); }
	DeferredTasks.Empty();
}

void FPCGExFactoryProviderContext::LaunchDeferredCallback(PCGExMT::FSimpleCallback&& InCallback)
{
	DeferredTasks.Add(PCGExMT::DeferredCallback(this, MoveTemp(InCallback)));
}

UPCGExFactoryData* UPCGExFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	InFactory->bCleanupConsumableAttributes = bCleanupConsumableAttributes;
	InFactory->bQuietMissingInputError = bQuietMissingInputError;
	return InFactory;
}

bool FPCGExFactoryProviderElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFactoryProviderElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(FactoryProvider)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->OutFactory = Settings->CreateFactory(Context, nullptr);

		if (!Context->OutFactory) { return true; }

		Context->OutFactory->OutputConfigToMetadata();

		if (Context->OutFactory->WantsPreparation(Context))
		{
			Context->SetAsyncState(PCGEx::State_WaitingOnAsyncWork);

			PCGEX_ASYNC_GROUP_CHKD(Context->GetAsyncManager(), Prepare)
			Prepare->AddSimpleCallback(
				[CtxHandle = Context->GetOrCreateHandle()]()
				{
					const FPCGContext::FSharedContext<FPCGExFactoryProviderContext> SharedContext(CtxHandle);
					FPCGExFactoryProviderContext* Ctx = SharedContext.Get();
					if (!Ctx) { return; }
					Ctx->OutFactory->bIsAsyncPreparationSuccessful = Ctx->OutFactory->Prepare(Ctx, Ctx->GetAsyncManager());
				});

			Prepare->StartSimpleCallbacks();

			return false;
		}
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGEx::State_WaitingOnAsyncWork)
	{
		if (!Context->OutFactory->bIsAsyncPreparationSuccessful)
		{
			Context->CancelExecution(TEXT(""));
			return true;
		}
	}

	Context->Done();

	// Register declared dependencies to root them
	TArray<FPCGPinProperties> InputPins = Settings->InputPinProperties();
	for (const FPCGPinProperties& Pin : InputPins)
	{
		const TArray<FPCGTaggedData>& InputData = Context->InputData.GetInputsByPin(Pin.Label);
		for (const FPCGTaggedData& TaggedData : InputData) { Context->OutFactory->AddDataDependency(TaggedData.Data); }
	}

	// We use a dummy attribute to update the factory CRC
	FPCGAttributeIdentifier CacheInvalidation(FName("PCGEx/CRC"), PCGMetadataDomainID::Data);
	Context->OutFactory->Metadata->CreateAttribute<int32>(CacheInvalidation, Settings->InternalCacheInvalidator, false, false);

	FPCGTaggedData& StagedData = Context->StageOutput(Context->OutFactory, false);
	StagedData.Pin = Settings->GetMainOutputPin();


	return Context->TryComplete();
}

bool FPCGExFactoryProviderElement::IsCacheable(const UPCGSettings* InSettings) const
{
	const UPCGExFactoryProviderSettings* Settings = static_cast<const UPCGExFactoryProviderSettings*>(InSettings);
	return Settings->ShouldCache();
}

#if WITH_EDITOR
void PCGExFactories::EDITOR_SortPins(UPCGSettings* InSettings, FName InPin)
{
}
#endif

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExFactoryProvider.h"
#include "PCGExContext.h"
#include "PCGPin.h"
#include "Tasks/Task.h"

#define LOCTEXT_NAMESPACE "PCGExFactoryProvider"
#define PCGEX_NAMESPACE PCGExFactoryProvider

void UPCGExParamDataBase::OutputConfigToMetadata()
{
}

void UPCGExFactoryData::InitializeFromPCGExData(const UPCGExPointData* InPCGExPointData, const PCGExData::EIOInit InitMode)
{
	Super::InitializeFromPCGExData(InPCGExPointData, InitMode);
	if (const UPCGExFactoryData* FactoryData = Cast<UPCGExFactoryData>(InPCGExPointData))
	{
		InitializeFromFactory(FactoryData);
	}
	else
	{
		HandleFailedInitializationFromFactory(InPCGExPointData);
	}
}

void UPCGExFactoryData::InitializeFromFactory(const UPCGExFactoryData* InFactoryData)
{
}

void UPCGExFactoryData::HandleFailedInitializationFromFactory(const UPCGPointData* InPointData)
{
	UE_LOG(LogTemp, Error, TEXT("Attempted to create a copy of a Factory from invalid or unrelated data!"));
}

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
	DeferredTasks.Add_GetRef(PCGExMT::DeferredCallback(this, MoveTemp(InCallback)));
}

UPCGExFactoryData* UPCGExFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	InFactory->bCleanupConsumableAttributes = bCleanupConsumableAttributes;
	InFactory->bQuietMissingInputError = bQuietMissingInputError;
	return InFactory;
}

bool FPCGExFactoryProviderElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExFactoryProviderElement::Execute);

	FPCGExFactoryProviderContext* InContext = static_cast<FPCGExFactoryProviderContext*>(Context);
	check(InContext);
	PCGEX_SETTINGS(FactoryProvider)

	if (!InContext->CanExecute()) { return true; }

	if (InContext->IsState(PCGEx::State_InitialExecution))
	{
		InContext->OutFactory = Settings->CreateFactory(InContext, nullptr);

		if (!InContext->OutFactory) { return true; }

		InContext->OutFactory->OutputConfigToMetadata();

		if (InContext->OutFactory->GetRequiresPreparation(InContext))
		{
			InContext->PauseContext();
			InContext->LaunchDeferredCallback(
				[CtxHandle = InContext->GetOrCreateHandle()]()
				{
					FPCGExFactoryProviderContext* Ctx = FPCGExContext::GetContextFromHandle<FPCGExFactoryProviderContext>(CtxHandle);
					if (!Ctx) { return; }

					if (!Ctx->OutFactory->Prepare(Ctx))
					{
						Ctx->CancelExecution(TEXT(""));
					}
					else
					{
						Ctx->Done();
						Ctx->ResumeExecution();
					}
				});

			InContext->SetState(PCGEx::State_WaitingOnAsyncWork);
			return false;
		}

		InContext->Done();
	}

	if (InContext->IsDone() && InContext->OutFactory)
	{
		InContext->StageOutput(Settings->GetMainOutputPin(), InContext->OutFactory, false);
	}

	return InContext->TryComplete();
}

bool FPCGExFactoryProviderElement::IsCacheable(const UPCGSettings* InSettings) const
{
	const UPCGExFactoryProviderSettings* Settings = static_cast<const UPCGExFactoryProviderSettings*>(InSettings);
	return Settings->ShouldCache();
}

FPCGContext* FPCGExFactoryProviderElement::Initialize(const FPCGDataCollection& InputData, const TWeakObjectPtr<UPCGComponent> SourceComponent, const UPCGNode* Node)
{
	FPCGExFactoryProviderContext* Context = new FPCGExFactoryProviderContext();
	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;
	Context->SetState(PCGEx::State_InitialExecution);
	return Context;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

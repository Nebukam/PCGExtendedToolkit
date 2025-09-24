// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExFactoryProvider.h"

#include "PCGExContext.h"
#include "PCGExGlobalSettings.h"
#include "Data/PCGExDataPreloader.h"
#include "PCGExPointsProcessor.h"
#include "Data/PCGExData.h"
#include "PCGPin.h"
#include "Tasks/Task.h"

#define LOCTEXT_NAMESPACE "PCGExFactoryProvider"
#define PCGEX_NAMESPACE PCGExFactoryProvider

PCG_DEFINE_TYPE_INFO(FPCGExFactoryDataTypeInfo, UPCGExFactoryData)

EPCGDataType FPCGExFactoryDataTypeInfo::GetAssociatedLegacyType() const
{
	return EPCGDataType::Param;
}

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
	PCGEX_PIN_FACTORY(GetMainOutputPin(), GetMainOutputPin().ToString(), Required, StaticClass())
	return PinProperties;
}

FPCGElementPtr UPCGExFactoryProviderSettings::CreateElement() const
{
	return MakeShared<FPCGExFactoryProviderElement>();
}

#if WITH_EDITOR
FString UPCGExFactoryProviderSettings::GetDisplayName() const { return TEXT(""); }
FLinearColor UPCGExFactoryProviderSettings::GetNodeTitleColor() const { return GetDefault<UPCGExGlobalSettings>()->NodeColorFilter; }

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
			Context->SetAsyncState(PCGExCommon::State_WaitingOnAsyncWork);
			Context->OutFactory->PrepResult = Context->OutFactory->Prepare(Context, Context->GetAsyncManager());
			if (Context->OutFactory->PrepResult == PCGExFactories::EPreparationResult::Success) { return false; }
		}
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExCommon::State_WaitingOnAsyncWork)
	{
		if (Context->OutFactory->PrepResult != PCGExFactories::EPreparationResult::Success)
		{
			if (Settings->ShouldCancel(Context, Context->OutFactory->PrepResult))
			{
				Context->CancelExecution(TEXT(""));
				return true;
			}
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

void FPCGExFactoryProviderElement::DisabledPassThroughData(FPCGContext* Context) const
{
	// Disabled factories should not output anything when disabled
	Context->OutputData.TaggedData.Empty();
}

namespace PCGExFactories
{
	bool GetInputFactories_Internal(FPCGExContext* InContext, const FName InLabel, TArray<TObjectPtr<const UPCGExFactoryData>>& OutFactories, const TSet<EType>& Types, const bool bThrowError)
	{
		const TArray<FPCGTaggedData>& Inputs = InContext->InputData.GetInputsByPin(InLabel);
		TSet<uint32> UniqueData;
		UniqueData.Reserve(Inputs.Num());

		for (const FPCGTaggedData& TaggedData : Inputs)
		{
			bool bIsAlreadyInSet;
			UniqueData.Add(TaggedData.Data->GetUniqueID(), &bIsAlreadyInSet);
			if (bIsAlreadyInSet) { continue; }

			const UPCGExFactoryData* Factory = Cast<UPCGExFactoryData>(TaggedData.Data);
			if (Factory)
			{
				if (!Types.Contains(Factory->GetFactoryType()))
				{
					PCGE_LOG_C(
						Warning, GraphAndLog, InContext,
						FText::Format(FTEXT("Input '{0}' is not supported."),
							FText::FromString(Factory->GetClass()->GetName())));
					continue;
				}

				OutFactories.AddUnique(Factory);
				Factory->RegisterAssetDependencies(InContext);
				Factory->RegisterConsumableAttributes(InContext);
			}
			else
			{
				PCGE_LOG_C(
					Warning, GraphAndLog, InContext,
					FText::Format(FTEXT("Input '{0}' is not supported."),
						FText::FromString(TaggedData.Data->GetClass()->GetName())));
			}
		}

		if (OutFactories.IsEmpty())
		{
			if (bThrowError)
			{
				PCGE_LOG_C(
					Error, GraphAndLog, InContext,
					FText::Format(FTEXT("Missing required '{0}' inputs."), FText::FromName(InLabel)));
			}
			return false;
		}

		OutFactories.Sort([](const UPCGExFactoryData& A, const UPCGExFactoryData& B) { return A.Priority < B.Priority; });

		return true;
	}

	void RegisterConsumableAttributesWithData_Internal(const TArray<TObjectPtr<const UPCGExFactoryData>>& InFactories, FPCGExContext* InContext, const UPCGData* InData)
	{
		check(InContext)

		if (!InData || InFactories.IsEmpty()) { return; }

		for (const TObjectPtr<const UPCGExFactoryData>& Factory : InFactories)
		{
			if (!Factory.Get()) { continue; }
			Factory->RegisterConsumableAttributesWithData(InContext, InData);
		}
	}

	void RegisterConsumableAttributesWithFacade_Internal(const TArray<TObjectPtr<const UPCGExFactoryData>>& InFactories, const TSharedPtr<PCGExData::FFacade>& InFacade)
	{
		FPCGContext::FSharedContext<FPCGExContext> SharedContext(InFacade->Source->GetContextHandle());
		check(SharedContext.Get())

		if (!InFacade->GetIn()) { return; }

		const UPCGData* Data = InFacade->GetIn();

		if (!Data) { return; }

		for (const TObjectPtr<const UPCGExFactoryData>& Factory : InFactories)
		{
			Factory->RegisterConsumableAttributesWithData(SharedContext.Get(), Data);
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

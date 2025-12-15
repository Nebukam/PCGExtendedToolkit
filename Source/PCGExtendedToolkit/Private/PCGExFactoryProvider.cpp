// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExFactoryProvider.h"

#include "PCGExContext.h"
#include "Data/PCGExDataPreloader.h"
#include "PCGExPointsProcessor.h"
#include "PCGPin.h"
#include "Data/PCGExPointIO.h"
#include "Tasks/Task.h"

#define LOCTEXT_NAMESPACE "PCGExFactoryProvider"
#define PCGEX_NAMESPACE PCGExFactoryProvider

PCG_DEFINE_TYPE_INFO(FPCGExFactoryDataTypeInfo, UPCGExFactoryData)

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

#if PCGEX_ENGINE_VERSION > 506
const FPCGDataTypeBaseId& UPCGExFactoryProviderSettings::GetFactoryTypeId() const
{
	return FPCGExFactoryDataTypeInfo::AsId();
}
#endif

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

	{
#if PCGEX_ENGINE_VERSION > 506
		FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(GetMainOutputPin(), GetFactoryTypeId(), false, false);
#else
		FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(GetMainOutputPin(), EPCGDataType::Param, false, false);
#endif
		PCGEX_PIN_TOOLTIP(GetMainOutputPin().ToString())
		PCGEX_PIN_STATUS(Required)
	}

	return PinProperties;
}

FPCGElementPtr UPCGExFactoryProviderSettings::CreateElement() const
{
	return MakeShared<FPCGExFactoryProviderElement>();
}

#if WITH_EDITOR
FString UPCGExFactoryProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

UPCGExFactoryData* UPCGExFactoryProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExFactoryData* InFactory) const
{
	InFactory->bCleanupConsumableAttributes = bCleanupConsumableAttributes;
	return InFactory;
}

bool FPCGExFactoryProviderElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
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
			TSharedPtr<PCGExMT::FTaskManager> TaskManager = Context->GetTaskManager();
			PCGEX_SCHEDULING_SCOPE(TaskManager, true)
			Context->OutFactory->PrepResult = Context->OutFactory->Prepare(Context, TaskManager);
			return false;
		}
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExCommon::State_WaitingOnAsyncWork)
	{
		if (Context->OutFactory->PrepResult != PCGExFactories::EPreparationResult::Success)
		{
			if (Settings->ShouldCancel(Context, Context->OutFactory->PrepResult))
			{
				return Context->CancelExecution();
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

void FPCGExFactoryProviderElement::DisabledPassThroughData(FPCGContext* Context) const
{
	// Disabled factories should not output anything when disabled
	Context->OutputData.TaggedData.Empty();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

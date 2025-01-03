// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Actions/PCGExActionWriteValues.h"

#include "PCGPin.h"


#define LOCTEXT_NAMESPACE "PCGExWriteActionWriteValuess"
#define PCGEX_NAMESPACE PCGExWriteActionWriteValuess


void UPCGExActionWriteValuesOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	// if (const UPCGExActionWriteValuesOperation* TypedOther = Cast<UPCGExActionWriteValuesOperation>(Other))	{	}
}

bool UPCGExActionWriteValuesOperation::PrepareForData(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	if (!Super::PrepareForData(InContext, InPointDataFacade)) { return false; }

	for (FPCGMetadataAttributeBase* AttributeBase : TypedFactory->CheckSuccessInfos->Attributes)
	{
		PCGEx::ExecuteWithRightType(
			AttributeBase->GetTypeId(), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				const FPCGMetadataAttribute<T>* TypedAttribute = static_cast<FPCGMetadataAttribute<T>*>(AttributeBase);
				TSharedPtr<PCGExData::TBuffer<T>> Writer = InPointDataFacade->GetWritable<T>(TypedAttribute, PCGExData::EBufferInit::Inherit);
				SuccessAttributes.Add(AttributeBase);
				SuccessWriters.Add(Writer);
			});
	}

	for (FPCGMetadataAttributeBase* AttributeBase : TypedFactory->CheckFailInfos->Attributes)
	{
		PCGEx::ExecuteWithRightType(
			AttributeBase->GetTypeId(), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				const FPCGMetadataAttribute<T>* TypedAttribute = static_cast<FPCGMetadataAttribute<T>*>(AttributeBase);
				TSharedPtr<PCGExData::TBuffer<T>> Writer = InPointDataFacade->GetWritable<T>(TypedAttribute, PCGExData::EBufferInit::Inherit);
				FailAttributes.Add(AttributeBase);
				FailWriters.Add(Writer);
			});
	}

	return true;
}

void UPCGExActionWriteValuesOperation::OnMatchSuccess(int32 Index, const FPCGPoint& Point)
{
	for (int i = 0; i < SuccessAttributes.Num(); i++)
	{
		FPCGMetadataAttributeBase* AttributeBase = SuccessAttributes[i];
		PCGEx::ExecuteWithRightType(
			AttributeBase->GetTypeId(), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				static_cast<PCGExData::TBuffer<T>*>(SuccessWriters[i].Get())->GetMutable(Index) = static_cast<FPCGMetadataAttribute<T>*>(AttributeBase)->GetValue(PCGDefaultValueKey);
			});
	}
}

void UPCGExActionWriteValuesOperation::OnMatchFail(int32 Index, const FPCGPoint& Point)
{
	for (int i = 0; i < FailAttributes.Num(); i++)
	{
		FPCGMetadataAttributeBase* AttributeBase = FailAttributes[i];
		PCGEx::ExecuteWithRightType(
			AttributeBase->GetTypeId(), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				static_cast<PCGExData::TBuffer<T>*>(FailWriters[i].Get())->GetMutable(Index) = static_cast<FPCGMetadataAttribute<T>*>(AttributeBase)->GetValue(PCGDefaultValueKey);
			});
	}
}

void UPCGExActionWriteValuesOperation::Cleanup()
{
	SuccessAttributes.Empty();
	SuccessWriters.Empty();
	FailAttributes.Empty();
	FailWriters.Empty();
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExActionWriteValuesProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

PCGEX_BITMASK_TRANSMUTE_CREATE_OPERATION(ActionWriteValues, {})

bool UPCGExActionWriteValuesFactory::Boot(FPCGContext* InContext)
{
	// Gather success/fail attributes

	SuccessAttributesFilter.bPreservePCGExData = false;
	FailAttributesFilter.bPreservePCGExData = false;

	SuccessAttributesFilter.Init();
	FailAttributesFilter.Init();

	CheckSuccessInfos = PCGEx::GatherAttributeInfos(InContext, PCGExActionWriteValues::SourceForwardSuccess, SuccessAttributesFilter, true);
	CheckFailInfos = PCGEx::GatherAttributeInfos(InContext, PCGExActionWriteValues::SourceForwardFail, FailAttributesFilter, true);

	if (!CheckSuccessInfos || !CheckFailInfos) { return false; }

	return true;
}

TArray<FPCGPinProperties> UPCGExActionWriteValuesProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_ANY(PCGExActionWriteValues::SourceForwardSuccess, "TBD", Normal, {})
	PCGEX_PIN_ANY(PCGExActionWriteValues::SourceForwardFail, "TBD", Normal, {})
	return PinProperties;
}

PCGEX_BITMASK_TRANSMUTE_CREATE_FACTORY(
	ActionWriteValues, {
	NewFactory->SuccessAttributesFilter = SuccessAttributesFilter;
	NewFactory->FailAttributesFilter = FailAttributesFilter;
	})


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

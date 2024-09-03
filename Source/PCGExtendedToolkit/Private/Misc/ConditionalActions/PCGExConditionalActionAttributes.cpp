// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/ConditionalActions/PCGExConditionalActionAttributes.h"

#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExWriteConditionalActionAttributess"
#define PCGEX_NAMESPACE PCGExWriteConditionalActionAttributess


void UPCGExConditionalActionAttributesOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExConditionalActionAttributesOperation* TypedOther = Cast<UPCGExConditionalActionAttributesOperation>(Other))
	{
	}
}

bool UPCGExConditionalActionAttributesOperation::PrepareForData(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade)
{
	if (!Super::PrepareForData(InContext, InPointDataFacade)) { return false; }

	for (FPCGMetadataAttributeBase* AttributeBase : TypedFactory->CheckSuccessInfos->Attributes)
	{
		PCGMetadataAttribute::CallbackWithRightType(
			static_cast<uint16>(AttributeBase->GetTypeId()), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				const FPCGMetadataAttribute<T>* TypedAttribute = static_cast<FPCGMetadataAttribute<T>*>(AttributeBase);
				PCGEx::TAttributeIO<T>* Writer = InPointDataFacade->GetWriter<T>(TypedAttribute, false);
				SuccessAttributes.Add(AttributeBase);
				SuccessWriters.Add(Writer);
			});
	}

	for (FPCGMetadataAttributeBase* AttributeBase : TypedFactory->CheckFailInfos->Attributes)
	{
		PCGMetadataAttribute::CallbackWithRightType(
			static_cast<uint16>(AttributeBase->GetTypeId()), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				const FPCGMetadataAttribute<T>* TypedAttribute = static_cast<FPCGMetadataAttribute<T>*>(AttributeBase);
				PCGEx::TAttributeIO<T>* Writer = InPointDataFacade->GetWriter<T>(TypedAttribute, false);
				FailAttributes.Add(AttributeBase);
				FailWriters.Add(Writer);
			});
	}

	return true;
}

void UPCGExConditionalActionAttributesOperation::OnMatchSuccess(int32 Index, const FPCGPoint& Point)
{
	for (int i = 0; i < SuccessAttributes.Num(); i++)
	{
		FPCGMetadataAttributeBase* AttributeBase = SuccessAttributes[i];
		PCGMetadataAttribute::CallbackWithRightType(
			static_cast<uint16>(AttributeBase->GetTypeId()), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				static_cast<PCGEx::TAttributeIO<T>*>(SuccessWriters[i])->Values[Index] = static_cast<FPCGMetadataAttribute<T>*>(AttributeBase)->GetValue(PCGDefaultValueKey);
			});
	}
}

void UPCGExConditionalActionAttributesOperation::OnMatchFail(int32 Index, const FPCGPoint& Point)
{
	for (int i = 0; i < FailAttributes.Num(); i++)
	{
		FPCGMetadataAttributeBase* AttributeBase = FailAttributes[i];
		PCGMetadataAttribute::CallbackWithRightType(
			static_cast<uint16>(AttributeBase->GetTypeId()), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				static_cast<PCGEx::TAttributeIO<T>*>(FailWriters[i])->Values[Index] = static_cast<FPCGMetadataAttribute<T>*>(AttributeBase)->GetValue(PCGDefaultValueKey);
			});
	}
}

void UPCGExConditionalActionAttributesOperation::Cleanup()
{
	SuccessAttributes.Empty();
	SuccessWriters.Empty();
	FailAttributes.Empty();
	FailWriters.Empty();
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExConditionalActionAttributesProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

PCGEX_BITMASK_TRANSMUTE_CREATE_OPERATION(ConditionalActionAttributes, {})

bool UPCGExConditionalActionAttributesFactory::Boot(FPCGContext* InContext)
{
	// Gather success/fail attributes

	SuccessAttributesFilter.bPreservePCGExData = false;
	FailAttributesFilter.bPreservePCGExData = false;

	SuccessAttributesFilter.Init();
	FailAttributesFilter.Init();

	CheckSuccessInfos = PCGEx::GatherAttributeInfos(InContext, PCGExConditionalActionAttribute::SourceForwardSuccess, SuccessAttributesFilter, true);
	CheckFailInfos = PCGEx::GatherAttributeInfos(InContext, PCGExConditionalActionAttribute::SourceForwardFail, FailAttributesFilter, true);

	if (!CheckSuccessInfos || !CheckFailInfos) { return false; }

	return true;
}

TArray<FPCGPinProperties> UPCGExConditionalActionAttributesProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_ANY(PCGExConditionalActionAttribute::SourceForwardSuccess, "TBD", Normal, {})
	PCGEX_PIN_ANY(PCGExConditionalActionAttribute::SourceForwardFail, "TBD", Normal, {})
	return PinProperties;
}

PCGEX_BITMASK_TRANSMUTE_CREATE_FACTORY(
	ConditionalActionAttributes, {
	NewFactory->SuccessAttributesFilter = SuccessAttributesFilter;
	NewFactory->FailAttributesFilter = FailAttributesFilter;
	})


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

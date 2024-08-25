// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/ConditionalActions/PCGExConditionalActionDataTable.h"

#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExWriteConditionalActionDataTable"
#define PCGEX_NAMESPACE PCGExWriteConditionalActionDataTables


void UPCGExConditionalActionDataTableOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExConditionalActionDataTableOperation* TypedOther = Cast<UPCGExConditionalActionDataTableOperation>(Other))
	{
	}
}

bool UPCGExConditionalActionDataTableOperation::PrepareForData(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade)
{
	if (!Super::PrepareForData(InContext, InPointDataFacade)) { return false; }

	for (FPCGMetadataAttributeBase* AttributeBase : TypedFactory->CheckSuccessInfos->Attributes)
	{
		PCGMetadataAttribute::CallbackWithRightType(
			static_cast<uint16>(AttributeBase->GetTypeId()), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				const FPCGMetadataAttribute<T>* TypedAttribute = static_cast<FPCGMetadataAttribute<T>*>(AttributeBase);
				PCGEx::FAttributeIOBase<T>* Writer = InPointDataFacade->GetWriter<T>(TypedAttribute, false);
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
				PCGEx::FAttributeIOBase<T>* Writer = InPointDataFacade->GetWriter<T>(TypedAttribute, false);
				FailAttributes.Add(AttributeBase);
				FailWriters.Add(Writer);
			});
	}

	return true;
}

void UPCGExConditionalActionDataTableOperation::OnMatchSuccess(int32 Index, const FPCGPoint& Point)
{
	for (int i = 0; i < SuccessAttributes.Num(); i++)
	{
		FPCGMetadataAttributeBase* AttributeBase = SuccessAttributes[i];
		PCGMetadataAttribute::CallbackWithRightType(
			static_cast<uint16>(AttributeBase->GetTypeId()), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				static_cast<PCGEx::FAttributeIOBase<T>*>(SuccessWriters[i])->Values[Index] = static_cast<FPCGMetadataAttribute<T>*>(AttributeBase)->GetValue(PCGDefaultValueKey);
			});
	}
}

void UPCGExConditionalActionDataTableOperation::OnMatchFail(int32 Index, const FPCGPoint& Point)
{
	for (int i = 0; i < FailAttributes.Num(); i++)
	{
		FPCGMetadataAttributeBase* AttributeBase = FailAttributes[i];
		PCGMetadataAttribute::CallbackWithRightType(
			static_cast<uint16>(AttributeBase->GetTypeId()), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				static_cast<PCGEx::FAttributeIOBase<T>*>(FailWriters[i])->Values[Index] = static_cast<FPCGMetadataAttribute<T>*>(AttributeBase)->GetValue(PCGDefaultValueKey);
			});
	}
}

void UPCGExConditionalActionDataTableOperation::Cleanup()
{
	SuccessAttributes.Empty();
	SuccessWriters.Empty();
	FailAttributes.Empty();
	FailWriters.Empty();
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExConditionalActionDataTableProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

PCGEX_BITMASK_TRANSMUTE_CREATE_OPERATION(ConditionalActionDataTable, {})

bool UPCGExConditionalActionDataTableFactory::Boot(FPCGContext* InContext)
{
	if (!DataTable) { return false; }
	return true;
}

TArray<FPCGPinProperties> UPCGExConditionalActionDataTableProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_ANY(PCGExConditionalActionDataTable::SourceForwardSuccess, "TBD", Normal, {})
	PCGEX_PIN_ANY(PCGExConditionalActionDataTable::SourceForwardFail, "TBD", Normal, {})
	return PinProperties;
}

PCGEX_BITMASK_TRANSMUTE_CREATE_FACTORY(
	ConditionalActionDataTable, {
	//PCGEX_LOAD_SOFTOBJECT(UDataTable, DataTable, NewFactory->DataTable, "None");
	})


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

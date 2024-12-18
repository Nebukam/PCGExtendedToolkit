// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExOperation.h"
#include "PCGParamData.h"

void UPCGExOperation::BindContext(FPCGExContext* InContext)
{
	Context = InContext;
}

void UPCGExOperation::FindSettingsOverrides(FPCGExContext* InContext, FName InPinLabel)
{
	TArray<FPCGTaggedData> OverrideParams = Context->InputData.GetParamsByPin(InPinLabel);
	for (FPCGTaggedData& InTaggedData : OverrideParams)
	{
		const UPCGParamData* ParamData = Cast<UPCGParamData>(InTaggedData.Data);

		if (!ParamData) { continue; }
		const TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(ParamData->Metadata);

		for (PCGEx::FAttributeIdentity& Identity : Infos->Identities)
		{
			PossibleOverrides.Add(Identity.Name, ParamData->Metadata->GetMutableAttribute(Identity.Name));
		}
	}

	ApplyOverrides();
	PossibleOverrides.Empty();
}

#if WITH_EDITOR
void UPCGExOperation::UpdateUserFacingInfos()
{
}
#endif

void UPCGExOperation::Cleanup()
{
	Context = nullptr;
	PrimaryDataFacade.Reset();
	SecondaryDataFacade.Reset();
}

void UPCGExOperation::BeginDestroy()
{
	Cleanup();
	UObject::BeginDestroy();
}

void UPCGExOperation::ApplyOverrides()
{
	UClass* ObjectClass = GetClass();

	for (const TPair<FName, FPCGMetadataAttributeBase*>& PossibleOverride : PossibleOverrides)
	{
		// Find the property by name
		FProperty* Property = ObjectClass->FindPropertyByName(PossibleOverride.Key);
		if (!Property) { continue; }

		PCGEx::ExecuteWithRightType(
			PossibleOverride.Value->GetTypeId(), [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				const FPCGMetadataAttribute<T>* TypedAttribute = static_cast<FPCGMetadataAttribute<T>*>(PossibleOverride.Value);
				PCGEx::TrySetFPropertyValue<T>(this, Property, TypedAttribute->GetValue(0));
			});
	}
}

void UPCGExOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	BindContext(Other->Context);

	check(GetClass() == Other->GetClass())

	// Get the class type
	UClass* Class = Other->GetClass();

	// Iterate over properties
	for (TFieldIterator<FProperty> It(Class); It; ++It)
	{
		const FProperty* Property = *It;

		// Skip properties that shouldn't be copied (like transient properties)
		if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_ConstParm | CPF_OutParm)) { continue; }

		// Copy the value from source to target
		const void* SourceValue = Property->ContainerPtrToValuePtr<void>(Other);
		void* TargetValue = Property->ContainerPtrToValuePtr<void>(this);
		Property->CopyCompleteValue(TargetValue, SourceValue);
	}
}

void UPCGExOperation::RegisterAssetDependencies(FPCGExContext* InContext)
{
	//InContext->AddAssetDependency();
}

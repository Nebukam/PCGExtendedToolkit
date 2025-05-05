// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExInstancedOperation.h"
#include "PCGParamData.h"

void UPCGExInstancedOperation::BindContext(FPCGExContext* InContext)
{
	Context = InContext;
}

void UPCGExInstancedOperation::FindSettingsOverrides(FPCGExContext* InContext, FName InPinLabel)
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
void UPCGExInstancedOperation::UpdateUserFacingInfos()
{
}
#endif

void UPCGExInstancedOperation::Cleanup()
{
	Context = nullptr;
	PrimaryDataFacade.Reset();
	SecondaryDataFacade.Reset();
}

void UPCGExInstancedOperation::RegisterConsumableAttributesWithFacade(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InFacade) const
{
}

void UPCGExInstancedOperation::RegisterPrimaryBuffersDependencies(PCGExData::FFacadePreloader& FacadePreloader) const
{
}

void UPCGExInstancedOperation::BeginDestroy()
{
	Cleanup();
	UObject::BeginDestroy();
}

void UPCGExInstancedOperation::ApplyOverrides()
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

void UPCGExInstancedOperation::CopySettingsFrom(const UPCGExInstancedOperation* Other)
{
	BindContext(Other->Context);
	PCGExHelpers::CopyProperties(this, Other);
}

void UPCGExInstancedOperation::RegisterAssetDependencies(FPCGExContext* InContext)
{
	//InContext->AddAssetDependency();
}

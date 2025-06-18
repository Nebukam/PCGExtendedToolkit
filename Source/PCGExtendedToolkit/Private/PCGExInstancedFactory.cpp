// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExInstancedFactory.h"
#include "PCGParamData.h"

void UPCGExInstancedFactory::BindContext(FPCGExContext* InContext)
{
	Context = InContext;
}

void UPCGExInstancedFactory::InitializeInContext(FPCGExContext* InContext, FName InOverridesPinLabel)
{
	FindSettingsOverrides(InContext, InOverridesPinLabel);
}

void UPCGExInstancedFactory::FindSettingsOverrides(FPCGExContext* InContext, FName InPinLabel)
{
	TArray<FPCGTaggedData> OverrideParams = InContext->InputData.GetParamsByPin(InPinLabel);
	for (FPCGTaggedData& InTaggedData : OverrideParams)
	{
		const UPCGParamData* ParamData = Cast<UPCGParamData>(InTaggedData.Data);

		if (!ParamData) { continue; }
		const TSharedPtr<PCGEx::FAttributesInfos> Infos = PCGEx::FAttributesInfos::Get(ParamData->Metadata);

		for (PCGEx::FAttributeIdentity& Identity : Infos->Identities)
		{
			PossibleOverrides.Add(Identity.Identifier.Name, ParamData->Metadata->GetMutableAttribute(Identity.Identifier));
		}
	}

	ApplyOverrides();
	PossibleOverrides.Empty();
}

#if WITH_EDITOR
void UPCGExInstancedFactory::UpdateUserFacingInfos()
{
}
#endif

void UPCGExInstancedFactory::Cleanup()
{
	Context = nullptr;
	PrimaryDataFacade.Reset();
	SecondaryDataFacade.Reset();
}

void UPCGExInstancedFactory::RegisterConsumableAttributesWithFacade(FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InFacade) const
{
}

void UPCGExInstancedFactory::RegisterPrimaryBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
}

void UPCGExInstancedFactory::BeginDestroy()
{
	Cleanup();
	UObject::BeginDestroy();
}

void UPCGExInstancedFactory::ApplyOverrides()
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

void UPCGExInstancedFactory::CopySettingsFrom(const UPCGExInstancedFactory* Other)
{
	BindContext(Other->Context);
	PCGExHelpers::CopyProperties(this, Other);
}

void UPCGExInstancedFactory::RegisterAssetDependencies(FPCGExContext* InContext)
{
	//InContext->AddAssetDependency();
}

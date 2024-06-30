// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Transmogs/PCGExBitmaskTransmog.h"

#define LOCTEXT_NAMESPACE "PCGExWriteBitmaskTransmogs"
#define PCGEX_NAMESPACE PCGExWriteBitmaskTransmogs


void UPCGExBitmaskTransmogOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExBitmaskTransmogOperation* TypedOther = Cast<UPCGExBitmaskTransmogOperation>(Other))
	{
	}
}

bool UPCGExBitmaskTransmogOperation::PrepareForData(const FPCGContext* InContext, PCGExData::FFacade* InPointDataCache)
{
	PrimaryDataCache = InPointDataCache;
	return true;
}

void UPCGExBitmaskTransmogOperation::ProcessPoint(const FPCGPoint& Point, int64& Flags)
{
}

void UPCGExBitmaskTransmogOperation::Cleanup()
{
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExBitmaskTransmogProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

PCGExFactories::EType UPCGExBitmaskTransmogFactoryBase::GetFactoryType() const { return PCGExFactories::EType::BitmaskTransmog; }

UPCGExBitmaskTransmogOperation* UPCGExBitmaskTransmogFactoryBase::CreateOperation() const
{
	UPCGExBitmaskTransmogOperation* NewOperation = NewObject<UPCGExBitmaskTransmogOperation>();
	NewOperation->Factory = const_cast<UPCGExBitmaskTransmogFactoryBase*>(this);
	return NewOperation;
}

bool UPCGExBitmaskTransmogFactoryBase::Boot(FPCGContext* InContext)
{
	return true;
}

bool UPCGExBitmaskTransmogFactoryBase::AppendAndValidate(PCGEx::FAttributesInfos* InInfos, FString& OutMessage)
{
	TSet<FName> Mismatch;
	if (CheckSuccessInfos) { InInfos->Append(CheckSuccessInfos, InputAttributesFilter, Mismatch); }
	if (!Mismatch.IsEmpty())
	{
		for (const FName& MismatchName : Mismatch)
		{
			OutMessage = FText::Format(FText::FromString(TEXT("Attribute {0} is referenced multiple times but has different types.")), FText::FromName(MismatchName)).ToString();
		}
		return false;
	}

	if (CheckFailInfos) { InInfos->Append(CheckFailInfos, InputAttributesFilter, Mismatch); }
	if (!Mismatch.IsEmpty())
	{
		for (const FName& MismatchName : Mismatch)
		{
			OutMessage = FText::Format(FText::FromString(TEXT("Attribute {0} is referenced multiple times but has different types.")), FText::FromName(MismatchName)).ToString();
		}
		return false;
	}

	return true;
}

void UPCGExBitmaskTransmogFactoryBase::BeginDestroy()
{
	PCGEX_DELETE(CheckSuccessInfos)
	PCGEX_DELETE(CheckFailInfos)

	Super::BeginDestroy();
}

TArray<FPCGPinProperties> UPCGExBitmaskTransmogProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	//PCGEX_PIN_PARAMS(PCGEx::SourcePointFilters, "Filters used to check which node will be processed by the sampler or not.", Advanced, {})
	//PCGEX_PIN_PARAMS(PCGEx::SourceUseValueIfFilters, "Filters used to check if a node can be used as a value source or not.", Advanced, {})
	return PinProperties;
}

FName UPCGExBitmaskTransmogProviderSettings::GetMainOutputLabel() const { return PCGExBitmaskTransmog::OutputTransmogLabel; }

UPCGExParamFactoryBase* UPCGExBitmaskTransmogProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExBitmaskTransmogFactoryBase* TypedFactory = Cast<UPCGExBitmaskTransmogFactoryBase>(InFactory);

	if (TypedFactory)
	{
		TypedFactory->Priority = Priority;
		if (!TypedFactory->Boot(InContext))
		{
			PCGEX_DELETE(TypedFactory)
			return nullptr;
		}
	}
	else
	{
		PCGEX_DELETE(InFactory)
		return nullptr;
	}

	return InFactory;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

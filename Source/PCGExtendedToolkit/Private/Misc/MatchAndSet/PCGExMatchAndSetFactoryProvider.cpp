// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/MatchAndSet/PCGExMatchAndSetFactoryProvider.h"

#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExWriteMatchAndSets"
#define PCGEX_NAMESPACE PCGExWriteMatchAndSets


void UPCGExMatchAndSetOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExMatchAndSetOperation* TypedOther = Cast<UPCGExMatchAndSetOperation>(Other))
	{
		Factory = TypedOther->Factory;
	}
}

bool UPCGExMatchAndSetOperation::PrepareForData(const FPCGContext* InContext, PCGExData::FFacade* InPointDataCache)
{
	PrimaryDataCache = InPointDataCache;

	FilterManager = new PCGExPointFilter::TManager(PrimaryDataCache);
	FilterManager->bCacheResults = false;
	FilterManager->bCacheResultsPerFilter = false;

	if (!FilterManager->Init(InContext, Factory->FilterFactories)) { return false; }

	return true;
}

void UPCGExMatchAndSetOperation::ProcessPoint(int32 Index, const FPCGPoint& Point)
{
	if (FilterManager->Test(Index)) { OnMatchSuccess(Index, Point); }
	else { OnMatchFail(Index, Point); }
}

void UPCGExMatchAndSetOperation::OnMatchSuccess(int32 Index, const FPCGPoint& Point)
{
}

void UPCGExMatchAndSetOperation::OnMatchFail(int32 Index, const FPCGPoint& Point)
{
}

void UPCGExMatchAndSetOperation::Cleanup()
{
	PCGEX_DELETE(FilterManager)
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExMatchAndSetProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

PCGExFactories::EType UPCGExMatchAndSetFactoryBase::GetFactoryType() const { return PCGExFactories::EType::MatchAndSet; }

UPCGExMatchAndSetOperation* UPCGExMatchAndSetFactoryBase::CreateOperation() const
{
	UPCGExMatchAndSetOperation* NewOperation = NewObject<UPCGExMatchAndSetOperation>();
	NewOperation->Factory = const_cast<UPCGExMatchAndSetFactoryBase*>(this);
	return NewOperation;
}

bool UPCGExMatchAndSetFactoryBase::Boot(FPCGContext* InContext)
{
	return true;
}

bool UPCGExMatchAndSetFactoryBase::AppendAndValidate(PCGEx::FAttributesInfos* InInfos, FString& OutMessage)
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

void UPCGExMatchAndSetFactoryBase::BeginDestroy()
{
	PCGEX_DELETE(CheckSuccessInfos)
	PCGEX_DELETE(CheckFailInfos)

	Super::BeginDestroy();
}

TArray<FPCGPinProperties> UPCGExMatchAndSetProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExMatchAndSet::SourceMatchFilterLabel, "Filters used to define if there's a match or not.", Required, {})
	return PinProperties;
}

FName UPCGExMatchAndSetProviderSettings::GetMainOutputLabel() const { return PCGExMatchAndSet::OutputMatchAndSetLabel; }

UPCGExParamFactoryBase* UPCGExMatchAndSetProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExMatchAndSetFactoryBase* TypedFactory = Cast<UPCGExMatchAndSetFactoryBase>(InFactory);

	auto Exit = [&]()
	{
		delete InFactory;
		PCGEX_DELETE(TypedFactory)
		return nullptr;
	};

	if (TypedFactory)
	{
		if (!PCGExFactories::GetInputFactories(
			InContext, PCGExMatchAndSet::SourceMatchFilterLabel, TypedFactory->FilterFactories,
			{PCGExFactories::EType::FilterPoint}, true))
		{
			return Exit();
		}

		TypedFactory->Priority = Priority;

		if (!TypedFactory->Boot(InContext)) { return Exit(); }
	}
	else
	{
		return Exit();
	}

	return InFactory;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

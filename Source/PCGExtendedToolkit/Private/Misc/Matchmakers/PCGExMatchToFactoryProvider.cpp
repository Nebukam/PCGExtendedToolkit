// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/Matchmakers/PCGExMatchToFactoryProvider.h"

#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExWriteMatchTos"
#define PCGEX_NAMESPACE PCGExWriteMatchTos


void UPCGExMatchToOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExMatchToOperation* TypedOther = Cast<UPCGExMatchToOperation>(Other))
	{
		Factory = TypedOther->Factory;
	}
}

bool UPCGExMatchToOperation::PrepareForData(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade)
{
	PrimaryDataFacade = InPointDataFacade;

	FilterManager = new PCGExPointFilter::TManager(PrimaryDataFacade);
	FilterManager->bCacheResults = false;
	FilterManager->bCacheResultsPerFilter = false;

	if (!FilterManager->Init(InContext, Factory->FilterFactories)) { return false; }

	return true;
}

void UPCGExMatchToOperation::ProcessPoint(int32 Index, const FPCGPoint& Point)
{
	if (FilterManager->Test(Index)) { OnMatchSuccess(Index, Point); }
	else { OnMatchFail(Index, Point); }
}

void UPCGExMatchToOperation::OnMatchSuccess(int32 Index, const FPCGPoint& Point)
{
}

void UPCGExMatchToOperation::OnMatchFail(int32 Index, const FPCGPoint& Point)
{
}

void UPCGExMatchToOperation::Cleanup()
{
	PCGEX_DELETE(FilterManager)
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExMatchToProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

UPCGExMatchToOperation* UPCGExMatchToFactoryBase::CreateOperation() const
{
	UPCGExMatchToOperation* NewOperation = NewObject<UPCGExMatchToOperation>();
	NewOperation->Factory = const_cast<UPCGExMatchToFactoryBase*>(this);
	return NewOperation;
}

bool UPCGExMatchToFactoryBase::Boot(FPCGContext* InContext)
{
	return true;
}

bool UPCGExMatchToFactoryBase::AppendAndValidate(PCGEx::FAttributesInfos* InInfos, FString& OutMessage)
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

void UPCGExMatchToFactoryBase::BeginDestroy()
{
	PCGEX_DELETE(CheckSuccessInfos)
	PCGEX_DELETE(CheckFailInfos)

	Super::BeginDestroy();
}

TArray<FPCGPinProperties> UPCGExMatchToProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExMatchmaking::SourceMatchFilterLabel, "Filters used to define if there's a match or not.", Required, {})
	return PinProperties;
}

UPCGExParamFactoryBase* UPCGExMatchToProviderSettings::CreateFactory(FPCGContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExMatchToFactoryBase* TypedFactory = Cast<UPCGExMatchToFactoryBase>(InFactory);

	auto Exit = [&]()
	{
		delete InFactory;
		PCGEX_DELETE(TypedFactory)
		return nullptr;
	};

	if (TypedFactory)
	{
		if (!GetInputFactories(
			InContext, PCGExMatchmaking::SourceMatchFilterLabel, TypedFactory->FilterFactories,
			PCGExFactories::PointFilters, true))
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

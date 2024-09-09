// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/ConditionalActions/PCGExConditionalActionFactoryProvider.h"

#include "PCGPin.h"

#define LOCTEXT_NAMESPACE "PCGExWriteConditionalActions"
#define PCGEX_NAMESPACE PCGExWriteConditionalActions


void UPCGExConditionalActionOperation::CopySettingsFrom(const UPCGExOperation* Other)
{
	Super::CopySettingsFrom(Other);
	if (const UPCGExConditionalActionOperation* TypedOther = Cast<UPCGExConditionalActionOperation>(Other))
	{
		Factory = TypedOther->Factory;
	}
}

bool UPCGExConditionalActionOperation::PrepareForData(const FPCGContext* InContext, PCGExData::FFacade* InPointDataFacade)
{
	PrimaryDataFacade = InPointDataFacade;

	FilterManager = new PCGExPointFilter::TManager(PrimaryDataFacade);

	if (!FilterManager->Init(InContext, Factory->FilterFactories)) { return false; }

	return true;
}

void UPCGExConditionalActionOperation::ProcessPoint(const int32 Index, const FPCGPoint& Point)
{
	if (FilterManager->Test(Index)) { OnMatchSuccess(Index, Point); }
	else { OnMatchFail(Index, Point); }
}

void UPCGExConditionalActionOperation::OnMatchSuccess(int32 Index, const FPCGPoint& Point)
{
}

void UPCGExConditionalActionOperation::OnMatchFail(int32 Index, const FPCGPoint& Point)
{
}

void UPCGExConditionalActionOperation::Cleanup()
{
	PCGEX_DELETE(FilterManager)
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExConditionalActionProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

UPCGExConditionalActionOperation* UPCGExConditionalActionFactoryBase::CreateOperation() const
{
	PCGEX_NEW_TRANSIENT(UPCGExConditionalActionOperation, NewOperation)
	NewOperation->Factory = const_cast<UPCGExConditionalActionFactoryBase*>(this);
	return NewOperation;
}

bool UPCGExConditionalActionFactoryBase::Boot(FPCGContext* InContext)
{
	return true;
}

bool UPCGExConditionalActionFactoryBase::AppendAndValidate(PCGEx::FAttributesInfos* InInfos, FString& OutMessage)
{
	TSet<FName> Mismatch;

	const FPCGExAttributeGatherDetails GatherDetails = FPCGExAttributeGatherDetails(); // Required for Append

	if (CheckSuccessInfos) { InInfos->Append(CheckSuccessInfos, GatherDetails, Mismatch); }
	if (!Mismatch.IsEmpty())
	{
		for (const FName& MismatchName : Mismatch)
		{
			OutMessage = FText::Format(FText::FromString(TEXT("Attribute \"{0}\" is referenced multiple times but has different types.")), FText::FromName(MismatchName)).ToString();
		}
		return false;
	}

	if (CheckFailInfos) { InInfos->Append(CheckFailInfos, GatherDetails, Mismatch); }
	if (!Mismatch.IsEmpty())
	{
		for (const FName& MismatchName : Mismatch)
		{
			OutMessage = FText::Format(FText::FromString(TEXT("Attribute \"{0}\" is referenced multiple times but has different types.")), FText::FromName(MismatchName)).ToString();
		}
		return false;
	}

	return true;
}

void UPCGExConditionalActionFactoryBase::BeginDestroy()
{
	PCGEX_DELETE(CheckSuccessInfos)
	PCGEX_DELETE(CheckFailInfos)

	Super::BeginDestroy();
}

TArray<FPCGPinProperties> UPCGExConditionalActionProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExConditionalActions::SourceConditionsFilterLabel, "Filters used to define if there's a match or not.", Required, {})
	return PinProperties;
}

UPCGExParamFactoryBase* UPCGExConditionalActionProviderSettings::CreateFactory(FPCGExContext* InContext, UPCGExParamFactoryBase* InFactory) const
{
	UPCGExConditionalActionFactoryBase* TypedFactory = Cast<UPCGExConditionalActionFactoryBase>(InFactory);

	if (TypedFactory)
	{
		if (!GetInputFactories(
			InContext, PCGExConditionalActions::SourceConditionsFilterLabel, TypedFactory->FilterFactories,
			PCGExFactories::PointFilters, true))
		{
			return nullptr;
		}

		TypedFactory->Priority = Priority;

		if (!TypedFactory->Boot(InContext)) { return nullptr; }
	}
	else
	{
		return nullptr;
	}

	return InFactory;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

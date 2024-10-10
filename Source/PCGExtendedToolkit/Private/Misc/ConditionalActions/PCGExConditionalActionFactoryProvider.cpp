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

bool UPCGExConditionalActionOperation::PrepareForData(const FPCGContext* InContext, const TSharedPtr<PCGExData::FFacade>& InPointDataFacade)
{
	PrimaryDataFacade = InPointDataFacade;

	FilterManager = MakeShared<PCGExPointFilter::FManager>(PrimaryDataFacade.ToSharedRef());

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
	Super::Cleanup();
}

#if WITH_EDITOR
FString UPCGExConditionalActionProviderSettings::GetDisplayName() const { return TEXT(""); }
#endif

UPCGExConditionalActionOperation* UPCGExConditionalActionFactoryBase::CreateOperation(FPCGExContext* InContext) const
{
	UPCGExConditionalActionOperation* NewOperation = InContext->ManagedObjects->New<UPCGExConditionalActionOperation>();
	NewOperation->Factory = const_cast<UPCGExConditionalActionFactoryBase*>(this);
	return NewOperation;
}

bool UPCGExConditionalActionFactoryBase::Boot(FPCGContext* InContext)
{
	return true;
}

bool UPCGExConditionalActionFactoryBase::AppendAndValidate(PCGEx::FAttributesInfos* InInfos, FString& OutMessage) const
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
	if (UPCGExConditionalActionFactoryBase* TypedFactory = Cast<UPCGExConditionalActionFactoryBase>(InFactory))
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

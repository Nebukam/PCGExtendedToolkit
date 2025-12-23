// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExElement.h"

#include "Core/PCGExContext.h"
#include "Factories/PCGExInstancedFactory.h"
#include "Core/PCGExSettings.h"
#include "Helpers/PCGExArrayHelpers.h"
#include "Helpers/PCGSettingsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"

bool IPCGExElement::PrepareDataInternal(FPCGContext* Context) const
{
	check(Context);

	FPCGExContext* InContext = static_cast<FPCGExContext*>(Context);

	const UPCGExSettings* InSettings = Context->GetInputSettings<UPCGExSettings>();
	check(InSettings);

	return AdvancePreparation(InContext, InSettings);
}

bool IPCGExElement::AdvancePreparation(FPCGExContext* Context, const UPCGExSettings* InSettings) const
{
	if (!Context->GetInputSettings<UPCGSettings>()->bEnabled) { return Context->CancelExecution(FString()); }

	PCGEX_EXECUTION_CHECK_C(Context)

	if (Context->IsState(PCGExCommon::States::State_Preparation))
	{
		if (!Boot(Context)) { return Context->CancelExecution(FString()); }

		// Have operations register their dependencies
		for (UPCGExInstancedFactory* Op : Context->InternalOperations) { Op->RegisterAssetDependencies(Context); }

		Context->RegisterAssetDependencies();
		if (Context->HasAssetRequirements() && Context->LoadAssets()) { return false; }

		// Call it so if there's initialization in there it'll run as a mandatory step
		PostLoadAssetsDependencies(Context);
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExCommon::States::State_LoadingAssetDependencies)
	{
		PostLoadAssetsDependencies(Context);
		PCGEX_EXECUTION_CHECK_C(Context)
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExCommon::States::State_AsyncPreparation)
	{
		PCGEX_EXECUTION_CHECK_C(Context)
	}

	if (!PostBoot(Context))
	{
		return Context->CancelExecution(TEXT("There was a problem during post-data preparation."));
	}

	Context->ReadyForExecution();
	return true;
}

FPCGContext* IPCGExElement::Initialize(const FPCGInitializeElementParams& InParams)
{
	FPCGExContext* Context = static_cast<FPCGExContext*>(IPCGElement::Initialize(InParams));

	const UPCGExSettings* Settings = Context->GetInputSettings<UPCGExSettings>();
	check(Settings);

	Context->bFlattenOutput = Settings->bFlattenOutput;
	Context->bScopedAttributeGet = Settings->WantsScopedAttributeGet();
	Context->bPropagateAbortedExecution = Settings->bPropagateAbortedExecution;

	Context->bQuietInvalidInputWarning = Settings->bQuietInvalidInputWarning;
	Context->bQuietMissingInputError = Settings->bQuietMissingInputError;
	Context->bQuietCancellationError = Settings->bQuietCancellationError;
	Context->bCleanupConsumableAttributes = Settings->bCleanupConsumableAttributes;

	Context->ElementHandle = this;

	if (Context->bCleanupConsumableAttributes)
	{
		for (const TArray<FString> Names = PCGExArrayHelpers::GetStringArrayFromCommaSeparatedList(Settings->CommaSeparatedProtectedAttributesName); const FString& Name : Names)
		{
			Context->AddProtectedAttributeName(FName(Name));
		}

		for (const FName& Name : Settings->ProtectedAttributes)
		{
			Context->AddProtectedAttributeName(FName(Name));
		}
	}

	OnContextInitialized(Context);

	return Context;
}

bool IPCGExElement::IsCacheable(const UPCGSettings* InSettings) const
{
	const UPCGExSettings* Settings = static_cast<const UPCGExSettings*>(InSettings);
	return Settings->ShouldCache();
}

FPCGContext* IPCGExElement::CreateContext() { return new FPCGExContext(); }

void IPCGExElement::OnContextInitialized(FPCGExContext* InContext) const
{
	InContext->SetState(PCGExCommon::States::State_Preparation);
}

bool IPCGExElement::Boot(FPCGExContext* InContext) const
{
	if (InContext->InputData.bCancelExecution) { return false; }
	return true;
}

void IPCGExElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
}

bool IPCGExElement::PostBoot(FPCGExContext* InContext) const
{
	return true;
}

void IPCGExElement::AbortInternal(FPCGContext* Context) const
{
	IPCGElement::AbortInternal(Context);

	if (!Context) { return; }

	//UE_LOG(LogTemp, Warning, TEXT(">> ABORTING @%s"), *Context->GetInputSettings<UPCGExSettings>()->GetName());

	FPCGExContext* PCGExContext = static_cast<FPCGExContext*>(Context);
	PCGExContext->CancelExecution();
}

bool IPCGExElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
	return false;
}

bool IPCGExElement::SupportsBasePointDataInputs(FPCGContext* InContext) const
{
	return true;
}

bool IPCGExElement::ExecuteInternal(FPCGContext* Context) const
{
	check(Context);

	FPCGExContext* InContext = static_cast<FPCGExContext*>(Context);

	PCGEX_EXECUTION_CHECK_C(InContext)

	const UPCGExSettings* InSettings = Context->GetInputSettings<UPCGExSettings>();
	check(InSettings);

	if (InContext->IsInitialExecution()) { InitializeData(InContext, InSettings); }
	return AdvanceWork(InContext, InSettings);
}

void IPCGExElement::InitializeData(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
}

bool IPCGExElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	return true;
}

void IPCGExElement::CompleteWork(FPCGExContext* InContext) const
{
	const UPCGExSettings* InSettings = InContext->GetInputSettings<UPCGExSettings>();
	check(InSettings);
}

#undef LOCTEXT_NAMESPACE

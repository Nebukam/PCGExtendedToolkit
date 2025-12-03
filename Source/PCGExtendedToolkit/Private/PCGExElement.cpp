// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExElement.h"

#include "PCGExInstancedFactory.h"
#include "PCGExSettings.h"
#include "Details/PCGExWaitMacros.h"
#include "Helpers/PCGAsync.h"
#include "Helpers/PCGSettingsHelpers.h"
#include "Transform/PCGExNormalize.h"

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
	if (!Context->GetInputSettings<UPCGSettings>()->bEnabled)
	{
		Context->bWorkCancelled = true;
		return true;
	}

	PCGEX_EXECUTION_CHECK_C(Context)

	if (Context->IsState(PCGExCommon::State_Preparation))
	{
		if (!Boot(Context))
		{
			return Context->CancelExecution(TEXT(""));
		}

		// Have operations register their dependencies
		for (UPCGExInstancedFactory* Op : Context->InternalOperations) { Op->RegisterAssetDependencies(Context); }

		Context->RegisterAssetDependencies();
		if (Context->HasAssetRequirements())
		{
			Context->LoadAssets();
			return false;
		}
		// Call it so if there's initialization in there it'll run as a mandatory step
		PostLoadAssetsDependencies(Context);
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExCommon::State_LoadingAssetDependencies)
	{
		PostLoadAssetsDependencies(Context);
		PCGEX_EXECUTION_CHECK_C(Context)
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExCommon::State_AsyncPreparation)
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
		for (const TArray<FString> Names = PCGExHelpers::GetStringArrayFromCommaSeparatedList(Settings->CommaSeparatedProtectedAttributesName);
		     const FString& Name : Names)
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
	InContext->SetState(PCGExCommon::State_Preparation);
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

	FPCGExContext* PCGExContext = static_cast<FPCGExContext*>(Context);
	PCGExContext->CancelExecution(TEXT(""));
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

	const UPCGExSettings* InSettings = Context->GetInputSettings<UPCGExSettings>();
	check(InSettings);

	if (InContext->CanExecute()){ InContext->bIsPaused = true; }	
	return AdvanceWork(InContext, InSettings);
}

bool IPCGExElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	return true;
}

void IPCGExElement::CompleteWork(FPCGExContext* InContext) const
{
	
}

#undef LOCTEXT_NAMESPACE

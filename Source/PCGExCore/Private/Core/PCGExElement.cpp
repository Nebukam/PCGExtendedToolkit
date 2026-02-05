// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExElement.h"

#include "PCGExCoreSettingsCache.h"
#include "Core/PCGExContext.h"
#include "Factories/PCGExInstancedFactory.h"
#include "Core/PCGExSettings.h"
#include "Details/PCGExWaitMacros.h"
#include "Helpers/PCGAsync.h"
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

	// Preparation is a multi-phase state machine:
	// 1. Boot: validate inputs, configure context
	// 2. Register & load asset dependencies (may pause for async loading)
	// 3. PostLoadAssetsDependencies: finalize setup after assets are available
	// 4. PostBoot: last chance setup before execution begins
	// Each PCGEX_ON_ASYNC_STATE_READY gate re-enters when the async state completes,
	// returning false to yield to the scheduler in the meantime.
	if (Context->IsState(PCGExCommon::States::State_Preparation))
	{
		if (!Boot(Context)) { return Context->CancelExecution(FString()); }

		for (UPCGExInstancedFactory* Op : Context->InternalOperations) { Op->RegisterAssetDependencies(Context); }

		Context->RegisterAssetDependencies();
		if (Context->HasAssetRequirements() && Context->LoadAssets()) { return false; }

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

	if (Settings->SupportsDataStealing()
		&& Settings->StealData == EPCGExOptionState::Enabled)
	{
		Context->bWantsDataStealing = true;
	}

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

	// Execution policy controls whether we block the calling thread or return to the scheduler.
	// On the game thread, or when the policy says don't block, we just drive one step and return.
	// "NoPauseButLoop" only blocks when inside a PCG loop (avoids frame-delay per loop iteration).
	const EPCGExExecutionPolicy DesiredPolicy = InSettings->GetExecutionPolicy();
	const EPCGExExecutionPolicy LocalPolicy = DesiredPolicy == EPCGExExecutionPolicy::Default ? PCGEX_CORE_SETTINGS.ExecutionPolicy : DesiredPolicy;

	if (IsInGameThread()
		|| LocalPolicy == EPCGExExecutionPolicy::Ignored
		|| LocalPolicy == EPCGExExecutionPolicy::Default
		|| (LocalPolicy == EPCGExExecutionPolicy::NoPauseButLoop && InContext->LoopIndex != INDEX_NONE)
		|| (LocalPolicy == EPCGExExecutionPolicy::NoPauseButTopLoop && InContext->IsExecutingInsideLoop()))
	{
		return InContext->DriveAdvanceWork(InSettings);
	}

	// Adaptive spin-wait: blocks the scheduler thread until all async work completes.
	// Phases escalate from hot spinning (low latency) to sleeping (low CPU usage):
	//   0-50:    Yield only (sub-microsecond wake, max throughput)
	//   50-200:  Mostly yield, occasional 1us sleep (reduce power draw)
	//   200-1k:  1us sleeps (work is taking a while)
	//   1k+:     5us sleeps (long-running work, minimize CPU waste)
	constexpr int SPIN_PHASE_ITERATIONS = 50;
	constexpr int YIELD_PHASE_ITERATIONS = 200;
	constexpr float SHORT_SLEEP_MS = 0.001f;
	constexpr float LONG_SLEEP_MS = 0.005f;
	constexpr int LONG_SLEEP_THRESHOLD = 1000;
	int WaitCounter = 0;

	while (!InContext->DriveAdvanceWork(InSettings))
	{
		if (WaitCounter < SPIN_PHASE_ITERATIONS)
		{
			FPlatformProcess::YieldThread();
		}
		else if (WaitCounter < YIELD_PHASE_ITERATIONS)
		{
			if ((WaitCounter & 0x7) == 0) { FPlatformProcess::SleepNoStats(SHORT_SLEEP_MS); }
			else { FPlatformProcess::YieldThread(); }
		}
		else if (WaitCounter < LONG_SLEEP_THRESHOLD)
		{
			FPlatformProcess::SleepNoStats(SHORT_SLEEP_MS);
		}
		else
		{
			FPlatformProcess::SleepNoStats(LONG_SLEEP_MS);
		}
		++WaitCounter;
	}

	return true;
}

void IPCGExElement::InitializeData(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	const FPCGStack* Stack = InContext->GetStack();
	if (!ensure(Stack))
	{
		PCGE_LOG_C(Error, LogOnly, InContext, LOCTEXT("ContextHasNoExecutionStack", "The execution context is malformed and has no call stack."));
		return;
	}

	// Extract loop indices from the PCG execution stack to determine if this node
	// is running inside a loop. LoopIndex is the immediate parent loop (second-to-last frame),
	// TopLoopIndex is the outermost loop in the stack. These affect execution policy decisions
	// (e.g. NoPauseButLoop only spin-waits when inside a loop to avoid per-iteration frame delays).
	const TArray<FPCGStackFrame>& StackFrames = Stack->GetStackFrames();

	if (StackFrames.Num() >= 2)
	{
		InContext->LoopIndex = StackFrames.Last(1).LoopIndex;
	}

	for (const FPCGStackFrame& Frame : StackFrames)
	{
		if (Frame.LoopIndex != INDEX_NONE)
		{
			InContext->TopLoopIndex = Frame.LoopIndex;
			break;
		}
	}
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

// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

/**
 * PCGEx Element Lifecycle Integration Tests (BDD Style)
 *
 * Tests the element execution lifecycle patterns used in PCGEx:
 * - Boot phase (initialization, validation)
 * - PostBoot phase (asset loading complete)
 * - Execution phases (state machine)
 * - Context state management
 *
 * These tests verify the expected lifecycle patterns without requiring
 * full PCG graph execution, focusing on the state machine logic.
 *
 * Test naming: PCGEx.Integration.Elements.<Phase>
 */

#include "Misc/AutomationTest.h"

// =============================================================================
// Element State Machine Tests
// =============================================================================

/**
 * Simulates the PCGEx element state machine for testing
 * without requiring actual PCG infrastructure
 */
class FMockElementStateMachine
{
public:
	enum class EState : uint8
	{
		Initial,
		Booting,
		PostBoot,
		Processing,
		Completing,
		Done,
		Aborted
	};

	EState CurrentState = EState::Initial;
	bool bBootSucceeded = true;
	bool bPostBootSucceeded = true;
	bool bAsyncTasksPending = false;
	bool bCanExecute = true;
	int32 ProcessIterations = 0;
	int32 MaxProcessIterations = 3;

	bool IsState(EState State) const { return CurrentState == State; }
	bool CanExecute() const { return bCanExecute; }
	bool IsWaitingForTasks() const { return bAsyncTasksPending; }

	// Simulate one tick of execution
	bool Tick()
	{
		if (!CanExecute())
		{
			return true; // Early exit, done
		}

		switch (CurrentState)
		{
		case EState::Initial:
			CurrentState = EState::Booting;
			return false; // Continue

		case EState::Booting:
			if (!bBootSucceeded)
			{
				CurrentState = EState::Aborted;
				return true; // Done (failed)
			}
			CurrentState = EState::PostBoot;
			return false;

		case EState::PostBoot:
			if (!bPostBootSucceeded)
			{
				CurrentState = EState::Aborted;
				return true;
			}
			CurrentState = EState::Processing;
			return false;

		case EState::Processing:
			if (bAsyncTasksPending)
			{
				return false; // Wait for async
			}
			ProcessIterations++;
			if (ProcessIterations >= MaxProcessIterations)
			{
				CurrentState = EState::Completing;
			}
			return false;

		case EState::Completing:
			CurrentState = EState::Done;
			return true; // Finished

		case EState::Done:
		case EState::Aborted:
			return true;

		default:
			return true;
		}
	}

	// Run to completion
	int32 RunToCompletion(int32 MaxTicks = 100)
	{
		int32 TickCount = 0;
		while (TickCount < MaxTicks && !Tick())
		{
			TickCount++;
		}
		return TickCount;
	}
};

BEGIN_DEFINE_SPEC(
	FPCGExElementLifecycleSpec,
	"PCGEx.Integration.Elements.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	TUniquePtr<FMockElementStateMachine> StateMachine;

END_DEFINE_SPEC(FPCGExElementLifecycleSpec)

void FPCGExElementLifecycleSpec::Define()
{
	BeforeEach([this]()
	{
		StateMachine = MakeUnique<FMockElementStateMachine>();
	});

	AfterEach([this]()
	{
		StateMachine.Reset();
	});

	Describe("Boot Phase", [this]()
	{
		It("should transition from Initial to Booting on first tick", [this]()
		{
			TestTrue(TEXT("Starts in Initial state"),
			         StateMachine->IsState(FMockElementStateMachine::EState::Initial));

			StateMachine->Tick();

			TestTrue(TEXT("Transitions to Booting"),
			         StateMachine->IsState(FMockElementStateMachine::EState::Booting));
		});

		It("should abort if boot fails", [this]()
		{
			StateMachine->bBootSucceeded = false;
			StateMachine->RunToCompletion();

			TestTrue(TEXT("Aborted on boot failure"),
			         StateMachine->IsState(FMockElementStateMachine::EState::Aborted));
		});

		It("should continue to PostBoot on boot success", [this]()
		{
			StateMachine->bBootSucceeded = true;
			StateMachine->Tick(); // Initial -> Booting
			StateMachine->Tick(); // Booting -> PostBoot

			TestTrue(TEXT("Transitions to PostBoot"),
			         StateMachine->IsState(FMockElementStateMachine::EState::PostBoot));
		});
	});

	Describe("PostBoot Phase", [this]()
	{
		It("should abort if post-boot fails", [this]()
		{
			StateMachine->bBootSucceeded = true;
			StateMachine->bPostBootSucceeded = false;
			StateMachine->RunToCompletion();

			TestTrue(TEXT("Aborted on post-boot failure"),
			         StateMachine->IsState(FMockElementStateMachine::EState::Aborted));
		});

		It("should continue to Processing on post-boot success", [this]()
		{
			StateMachine->bBootSucceeded = true;
			StateMachine->bPostBootSucceeded = true;

			// Run through boot phases
			StateMachine->Tick(); // Initial -> Booting
			StateMachine->Tick(); // Booting -> PostBoot
			StateMachine->Tick(); // PostBoot -> Processing

			TestTrue(TEXT("Transitions to Processing"),
			         StateMachine->IsState(FMockElementStateMachine::EState::Processing));
		});
	});

	Describe("Processing Phase", [this]()
	{
		It("should iterate until completion", [this]()
		{
			StateMachine->MaxProcessIterations = 5;
			StateMachine->RunToCompletion();

			TestEqual(TEXT("Processed expected iterations"),
			          StateMachine->ProcessIterations, 5);
			TestTrue(TEXT("Completed successfully"),
			         StateMachine->IsState(FMockElementStateMachine::EState::Done));
		});

		It("should wait for async tasks", [this]()
		{
			StateMachine->bAsyncTasksPending = true;

			// Run through boot phases
			StateMachine->Tick(); // Initial -> Booting
			StateMachine->Tick(); // Booting -> PostBoot
			StateMachine->Tick(); // PostBoot -> Processing

			// Should stay in Processing while async pending
			StateMachine->Tick();
			TestTrue(TEXT("Stays in Processing while async"),
			         StateMachine->IsState(FMockElementStateMachine::EState::Processing));
			TestEqual(TEXT("No processing while waiting"),
			          StateMachine->ProcessIterations, 0);

			// Complete async
			StateMachine->bAsyncTasksPending = false;
			StateMachine->RunToCompletion();

			TestTrue(TEXT("Completes after async"),
			         StateMachine->IsState(FMockElementStateMachine::EState::Done));
		});
	});

	Describe("Completion Phase", [this]()
	{
		It("should transition to Done after completing", [this]()
		{
			StateMachine->RunToCompletion();

			TestTrue(TEXT("Final state is Done"),
			         StateMachine->IsState(FMockElementStateMachine::EState::Done));
		});

		It("should return true (finished) when Done", [this]()
		{
			StateMachine->RunToCompletion();

			// Additional tick should still return true
			bool IsFinished = StateMachine->Tick();
			TestTrue(TEXT("Returns true when Done"), IsFinished);
		});
	});

	Describe("Execution Control", [this]()
	{
		It("should early exit when cannot execute", [this]()
		{
			StateMachine->bCanExecute = false;

			bool Result = StateMachine->Tick();

			TestTrue(TEXT("Returns true immediately when cannot execute"), Result);
			TestTrue(TEXT("Stays in Initial state"),
			         StateMachine->IsState(FMockElementStateMachine::EState::Initial));
		});
	});
}

// =============================================================================
// Element State Tracking Tests
// =============================================================================

BEGIN_DEFINE_SPEC(
	FPCGExElementStateTrackingSpec,
	"PCGEx.Integration.Elements.StateTracking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	// Simulate state-based execution pattern used in PCGEX_ON_STATE macros
	struct FStateTracker
	{
		int32 CurrentState = 0;
		bool bWaitingForAsync = false;
		TArray<int32> StatesVisited;

		bool IsState(int32 State) const { return CurrentState == State; }
		bool IsWaitingForTasks() const { return bWaitingForAsync; }

		void SetState(int32 NewState)
		{
			CurrentState = NewState;
			StatesVisited.Add(NewState);
		}
	};

	TUniquePtr<FStateTracker> Tracker;

END_DEFINE_SPEC(FPCGExElementStateTrackingSpec)

void FPCGExElementStateTrackingSpec::Define()
{
	BeforeEach([this]()
	{
		Tracker = MakeUnique<FStateTracker>();
	});

	AfterEach([this]()
	{
		Tracker.Reset();
	});

	Describe("State Transitions", [this]()
	{
		It("should track state history", [this]()
		{
			Tracker->SetState(0);
			Tracker->SetState(1);
			Tracker->SetState(2);

			TestEqual(TEXT("Three states visited"), Tracker->StatesVisited.Num(), 3);
			TestEqual(TEXT("First state was 0"), Tracker->StatesVisited[0], 0);
			TestEqual(TEXT("Current state is 2"), Tracker->CurrentState, 2);
		});

		It("should support conditional state checks", [this]()
		{
			Tracker->SetState(5);

			// Simulate PCGEX_ON_STATE pattern
			bool StateHandled = false;
			if (Tracker->IsState(5))
			{
				StateHandled = true;
			}

			TestTrue(TEXT("State 5 was handled"), StateHandled);
		});
	});

	Describe("Async State Ready Pattern", [this]()
	{
		It("should wait when async pending", [this]()
		{
			Tracker->SetState(10);
			Tracker->bWaitingForAsync = true;

			// Simulate PCGEX_ON_ASYNC_STATE_READY pattern
			bool ShouldContinue = true;
			if (Tracker->IsState(10) && Tracker->IsWaitingForTasks())
			{
				ShouldContinue = false; // Wait
			}

			TestFalse(TEXT("Should wait when async pending"), ShouldContinue);
		});

		It("should proceed when async complete", [this]()
		{
			Tracker->SetState(10);
			Tracker->bWaitingForAsync = false;

			bool ShouldContinue = false;
			bool StateReady = Tracker->IsState(10) && !Tracker->IsWaitingForTasks();
			if (StateReady)
			{
				ShouldContinue = true;
			}

			TestTrue(TEXT("Should continue when async complete"), ShouldContinue);
		});
	});

	Describe("Common State Values", [this]()
	{
		It("should use sequential state values", [this]()
		{
			// PCGEx elements typically use 0, 100, 200, etc. for major states
			// and increments like 101, 102 for sub-states

			constexpr int32 STATE_BOOT = 0;
			constexpr int32 STATE_PROCESS_START = 100;
			constexpr int32 STATE_PROCESS_STEP1 = 101;
			constexpr int32 STATE_PROCESS_STEP2 = 102;
			constexpr int32 STATE_COMPLETE = 200;

			Tracker->SetState(STATE_BOOT);
			TestTrue(TEXT("Boot state check"), Tracker->IsState(STATE_BOOT));

			Tracker->SetState(STATE_PROCESS_START);
			TestTrue(TEXT("Process start check"), Tracker->IsState(STATE_PROCESS_START));

			Tracker->SetState(STATE_PROCESS_STEP1);
			TestFalse(TEXT("Not at process start"), Tracker->IsState(STATE_PROCESS_START));
			TestTrue(TEXT("At step 1"), Tracker->IsState(STATE_PROCESS_STEP1));
		});
	});
}

// =============================================================================
// Element Output Management Tests
// =============================================================================

BEGIN_DEFINE_SPEC(
	FPCGExElementOutputSpec,
	"PCGEx.Integration.Elements.Output",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

	struct FMockOutput
	{
		FName Label;
		bool bEnabled = true;
		int32 DataCount = 0;
	};

	TArray<FMockOutput> Outputs;

END_DEFINE_SPEC(FPCGExElementOutputSpec)

void FPCGExElementOutputSpec::Define()
{
	BeforeEach([this]()
	{
		Outputs.Reset();
	});

	Describe("Output Pin Management", [this]()
	{
		It("should track multiple output pins", [this]()
		{
			Outputs.Add({FName(TEXT("Out")), true, 100});
			Outputs.Add({FName(TEXT("Inside")), true, 50});
			Outputs.Add({FName(TEXT("Outside")), true, 50});

			TestEqual(TEXT("Three outputs registered"), Outputs.Num(), 3);
		});

		It("should support disabled outputs", [this]()
		{
			Outputs.Add({FName(TEXT("Out")), true, 100});
			Outputs.Add({FName(TEXT("Optional")), false, 0});

			int32 EnabledCount = 0;
			for (const FMockOutput& Output : Outputs)
			{
				if (Output.bEnabled) EnabledCount++;
			}

			TestEqual(TEXT("One enabled output"), EnabledCount, 1);
		});

		It("should find output by label", [this]()
		{
			Outputs.Add({FName(TEXT("Primary")), true, 100});
			Outputs.Add({FName(TEXT("Secondary")), true, 50});

			FMockOutput* Found = nullptr;
			for (FMockOutput& Output : Outputs)
			{
				if (Output.Label == FName(TEXT("Secondary")))
				{
					Found = &Output;
					break;
				}
			}

			TestNotNull(TEXT("Found secondary output"), Found);
			if (Found)
			{
				TestEqual(TEXT("Correct data count"), Found->DataCount, 50);
			}
		});
	});

	Describe("Output Validation", [this]()
	{
		It("should validate non-empty output", [this]()
		{
			Outputs.Add({FName(TEXT("Out")), true, 100});

			bool HasData = Outputs[0].bEnabled && Outputs[0].DataCount > 0;
			TestTrue(TEXT("Output has data"), HasData);
		});

		It("should handle empty but valid output", [this]()
		{
			Outputs.Add({FName(TEXT("Out")), true, 0});

			bool IsValid = Outputs[0].bEnabled;
			bool HasData = Outputs[0].DataCount > 0;

			TestTrue(TEXT("Output is valid"), IsValid);
			TestFalse(TEXT("Output is empty"), HasData);
		});
	});
}

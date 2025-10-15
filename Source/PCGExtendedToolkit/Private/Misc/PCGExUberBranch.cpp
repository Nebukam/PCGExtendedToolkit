// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExUberBranch.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointFilter.h"
#include "Data/PCGExPointIO.h"


#define LOCTEXT_NAMESPACE "PCGExUberBranch"
#define PCGEX_NAMESPACE UberBranch

#if WITH_EDITOR
void UPCGExUberBranchSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	InputLabels.Reset(NumBranches);
	OutputLabels.Reset(NumBranches);

	for (int i = 0; i < NumBranches; i++)
	{
		FString SI = FString::Printf(TEXT("%d"), i + 0);
		InputLabels.Emplace(FName(TEXT("→ ") + SI));
		OutputLabels.Emplace(FName(SI + TEXT(" →")));
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

TArray<FPCGPinProperties> UPCGExUberBranchSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	for (int i = 0; i < NumBranches; i++)
	{
		PCGEX_PIN_FILTERS(InputLabels[i], "Collection filters. Only support C-Filter or regular filters that are set-up to work with data bounds or @Data attributes.", Normal)
	}
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExUberBranchSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PCGEX_PIN_POINTS(GetMainOutputPin(), "Collections that didn't branch in any specific pin", Normal)

	for (int i = 0; i < NumBranches; i++)
	{
		PCGEX_PIN_POINTS(OutputLabels[i], "Collections that passed the matching input filters, if they weren't output to any previous pin.", Normal)
	}

	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(UberBranch)

FName UPCGExUberBranchSettings::GetMainOutputPin() const
{
	// Ensure proper forward when node is disabled
	return FName("Default");
}

bool FPCGExUberBranchElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(UberBranch)

	for (const TSharedPtr<PCGExData::FPointIO>& IO : Context->MainPoints->Pairs)
	{
		IO->InitializeOutput(PCGExData::EIOInit::Forward);
		PCGEX_MAKE_SHARED(Facade, PCGExData::FFacade, IO.ToSharedRef())
		Context->Facades.Add(Facade);
	}

	for (int i = 0; i < Settings->NumBranches; i++)
	{
		TArray<TObjectPtr<const UPCGExPointFilterFactoryData>> Factories;
		if (GetInputFactories(
			Context, Settings->InputLabels[i], Factories,
			PCGExFactories::PointFilters))
		{
			for (int f = 0; f < Factories.Num(); f++)
			{
				TObjectPtr<const UPCGExPointFilterFactoryData> Factory = Factories[f];
				if (!Factory->SupportsCollectionEvaluation())
				{
					PCGEX_LOG_INVALID_INPUT(InContext, FText::Format(FTEXT("Unsupported filter : {0} (Requires per-point evaluation)"), FText::FromString(Factory->GetName())))
					Factories.RemoveAt(f);
					f--;
				}
			}
		}

		if (Factories.IsEmpty())
		{
			Context->Managers.Add(nullptr);
		}
		else
		{
			bool bInitialized = false;
			for (const TSharedPtr<PCGExData::FFacade>& Facade : Context->Facades)
			{
				// Attempt to initialize with all data until hopefully one works
				PCGEX_MAKE_SHARED(Manager, PCGExPointFilter::FManager, Facade.ToSharedRef())
				bInitialized = Manager->Init(Context, Factories);
				if (bInitialized)
				{
					Context->Managers.Add(Manager);
					break;
				}
			}

			if (!bInitialized) { Context->Managers.Add(nullptr); }
		}
	}

	return true;
}

bool FPCGExUberBranchElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExUberBranchElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(UberBranch)
	PCGEX_EXECUTION_CHECK
	if (Settings->AsyncChunkSize > 0)
	{
		PCGEX_ON_INITIAL_EXECUTION
		{
			TWeakPtr<FPCGContextHandle> Handle = Context->GetOrCreateHandle();

			Context->SetAsyncState(PCGExCommon::State_WaitingOnAsyncWork);
			PCGEX_ASYNC_GROUP_CHKD_CUSTOM(Context->GetAsyncManager(), BranchTask, true)

			BranchTask->OnSubLoopStartCallback =
				[Handle, Settings](const PCGExMT::FScope& Scope)
				{
					PCGEX_SHARED_TCONTEXT_VOID(UberBranch, Handle)
					PCGEX_SCOPE_LOOP(Index)
					{
						const TSharedPtr<PCGExData::FFacade>& Facade = SharedContext.Get()->Facades[Index];

						bool bDistributed = false;
						for (int i = 0; i < Settings->NumBranches; i++)
						{
							const TSharedPtr<PCGExPointFilter::FManager> Manager = SharedContext.Get()->Managers[i];
							if (!Manager) { continue; }
							if (Manager->Test(Facade->Source, SharedContext.Get()->MainPoints))
							{
								Facade->Source->OutputPin = Settings->OutputLabels[i];
								bDistributed = true;
								break;
							}
						}

						if (!bDistributed) { Facade->Source->OutputPin = Settings->GetMainOutputPin(); }
					}
				};

			BranchTask->StartSubLoops(Context->Facades.Num(), Settings->AsyncChunkSize);
			return false;
		}

		PCGEX_ON_ASYNC_STATE_READY(PCGExCommon::State_WaitingOnAsyncWork)
		{
			Context->MainPoints->StageOutputs();
			Context->Done();
		}
	}
	else
	{
		for (const TSharedPtr<PCGExData::FFacade>& Facade : Context->Facades)
		{
			bool bDistributed = false;
			for (int i = 0; i < Settings->NumBranches; i++)
			{
				const TSharedPtr<PCGExPointFilter::FManager> Manager = Context->Managers[i];
				if (!Manager) { continue; }
				if (Manager->Test(Facade->Source, Context->MainPoints))
				{
					Facade->Source->OutputPin = Settings->OutputLabels[i];
					bDistributed = true;
					break;
				}
			}

			if (!bDistributed) { Facade->Source->OutputPin = Settings->GetMainOutputPin(); }
		}

		Context->MainPoints->StageOutputs();
		Context->Done();
	}

	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

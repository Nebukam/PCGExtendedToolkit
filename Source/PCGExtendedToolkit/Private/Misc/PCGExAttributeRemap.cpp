// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Misc/PCGExAttributeRemap.h"
#include "PCGExHelpers.h"
#include "PCGExScopedContainers.h"
#include "AssetStaging/PCGExAssetStaging.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExProxyData.h"
#include "Data/PCGExProxyDataHelpers.h"
#include "Details/PCGExVersion.h"


#define LOCTEXT_NAMESPACE "PCGExAttributeRemap"
#define PCGEX_NAMESPACE AttributeRemap

#if WITH_EDITOR
FString UPCGExAttributeRemapSettings::GetDisplayName() const
{
	if (Attributes.WantsRemappedOutput())
	{
		return TEXT("Remap : ") + Attributes.Source.ToString() + TEXT(" → ") + Attributes.Target.ToString();
	}

	return TEXT("Remap : ") + Attributes.Source.ToString();
}

void UPCGExAttributeRemapSettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	PCGEX_IF_DATA_VERSION(1, 70, 11)
	{
		if (SourceAttributeName_DEPRECATED != NAME_None) { Attributes.Source = SourceAttributeName_DEPRECATED; }
		if (TargetAttributeName_DEPRECATED != NAME_None)
		{
			Attributes.Target = TargetAttributeName_DEPRECATED;
			Attributes.bOutputToDifferentName = (SourceAttributeName_DEPRECATED != TargetAttributeName_DEPRECATED);
		}
	}

	PCGEX_UPDATE_DATA_VERSION
	Super::ApplyDeprecation(InOutNode);
}
#endif

void FPCGExAttributeRemapContext::RegisterAssetDependencies()
{
	FPCGExPointsProcessorContext::RegisterAssetDependencies();
	for (const FPCGExComponentRemapRule& Rule : RemapSettings) { AddAssetDependency(Rule.RemapDetails.RemapCurve.ToSoftObjectPath()); }
}

PCGExData::EIOInit UPCGExAttributeRemapSettings::GetMainDataInitializationPolicy() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_INITIALIZE_ELEMENT(AttributeRemap)
PCGEX_ELEMENT_BATCH_POINT_IMPL(AttributeRemap)

bool FPCGExAttributeRemapElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(AttributeRemap)

	if (!Settings->Attributes.ValidateNamesOrProperties(Context)) { return false; }

	Context->RemapSettings[0] = Settings->BaseRemap;
	Context->RemapSettings[1] = Settings->Component2RemapOverride;
	Context->RemapSettings[2] = Settings->Component3RemapOverride;
	Context->RemapSettings[3] = Settings->Component4RemapOverride;

	return true;
}

void FPCGExAttributeRemapElement::PostLoadAssetsDependencies(FPCGExContext* InContext) const
{
	FPCGExPointsProcessorElement::PostLoadAssetsDependencies(InContext);

	PCGEX_CONTEXT_AND_SETTINGS(AttributeRemap)

	for (int i = 0; i < 4; i++) { Context->RemapSettings[i].RemapDetails.Init(); }

	Context->RemapIndices[0] = 0;
	Context->RemapIndices[1] = Settings->bOverrideComponent2 ? 1 : 0;
	Context->RemapIndices[2] = Settings->bOverrideComponent3 ? 2 : 0;
	Context->RemapIndices[3] = Settings->bOverrideComponent4 ? 3 : 0;
}

bool FPCGExAttributeRemapElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributeRemapElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(AttributeRemap)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to remap."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

bool FPCGExAttributeRemapElement::CanExecuteOnlyOnMainThread(FPCGContext* Context) const
{
	return Context ? Context->CurrentPhase == EPCGExecutionPhase::PrepareData : false;
}

namespace PCGExAttributeRemap
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		TArray<TSharedPtr<PCGExData::IBufferProxy>> UntypedInputProxies;
		TArray<TSharedPtr<PCGExData::IBufferProxy>> UntypedOutputProxies;

		InputDescriptor.DataFacade = PointDataFacade;
		OutputDescriptor.DataFacade = PointDataFacade;
		OutputDescriptor.Role = PCGExData::EProxyRole::Write;

		if (!InputDescriptor.Capture(Context, Settings->Attributes.GetSourceSelector(), PCGExData::EIOSide::In)) { return false; }

		// Number of dimensions to be remapped
		UnderlyingType = InputDescriptor.WorkingType;
		Dimensions = PCGEx::GetMetadataSize(UnderlyingType);

		// Get per-field proxies for input
		if (!GetPerFieldProxyBuffers(Context, InputDescriptor, Dimensions, UntypedInputProxies)) { return false; }

		if (!OutputDescriptor.CaptureStrict(Context, Settings->Attributes.GetTargetSelector(), PCGExData::EIOSide::Out, false))
		{
			// This might be expected if the destination does not exist

			if (Dimensions == 1 &&
				Settings->Attributes.WantsRemappedOutput() &&
				!OutputDescriptor.SubSelection.bIsValid)
			{
				// We're remapping a component to a single value with no subselection
				OutputDescriptor.RealType = InputDescriptor.WorkingType;
			}
			else
			{
				// We're remapping to a component within the same larger type
				OutputDescriptor.RealType = InputDescriptor.RealType;
			}

			if (Settings->bAutoCastIntegerToDouble &&
				(OutputDescriptor.RealType == EPCGMetadataTypes::Integer32 || OutputDescriptor.RealType == EPCGMetadataTypes::Integer64))
			{
				OutputDescriptor.RealType = EPCGMetadataTypes::Double;
			}

			OutputDescriptor.WorkingType = InputDescriptor.WorkingType;
		}
		else
		{
			// TODO : Grab default type for attribute if it cannot be inferred
			// GetPerFieldProxyBuffers expect a valid RealType to work from
		}

		// Get per-field proxies for output
		if (!GetPerFieldProxyBuffers(Context, OutputDescriptor, Dimensions, UntypedOutputProxies)) { return false; }

		for (int i = 0; i < Dimensions; i++)
		{
			TSharedPtr<PCGExData::IBufferProxy> InProxy = UntypedInputProxies[i];
			TSharedPtr<PCGExData::IBufferProxy> OutProxy = UntypedOutputProxies[i];

			if (InProxy->WorkingType != EPCGMetadataTypes::Double)
			{
				// TODO : Some additional validation, just making sure we can safely cast those
			}

			if (OutProxy->WorkingType != EPCGMetadataTypes::Double)
			{
				// TODO : Some additional validation, just making sure we can safely cast those
			}

			InputProxies.Add(StaticCastSharedPtr<PCGExData::TBufferProxy<double>>(InProxy));
			OutputProxies.Add(StaticCastSharedPtr<PCGExData::TBufferProxy<double>>(OutProxy));
		}

		Rules.Reserve(Dimensions);
		for (int i = 0; i < Dimensions; i++)
		{
			FPCGExComponentRemapRule& Rule = Rules.Add_GetRef(FPCGExComponentRemapRule(Context->RemapSettings[Context->RemapIndices[i]]));
			if (!Rule.RemapDetails.bUseInMin) { Rule.RemapDetails.InMin = MAX_dbl; }
			if (!Rule.RemapDetails.bUseInMax) { Rule.RemapDetails.InMax = MIN_dbl_neg; }
		}

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, FetchTask)

		FetchTask->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS

				// Fix min/max range
				for (FPCGExComponentRemapRule& Rule : This->Rules)
				{
					if (!Rule.RemapDetails.bUseInMin)
					{
						Rule.RemapDetails.InMin = Rule.MinCache->Min();
					}

					if (!Rule.RemapDetails.bUseInMax)
					{
						Rule.RemapDetails.InMax = Rule.MaxCache->Max();
					}

					if (Rule.RemapDetails.RangeMethod == EPCGExRangeType::FullRange && Rule.RemapDetails.InMin > 0) { Rule.RemapDetails.InMin = 0; }
				}

				This->OnPreparationComplete();
			};

		FetchTask->OnPrepareSubLoopsCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const TArray<PCGExMT::FScope>& Loops)
			{
				PCGEX_ASYNC_THIS
				for (FPCGExComponentRemapRule& Rule : This->Rules)
				{
					Rule.MinCache = MakeShared<PCGExMT::TScopedNumericValue<double>>(Loops, MAX_dbl);
					Rule.MaxCache = MakeShared<PCGExMT::TScopedNumericValue<double>>(Loops, MIN_dbl_neg);
				}
			};

		FetchTask->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributeRemap::Fetch);
				PCGEX_ASYNC_THIS

				This->PointDataFacade->Fetch(Scope);

				// Find min/max & clamp values

				for (int d = 0; d < This->Dimensions; d++)
				{
					FPCGExComponentRemapRule& Rule = This->Rules[d];

					TSharedPtr<PCGExData::TBufferProxy<double>> InProxy = This->InputProxies[d];
					TSharedPtr<PCGExData::TBufferProxy<double>> OutProxy = This->OutputProxies[d];

					double Min = MAX_dbl;
					double Max = MIN_dbl_neg;

					if (Rule.RemapDetails.bUseAbsoluteRange)
					{
						PCGEX_SCOPE_LOOP(i)
						{
							double V = Rule.InputClampDetails.GetClampedValue(InProxy->Get(i));
							Min = FMath::Min(Min, FMath::Abs(V));
							Max = FMath::Max(Max, FMath::Abs(V));
							OutProxy->Set(i, V);
						}
					}
					else
					{
						PCGEX_SCOPE_LOOP(i)
						{
							double V = Rule.InputClampDetails.GetClampedValue(InProxy->Get(i));
							Min = FMath::Min(Min, V);
							Max = FMath::Max(Max, V);
							OutProxy->Set(i, V);
						}
					}

					Rule.MinCache->Set(Scope, Min);
					Rule.MaxCache->Set(Scope, Max);
				}
			};

		FetchTask->StartSubLoops(PointDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

		return true;
	}

	void FProcessor::RemapRange(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExAttributeRemap::RemapRange);

		for (int d = 0; d < Dimensions; d++)
		{
			FPCGExComponentRemapRule& Rule = Rules[d];
			TSharedPtr<PCGExData::TBufferProxy<double>> InProxy = InputProxies[d];
			TSharedPtr<PCGExData::TBufferProxy<double>> OutProxy = OutputProxies[d];

			if (Rule.RemapDetails.bUseAbsoluteRange)
			{
				if (Rule.RemapDetails.bPreserveSign)
				{
					PCGEX_SCOPE_LOOP(i)
					{
						double V = InProxy->Get(i);
						OutProxy->Set(
							i, Rule.OutputClampDetails.GetClampedValue(
								Rule.RemapDetails.GetRemappedValue(FMath::Abs(V)) * PCGExMath::SignPlus(V)));
					}
				}
				else
				{
					PCGEX_SCOPE_LOOP(i)
					{
						OutProxy->Set(
							i, Rule.OutputClampDetails.GetClampedValue(
								Rule.RemapDetails.GetRemappedValue(
									FMath::Abs(InProxy->Get(i)))));
					}
				}
			}
			else
			{
				if (Rule.RemapDetails.bPreserveSign)
				{
					PCGEX_SCOPE_LOOP(i)
					{
						OutProxy->Set(
							i, Rule.OutputClampDetails.GetClampedValue(
								Rule.RemapDetails.GetRemappedValue(
									InProxy->Get(i))));
					}
				}
				else
				{
					PCGEX_SCOPE_LOOP(i)
					{
						OutProxy->Set(
							i, Rule.OutputClampDetails.GetClampedValue(
								Rule.RemapDetails.GetRemappedValue(
									FMath::Abs(InProxy->Get(i)))));
					}
				}
			}
		}
	}

	void FProcessor::OnPreparationComplete()
	{
		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, RemapTask)
		RemapTask->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				This->RemapRange(Scope);
			};

		RemapTask->StartSubLoops(PointDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->WriteFastest(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

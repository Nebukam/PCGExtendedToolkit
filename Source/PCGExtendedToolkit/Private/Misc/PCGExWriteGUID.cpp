// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExWriteGUID.h"
#include "Misc/Guid.h"

#define LOCTEXT_NAMESPACE "PCGExWriteGUIDElement"
#define PCGEX_NAMESPACE WriteGUID

PCGExData::EIOInit UPCGExWriteGUIDSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_INITIALIZE_ELEMENT(WriteGUID)

bool FPCGExWriteGUIDElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteGUID)

	PCGEX_VALIDATE_NAME(Settings->OutputAttributeName)

	switch (Settings->Format)
	{
	default:
	case EPCGExGUIDFormat::Digits:
		Context->Format = EGuidFormats::Digits;
		break;
	case EPCGExGUIDFormat::DigitsLower:
		Context->Format = EGuidFormats::DigitsLower;
		break;
	case EPCGExGUIDFormat::DigitsWithHyphens:
		Context->Format = EGuidFormats::DigitsWithHyphens;
		break;
	case EPCGExGUIDFormat::DigitsWithHyphensLower:
		Context->Format = EGuidFormats::DigitsWithHyphensLower;
		break;
	case EPCGExGUIDFormat::DigitsWithHyphensInBraces:
		Context->Format = EGuidFormats::DigitsWithHyphensInBraces;
		break;
	case EPCGExGUIDFormat::DigitsWithHyphensInParentheses:
		Context->Format = EGuidFormats::DigitsWithHyphensInParentheses;
		break;
	case EPCGExGUIDFormat::HexValuesInBraces:
		Context->Format = EGuidFormats::HexValuesInBraces;
		break;
	case EPCGExGUIDFormat::UniqueObjectGuid:
		Context->Format = EGuidFormats::UniqueObjectGuid;
		break;
	case EPCGExGUIDFormat::Short:
		Context->Format = EGuidFormats::Short;
		break;
	case EPCGExGUIDFormat::Base36Encoded:
		Context->Format = EGuidFormats::Base36Encoded;
		break;
	}

	return true;
}

bool FPCGExWriteGUIDElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWriteGUIDElement::Execute);

	PCGEX_CONTEXT(WriteGUID)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExWriteGUID::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExWriteGUID::FProcessor>>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any points to process."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExWriteGUID
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWriteGUID::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		NumPoints = PointDataFacade->GetNum();

		uint32 BaseHash = 0;
		if (const AActor* TargetActor = Context->GetTargetActor(PointDataFacade->Source->GetIn())) { BaseHash = PCGEx::GH3(TargetActor->GetActorLocation(), CWTolerance); }

		DefaultGUID = FGuid(BaseHash, static_cast<uint32>(Settings->UniqueKey), PointDataFacade->Source->IOIndex, 42);
		UIDComponent = HashCombine(BaseHash, static_cast<uint32>(Settings->UniqueKey));

		if (Settings->OutputType == EPCGExGUIDOutputType::Integer)
		{
			IntegerGUIDWriter = PointDataFacade->GetWritable<int32>(Settings->OutputAttributeName, -1, Settings->bAllowInterpolation, PCGExData::EBufferInit::New);
		}
		else
		{
			StringGUIDWriter = PointDataFacade->GetWritable<FString>(Settings->OutputAttributeName, TEXT(""), Settings->bAllowInterpolation, PCGExData::EBufferInit::New);
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		const FGuid GUID = FGuid(UIDComponent, Index, Point.Seed, PCGEx::GH3(Point.Transform.GetLocation(), CWTolerance));

		if (IntegerGUIDWriter)
		{
			IntegerGUIDWriter->GetMutable(Index) = GetTypeHash(GUID.ToString(Context->Format));
		}
		else
		{
			StringGUIDWriter->GetMutable(Index) = GUID.ToString(Context->Format);
		}
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

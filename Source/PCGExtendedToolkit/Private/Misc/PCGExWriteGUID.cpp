// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExWriteGUID.h"

#include "Helpers/PCGHelpers.h"
#include "Misc/Guid.h"

#define LOCTEXT_NAMESPACE "PCGExWriteGUIDElement"
#define PCGEX_NAMESPACE WriteGUID

bool FPCGExGUIDDetails::Init(FPCGExContext* InContext, TSharedRef<PCGExData::FFacade>& InFacade)
{
	switch (Format)
	{
	default:
	case EPCGExGUIDFormat::Digits:
		GUIDFormat = EGuidFormats::Digits;
		break;
	case EPCGExGUIDFormat::DigitsLower:
		GUIDFormat = EGuidFormats::DigitsLower;
		break;
	case EPCGExGUIDFormat::DigitsWithHyphens:
		GUIDFormat = EGuidFormats::DigitsWithHyphens;
		break;
	case EPCGExGUIDFormat::DigitsWithHyphensLower:
		GUIDFormat = EGuidFormats::DigitsWithHyphensLower;
		break;
	case EPCGExGUIDFormat::DigitsWithHyphensInBraces:
		GUIDFormat = EGuidFormats::DigitsWithHyphensInBraces;
		break;
	case EPCGExGUIDFormat::DigitsWithHyphensInParentheses:
		GUIDFormat = EGuidFormats::DigitsWithHyphensInParentheses;
		break;
	case EPCGExGUIDFormat::HexValuesInBraces:
		GUIDFormat = EGuidFormats::HexValuesInBraces;
		break;
	case EPCGExGUIDFormat::UniqueObjectGuid:
		GUIDFormat = EGuidFormats::UniqueObjectGuid;
		break;
	case EPCGExGUIDFormat::Short:
		GUIDFormat = EGuidFormats::Short;
		break;
	case EPCGExGUIDFormat::Base36Encoded:
		GUIDFormat = EGuidFormats::Base36Encoded;
		break;
	}

	AdjustedGridHashCollision = FVector(1 / GridHashCollision.X, 1 / GridHashCollision.Y, 1 / GridHashCollision.Z);
	AdjustedPositionHashCollision = FVector(1 / PositionHashCollision.X, 1 / PositionHashCollision.Y, 1 / PositionHashCollision.Z);

	bUseIndex = (Uniqueness & static_cast<uint8>(EPCGExGUIDUniquenessFlags::Index)) != 0;
	bUseSeed = (Uniqueness & static_cast<uint8>(EPCGExGUIDUniquenessFlags::Seed)) != 0;
	bUsePosition = (Uniqueness & static_cast<uint8>(EPCGExGUIDUniquenessFlags::Position)) != 0;

	if (UniqueKeyInput == EPCGExInputValueType::Attribute)
	{
		UniqueKeyReader = InFacade->GetScopedBroadcaster<int32>(UniqueKeyAttribute);
		if (!UniqueKeyReader)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Invalid UniqueKey attribute: \"{0}\"."), FText::FromName(UniqueKeyAttribute.GetName())));
			return false;
		}
	}

	const uint32 BaseUniqueKey = UniqueKeyReader ? 0 : UniqueKeyConstant;
	if ((Uniqueness & static_cast<uint8>(EPCGExGUIDUniquenessFlags::Grid)) != 0)
	{
		const FBox RefBounds = PCGHelpers::GetGridBounds(InContext->GetTargetActor(InFacade->Source->GetIn()), InContext->SourceComponent.Get());
		GridHash = HashCombine(BaseUniqueKey, PCGEx::GH3(RefBounds.Min, AdjustedGridHashCollision));
		GridHash = HashCombine(GridHash, PCGEx::GH3(RefBounds.Max, AdjustedGridHashCollision));
	}
	else
	{
		GridHash = BaseUniqueKey;
	}

	DefaultGUID = FGuid(GridHash, 0, 0, 0);

	return true;
}

void FPCGExGUIDDetails::GetGUID(const int32 Index, const FPCGPoint& InPoint, FGuid& OutGUID) const
{
	const uint32 SeededBase = bUseSeed ? InPoint.Seed : 0;
	OutGUID = FGuid(
		GridHash,
		bUseIndex ? Index : -1,
		UniqueKeyReader ? HashCombine(SeededBase, static_cast<uint32>(UniqueKeyReader->Read(Index))) : SeededBase,
		bUsePosition ? PCGEx::GH3(InPoint.Transform.GetLocation() + PositionHashOffset, AdjustedPositionHashCollision) : 0);
}

PCGExData::EIOInit UPCGExWriteGUIDSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::Duplicate; }

PCGEX_INITIALIZE_ELEMENT(WriteGUID)

bool FPCGExWriteGUIDElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WriteGUID)

	PCGEX_VALIDATE_NAME(Settings->Config.OutputAttributeName)

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

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		Config = Settings->Config;

		if (!Config.Init(Context, PointDataFacade)) { return false; }

		if (Config.OutputType == EPCGExGUIDOutputType::Integer)
		{
			IntegerGUIDWriter = PointDataFacade->GetWritable<int32>(Config.OutputAttributeName, -1, Config.bAllowInterpolation, PCGExData::EBufferInit::New);
		}
		else
		{
			StringGUIDWriter = PointDataFacade->GetWritable<FString>(Config.OutputAttributeName, TEXT(""), Config.bAllowInterpolation, PCGExData::EBufferInit::New);
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const PCGExMT::FScope& Scope)
	{
		TPointsProcessor::PrepareSingleLoopScopeForPoints(Scope);
		PointDataFacade->Fetch(Scope);
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		FGuid GUID = FGuid();
		Config.GetGUID(Index, Point, GUID);

		if (IntegerGUIDWriter) { IntegerGUIDWriter->GetMutable(Index) = GetTypeHash(GUID.ToString(Config.GUIDFormat)); }
		else { StringGUIDWriter->GetMutable(Index) = GUID.ToString(Config.GUIDFormat); }
	}

	void FProcessor::CompleteWork()
	{
		PointDataFacade->Write(AsyncManager);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

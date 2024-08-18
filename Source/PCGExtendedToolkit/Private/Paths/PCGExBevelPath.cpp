// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExBevelPath.h"

#include "Data/PCGExPointFilter.h"

#define LOCTEXT_NAMESPACE "PCGExBevelPathElement"
#define PCGEX_NAMESPACE BevelPath

TArray<FPCGPinProperties> UPCGExBevelPathSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGEX_PIN_PARAMS(PCGExBevelPath::SourceBevelFilters, "Filters used to know if a point should be Beveled", Normal, {})
	if (Type == EPCGExBevelProfileType::Custom) { PCGEX_PIN_POINT(PCGExBevelPath::SourceCustomProfile, "Single path used as bevel profile", Required, {}) }
	return PinProperties;
}

PCGExData::EInit UPCGExBevelPathSettings::GetMainOutputInitMode() const { return PCGExData::EInit::NoOutput; }

PCGEX_INITIALIZE_ELEMENT(BevelPath)

void UPCGExBevelPathSettings::InitOutputFlags(const PCGExData::FPointIO* InPointIO) const
{
	UPCGMetadata* Metadata = InPointIO->GetOut()->Metadata;
	if (bFlagStartPoint) { Metadata->FindOrCreateAttribute(StartPointFlagName, false); }
	if (bFlagEndPoint) { Metadata->FindOrCreateAttribute(EndPointFlagName, false); }
	if (bFlagSubdivision) { Metadata->FindOrCreateAttribute(SubdivisionFlagName, false); }
}

FPCGExBevelPathContext::~FPCGExBevelPathContext()
{
	PCGEX_TERMINATE_ASYNC

	BevelFilterFactories.Empty();
}

bool FPCGExBevelPathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BevelPath)

	GetInputFactories(Context, PCGExBevelPath::SourceBevelFilters, Context->BevelFilterFactories, PCGExFactories::PointFilters, false);

	if (Settings->bFlagEndpoints) { PCGEX_VALIDATE_NAME(Settings->EndpointsFlagName) }
	if (Settings->bFlagStartPoint) { PCGEX_VALIDATE_NAME(Settings->StartPointFlagName) }
	if (Settings->bFlagEndPoint) { PCGEX_VALIDATE_NAME(Settings->EndPointFlagName) }
	if (Settings->bFlagSubdivision) { PCGEX_VALIDATE_NAME(Settings->SubdivisionFlagName) }

	Context->MainPaths = new PCGExData::FPointIOCollection(Context);
	Context->MainPaths->DefaultOutputLabel = Settings->GetMainOutputLabel();

	return true;
}

bool FPCGExBevelPathElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBevelPathElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BevelPath)

	if (Context->IsSetup())
	{
		if (!Boot(Context)) { return true; }

		bool bHasInvalidInputs = false;
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBevelPath::FProcessor>>(
			[&](PCGExData::FPointIO* Entry)
			{
				if (Entry->GetNum() < 3)
				{
					Entry->InitializeOutput(PCGExData::EInit::DuplicateInput);
					Settings->InitOutputFlags(Entry);
					bHasInvalidInputs = true;
					return false;
				}

				return true;
			},
			[&](PCGExPointsMT::TBatch<PCGExBevelPath::FProcessor>* NewBatch)
			{
			},
			PCGExMT::State_Done))
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Could not find any paths to Bevel."));
			return true;
		}

		if (bHasInvalidInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("Some inputs have less than 3 points and won't be processed."));
		}
	}

	if (!Context->ProcessPointsBatch()) { return false; }

	Context->MainPaths->OutputToContext();

	return Context->TryComplete();
}

namespace PCGExBevelPath
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE_TARRAY(Bevels)
		StartIndices.Empty();
	}

	bool FProcessor::Process(PCGExMT::FTaskManager* AsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBevelPath::Process);
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BevelPath)

		LocalTypedContext = TypedContext;
		LocalSettings = Settings;

		if (!FPointsProcessor::Process(AsyncManager)) { return false; }

		PointDataFacade->bSupportsDynamic = true;

		bInlineProcessPoints = true;
		bClosedPath = Settings->bClosedPath;

		PCGEX_SET_NUM_UNINITIALIZED_NULL(Bevels, PointIO->GetNum())

		if (!InitPrimaryFilters(&TypedContext->BevelFilterFactories))
		{
			// TODO : Throw error/warning, some filter metadata is missing
			return false;
		}

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);

		if (PrimaryFilters) { for (int i = 0; i < Count; i++) { DoBevel[i + StartIndex] = PrimaryFilters->Test(i + StartIndex); } }

		if (!bClosedPath)
		{
			// Ensure bevel is disabled on start/end points
			DoBevel[0] = false;
			DoBevel[DoBevel.Num() - 1] = false;
		}
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		if (!DoBevel[Index]) { return; }

		FBevel* NewBevel = new FBevel(Index);
		Bevels[Index] = NewBevel;
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		const FBevel* Bevel = Bevels[Iteration];
		if (!Bevel)
		{
			// TODO : Just insert existing point and move on
			return;
		}
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BevelPath)

		PCGEX_SET_NUM_UNINITIALIZED(StartIndices, PointIO->GetNum())

		int32 NumBevels = 0;
		int32 NumOutPoints = 0;
		for (int i = 0; i < StartIndices.Num(); i++)
		{
			StartIndices[i] = NumOutPoints;

			if (FBevel* Bevel = Bevels[i])
			{
				NumBevels++;
				
				Bevel->StartOutputIndex = NumOutPoints++;
				NumOutPoints += Bevel->Subdivisions.Num() + 1; // End + subdivs
				Bevel->EndOutputIndex = NumOutPoints++;

				continue;
			}

			NumOutPoints++;
		}

		if (NumBevels == 0)
		{
			PointIO->InitializeOutput(PCGExData::EInit::DuplicateInput);
			Settings->InitOutputFlags(PointIO);
			return;
		}

		PointIO->InitializeOutput(PCGExData::EInit::NewOutput);
		Settings->InitOutputFlags(PointIO);

		// Build output points

		const TArray<FPCGPoint>& InputPoints = PointIO->GetIn()->GetPoints();
		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
		PCGEX_SET_NUM(MutablePoints, NumOutPoints);

		UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;
		for (int i = 0; i < StartIndices.Num(); i++)
		{
			const int32 StartIndex = StartIndices[i];
			const FPCGPoint& OriginalPoint = InputPoints[i];

			if (const FBevel* Bevel = Bevels[i])
			{
				for (int j = Bevel->StartOutputIndex; j <= Bevel->EndOutputIndex; j++)
				{
					MutablePoints[j] = OriginalPoint;
					Metadata->InitializeOnSet(MutablePoints[j].MetadataEntry);
				}
			}
			else
			{
				MutablePoints[StartIndex] = OriginalPoint;
				Metadata->InitializeOnSet(MutablePoints[StartIndex].MetadataEntry);
			}
		}

		PointIO->CreateOutKeys();

		bInlineProcessRange = true;
		StartParallelLoopForRange(PointIO->GetNum());
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

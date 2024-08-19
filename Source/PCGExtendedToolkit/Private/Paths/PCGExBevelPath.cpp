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

	Context->MainPoints->OutputToContext();

	return Context->TryComplete();
}

PCGExBevelPath::FSegmentInfos::FSegmentInfos(const FProcessor* InProcessor, const int32 InStartIndex, const int32 InEndIndex):
	StartIndex(InStartIndex), EndIndex(InEndIndex)
{
	Start = InProcessor->PointDataFacade->Source->GetInPoint(StartIndex).Transform.GetLocation();
	End = InProcessor->PointDataFacade->Source->GetInPoint(EndIndex).Transform.GetLocation();
}

namespace PCGExBevelPath
{
	FProcessor::~FProcessor()
	{
		PCGEX_DELETE_TARRAY(Bevels)

		Segments.Empty();
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

		PCGEX_SET_NUM_NULLPTR(Bevels, PointIO->GetNum())
		PCGEX_SET_NUM_UNINITIALIZED(Segments, PointIO->GetNum())

		if (!InitPrimaryFilters(&TypedContext->BevelFilterFactories))
		{
			// TODO : Throw error/warning, some filter metadata is missing
			return false;
		}

		if (Settings->WidthSource == EPCGExFetchType::Attribute)
		{
			WidthGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->WidthAttribute);
			if (!WidthGetter)
			{
				// TODO : Throw error/warning, some filter metadata is missing
				return false;
			}
		}

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::PrepareSingleLoopScopeForPoints(const uint32 StartIndex, const int32 Count)
	{
		PointDataFacade->Fetch(StartIndex, Count);

		if (PrimaryFilters) { for (int i = 0; i < Count; i++) { PointFilterCache[i + StartIndex] = PrimaryFilters->Test(i + StartIndex); } }

		if (!bClosedPath)
		{
			// Ensure bevel is disabled on start/end points
			PointFilterCache[0] = false;
			PointFilterCache[PointFilterCache.Num() - 1] = false;
		}
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		if (!PointFilterCache[Index]) { return; }

		FBevel* NewBevel = new FBevel(Index);
		Bevels[Index] = NewBevel;
	}

	void FProcessor::PrepareSinglePoint(const int32 Index, const FPCGPoint& Point)
	{
		const int32 StartIndex = StartIndices[Index];

		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
		UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;

		if (StartIndices.IsValidIndex(Index + 1)) { Segments[Index] = FSegmentInfos(this, Index, Index + 1); }
		else { Segments[Index] = FSegmentInfos(this, Index, 0); }

		if (const FBevel* Bevel = Bevels[Index])
		{
			for (int j = Bevel->StartOutputIndex; j <= Bevel->EndOutputIndex; j++)
			{
				MutablePoints[j] = Point;
				Metadata->InitializeOnSet(MutablePoints[j].MetadataEntry);
			}
		}
		else
		{
			MutablePoints[StartIndex] = Point;
			Metadata->InitializeOnSet(MutablePoints[StartIndex].MetadataEntry);
		}
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		FBevel* Bevel = Bevels[Iteration];
		if (!Bevel) { return; }

		FPCGPoint& StartPoint = PointIO->GetMutablePoint(Bevel->StartOutputIndex);
		FPCGPoint& EndPoint = PointIO->GetMutablePoint(Bevel->EndOutputIndex);

		const FVector Corner = PointIO->GetInPoint(Bevel->Index).Transform.GetLocation();
		const FSegmentInfos& PrevSegment = Bevel->Index == 0 ? Segments.Last() : Segments[Bevel->Index - 1];
		const FSegmentInfos& NextSegment = Segments[Bevel->Index];

		const FVector DirToPrev = PrevSegment.GetEndStartDir();
		const FVector DirToNext = NextSegment.GetStartEndDir();

		const double Width = LocalSettings->WidthSource == EPCGExFetchType::Constant ? LocalSettings->WidthConstant : WidthGetter->Values[Iteration];

		if (LocalSettings->WidthMeasure == EPCGExMeanMeasure::Relative)
		{
			const double LengthToPrev = PrevSegment.GetLength();
			const double LengthToNext = PrevSegment.GetLength();

			if (LocalSettings->bUseSmallestRelative)
			{
				const double SmallestLength = FMath::Min(LengthToPrev, LengthToNext);
				Bevel->Start = Corner + DirToPrev * (SmallestLength * Width);
				Bevel->End = Corner + DirToNext * (SmallestLength * Width);
			}
			else
			{
				Bevel->Start = Corner + DirToPrev * (LengthToPrev * Width);
				Bevel->End = Corner + DirToNext * (LengthToNext * Width);
			}
		}
		else
		{
			Bevel->Start = Corner + DirToPrev * Width;
			Bevel->End = Corner + DirToNext * Width;
		}

		StartPoint.Transform.SetLocation(Bevel->Start);
		EndPoint.Transform.SetLocation(Bevel->End);

		if (Subdivisions == 0) { return; }

		PCGEX_SET_NUM_UNINITIALIZED(Bevel->Subdivisions, Subdivisions);

		// TODO : Subdivisions
	}

	void FProcessor::CompleteWork()
	{
		PCGEX_TYPED_CONTEXT_AND_SETTINGS(BevelPath)

		PCGEX_SET_NUM_UNINITIALIZED(StartIndices, PointIO->GetNum())

		const int32 Increment = LocalSettings->bKeepCornerPoint ? 2 : Settings->NumSubdivision + 1; // End + subdivs

		int32 NumBevels = 0;
		int32 NumOutPoints = 0;

		for (int i = 0; i < StartIndices.Num(); i++)
		{
			StartIndices[i] = NumOutPoints;

			if (FBevel* Bevel = Bevels[i])
			{
				NumBevels++;

				Bevel->StartOutputIndex = NumOutPoints;
				NumOutPoints += Increment;
				Bevel->EndOutputIndex = NumOutPoints;
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

		PCGExMT::FTaskGroup* PrepGroup = AsyncManagerPtr->CreateGroup();
		
		PrepGroup->SetOnCompleteCallback([&]() { StartParallelLoopForRange(PointIO->GetNum()); });

		PrepGroup->StartRanges(
			[&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				PrepareSinglePoint(Index, InputPoints[Index]);
			}, StartIndices.Num(), 64, false, false);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

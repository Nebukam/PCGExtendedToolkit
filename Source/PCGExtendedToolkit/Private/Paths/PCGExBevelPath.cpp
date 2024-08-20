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

namespace PCGExBevelPath
{
	FBevel::FBevel(const int32 InIndex, const FProcessor* InProcessor):
		Index(InIndex)
	{
		const TArray<FPCGPoint>& InPoints = InProcessor->PointIO->GetIn()->GetPoints();
		ArriveIdx = Index - 1 < 0 ? InPoints.Num() - 1 : Index - 1;
		LeaveIdx = Index + 1 == InPoints.Num() ? 0 : Index + 1;

		Corner = InPoints[InIndex].Transform.GetLocation();
		PrevLocation = InPoints[ArriveIdx].Transform.GetLocation();
		NextLocation = InPoints[LeaveIdx].Transform.GetLocation();

		// Pre-compute some data

		ArriveDir = (PrevLocation - Corner).GetSafeNormal();
		LeaveDir = (NextLocation - Corner).GetSafeNormal();

		Width = InProcessor->WidthGetter ? InProcessor->WidthGetter->Values[Index] : InProcessor->LocalSettings->WidthConstant;

		const double ArriveLen = InProcessor->Len(ArriveIdx);
		const double LeaveLen = InProcessor->Len(Index);
		const double SmallestLength = FMath::Min(ArriveLen, LeaveLen);

		if (InProcessor->LocalSettings->WidthMeasure == EPCGExMeanMeasure::Relative)
		{
			Width *= SmallestLength;
		}

		if (InProcessor->LocalSettings->Mode == EPCGExBevelMode::Radius)
		{
			Width = Width / FMath::Sin(FMath::Acos(FVector::DotProduct(ArriveDir, LeaveDir)) / 2.0f);
		}

		if (InProcessor->LocalSettings->Limit != EPCGExBevelLimit::None)
		{
			Width = FMath::Min(Width, SmallestLength);
		}

		ArriveAlpha = Width / ArriveLen;
		LeaveAlpha = Width / LeaveLen;
	}

	void FBevel::Balance(const FProcessor* InProcessor)
	{
		const FBevel* PrevBevel = InProcessor->Bevels[ArriveIdx];
		const FBevel* NextBevel = InProcessor->Bevels[LeaveIdx];

		double ArriveAlphaSum = ArriveAlpha;
		double LeaveAlphaSum = LeaveAlpha;

		if (PrevBevel) { ArriveAlphaSum += PrevBevel->LeaveAlpha; }
		else { ArriveAlphaSum = 1; }

		Width = FMath::Min(Width, InProcessor->Len(ArriveIdx) * (ArriveAlpha * (1 / ArriveAlphaSum)));

		if (NextBevel) { LeaveAlphaSum += NextBevel->ArriveAlpha; }
		else { LeaveAlphaSum = 1; }

		Width = FMath::Min(Width, InProcessor->Len(Index) * (LeaveAlpha * (1 / LeaveAlphaSum)));
	}

	void FBevel::Compute()
	{
		Arrive = Corner + Width * ArriveDir;
		Leave = Corner + Width * LeaveDir;

		// TODO : compute final subdivision count
		
	}

	FProcessor::~FProcessor()
	{
		PCGEX_DELETE_TARRAY(Bevels)
		Lengths.Empty();
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

		const TArray<FPCGPoint>& InPoints = PointIO->GetIn()->GetPoints();
		const int32 NumPoints = InPoints.Num();
		PCGEX_SET_NUM_UNINITIALIZED(Lengths, NumPoints)
		for (int i = 0; i < NumPoints; i++)
		{
			Lengths[i] = FVector::Distance(InPoints[i].Transform.GetLocation(), InPoints[i + 1 == NumPoints ? 0 : i + 1].Transform.GetLocation());
		}

		PCGExMT::FTaskGroup* Preparation = AsyncManagerPtr->CreateGroup();
		Preparation->SetOnCompleteCallback([&]() { StartParallelLoopForPoints(PCGExData::ESource::In); });
		Preparation->SetOnIterationRangeStartCallback(
			[&](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				PointDataFacade->Fetch(StartIndex, Count);

				if (PrimaryFilters) { for (int i = 0; i < Count; i++) { PointFilterCache[i + StartIndex] = PrimaryFilters->Test(i + StartIndex); } }

				if (!bClosedPath)
				{
					// Ensure bevel is disabled on start/end points
					PointFilterCache[0] = false;
					PointFilterCache[PointFilterCache.Num() - 1] = false;
				}
			});

		Preparation->StartRanges(
			[&](const int32 Index, const int32 Count, const int32 LoopIdx)
			{
				if (!PointFilterCache[Index]) { return; }
				FBevel* NewBevel = new FBevel(Index, this);
				Bevels[Index] = NewBevel;
			}, PointIO->GetNum(), 64);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		FBevel* Bevel = Bevels[Index];
		if (!Bevel) { return; }

		if (LocalSettings->Limit == EPCGExBevelLimit::Balanced) { Bevel->Balance(this); }
		Bevel->Compute();
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		const int32 StartIndex = StartIndices[Iteration];

		FBevel* Bevel = Bevels[Iteration];
		const FPCGPoint& OriginalPoint = PointIO->GetInPoint(Iteration);

		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
		UPCGMetadata* Metadata = PointIO->GetOut()->Metadata;

		if (!Bevel)
		{
			MutablePoints[StartIndex] = OriginalPoint;
			Metadata->InitializeOnSet(MutablePoints[StartIndex].MetadataEntry);
			return;
		}

		for (int j = Bevel->StartOutputIndex; j <= Bevel->EndOutputIndex; j++)
		{
			MutablePoints[j] = OriginalPoint;
			Metadata->InitializeOnSet(MutablePoints[j].MetadataEntry);
		}

		FPCGPoint& StartPoint = PointIO->GetMutablePoint(Bevel->StartOutputIndex);
		FPCGPoint& EndPoint = PointIO->GetMutablePoint(Bevel->EndOutputIndex);

		StartPoint.Transform.SetLocation(Bevel->Arrive);
		EndPoint.Transform.SetLocation(Bevel->Leave);

		if (Subdivisions == 0) { return; }

		PCGEX_SET_NUM_UNINITIALIZED(Bevel->Subdivisions, Subdivisions);

		// TODO : Subdivisions & blending
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

		TArray<FPCGPoint>& MutablePoints = PointIO->GetOut()->GetMutablePoints();
		PCGEX_SET_NUM(MutablePoints, NumOutPoints);

		StartParallelLoopForRange(PointIO->GetNum());
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

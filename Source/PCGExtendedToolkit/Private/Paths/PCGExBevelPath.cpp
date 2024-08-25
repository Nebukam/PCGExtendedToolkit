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
	if (bFlagEndpoints) { Metadata->FindOrCreateAttribute(EndpointsFlagName, false); }
	if (bFlagStartPoint) { Metadata->FindOrCreateAttribute(StartPointFlagName, false); }
	if (bFlagEndPoint) { Metadata->FindOrCreateAttribute(EndPointFlagName, false); }
	if (bFlagSubdivision) { Metadata->FindOrCreateAttribute(SubdivisionFlagName, false); }
}

FPCGExBevelPathContext::~FPCGExBevelPathContext()
{
	PCGEX_TERMINATE_ASYNC

	PCGEX_DELETE_FACADE_AND_SOURCE(CustomProfileFacade)

	CustomProfilePositions.Empty();
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

	if (Settings->Type == EPCGExBevelProfileType::Custom)
	{
		PCGExData::FPointIO* CustomProfileIO = PCGExData::TryGetSingleInput(Context, PCGExBevelPath::SourceCustomProfile, true);
		if (!CustomProfileIO) { return false; }

		if (CustomProfileIO->GetNum() < 2)
		{
			PCGEX_DELETE(CustomProfileIO)
			PCGE_LOG(Error, GraphAndLog, FTEXT("Custom profile must have at least two points."));
			return false;
		}

		Context->CustomProfileFacade = new PCGExData::FFacade(CustomProfileIO);

		const TArray<FPCGPoint>& ProfilePoints = CustomProfileIO->GetIn()->GetPoints();
		PCGEX_SET_NUM_UNINITIALIZED(Context->CustomProfilePositions, ProfilePoints.Num())

		const FVector Start = ProfilePoints[0].Transform.GetLocation();
		const FVector End = ProfilePoints.Last().Transform.GetLocation();
		const double Factor = 1 / FVector::Dist(Start, End);

		const FVector ProjectionNormal = (End - Start).GetSafeNormal(1E-08, FVector::ForwardVector);
		const FQuat ProjectionQuat = FQuat::FindBetweenNormals(ProjectionNormal, FVector::ForwardVector);

		for (int i = 0; i < ProfilePoints.Num(); i++)
		{
			Context->CustomProfilePositions[i] = ProjectionQuat.RotateVector((ProfilePoints[i].Transform.GetLocation() - Start) * Factor);
		}
	}

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

	void FBevel::Compute(const FProcessor* InProcessor)
	{
		Arrive = Corner + Width * ArriveDir;
		Leave = Corner + Width * LeaveDir;

		// TODO : compute final subdivision count

		if (InProcessor->LocalSettings->Type == EPCGExBevelProfileType::Custom)
		{
			SubdivideCustom(InProcessor);
			return;
		}

		if (!InProcessor->bSubdivide) { return; }

		const double Amount = InProcessor->SubdivAmountGetter ? InProcessor->SubdivAmountGetter->Values[Index] : InProcessor->ConstantSubdivAmount;

		if (!InProcessor->bArc) { SubdivideLine(Amount, InProcessor->bSubdivideCount); }
		else { SubdivideArc(Amount, InProcessor->bSubdivideCount); }
	}

	void FBevel::SubdivideLine(const double Factor, const double bIsCount)
	{
		const double Dist = FVector::Dist(Arrive, Leave);
		const FVector Dir = (Leave - Arrive).GetSafeNormal();

		int32 SubdivCount = Factor;
		double StepSize = 0;

		if (bIsCount)
		{
			StepSize = Dist / static_cast<double>(SubdivCount + 1);
		}
		else
		{
			StepSize = FMath::Min(Dist, Factor);
			SubdivCount = FMath::Floor(Dist / Factor);
		}

		SubdivCount = FMath::Max(0, SubdivCount);
		PCGEX_SET_NUM(Subdivisions, SubdivCount)


		for (int i = 0; i < SubdivCount; i++) { Subdivisions[i] = Arrive + Dir * (StepSize + i * StepSize); }
	}

	void FBevel::SubdivideArc(const double Factor, const double bIsCount)
	{
		const PCGExGeo::FExCenterArc Arc = PCGExGeo::FExCenterArc(Arrive, Corner, Leave);
		int32 SubdivCount = bIsCount ? Factor : FMath::Floor(Arc.GetLength() / Factor);

		const double StepSize = 1 / static_cast<double>(SubdivCount + 1);
		PCGEX_SET_NUM(Subdivisions, SubdivCount)

		for (int i = 0; i < SubdivCount; i++) { Subdivisions[i] = Arc.GetLocationOnArc(StepSize + i * StepSize); }
	}

	void FBevel::SubdivideCustom(const FProcessor* InProcessor)
	{
		const TArray<FVector>& SourcePos = InProcessor->LocalTypedContext->CustomProfilePositions;
		const int32 SubdivCount = SourcePos.Num() - 2;

		PCGEX_SET_NUM(Subdivisions, SubdivCount)

		if (SubdivCount == 0) { return; }

		const double Factor = FVector::Dist(Leave, Arrive);
		const FVector ProjectionNormal = (Leave - Arrive).GetSafeNormal(1E-08, FVector::ForwardVector);
		const FQuat ProjectionQuat = FQuat::FindBetweenNormals(FVector::ForwardVector, ProjectionNormal);

		for (int i = 0; i < SubdivCount; i++)
		{
			Subdivisions[i] = Arrive + ProjectionQuat.RotateVector(SourcePos[i + 1] * Factor);
		}
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
			PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Some filter data is invalid or missing."));
			return false;
		}

		if (Settings->WidthSource == EPCGExFetchType::Attribute)
		{
			WidthGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->WidthAttribute);
			if (!WidthGetter)
			{
				PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Width attribute data is invalid or missing."));
				return false;
			}
		}

		if (Settings->bSubdivide)
		{
			bSubdivide = !Settings->bKeepCornerPoint && Settings->Type != EPCGExBevelProfileType::Custom;
			if (bSubdivide)
			{
				bSubdivideCount = Settings->SubdivideMethod == EPCGExSubdivideMode::Count;

				if (Settings->SubdivisionValueSource == EPCGExFetchType::Attribute)
				{
					SubdivAmountGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->SubdivisionAmount);
					if (!SubdivAmountGetter)
					{
						PCGE_LOG_C(Error, GraphAndLog, Context, FTEXT("Subdivision Amount attribute is invalid or missing."));
						return false;
					}
				}
				else
				{
					ConstantSubdivAmount = bSubdivideCount ? Settings->SubdivisionCount : Settings->SubdivisionDistance;
				}
			}
		}

		bArc = Settings->Type == EPCGExBevelProfileType::Arc;

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
		Bevel->Compute(this);
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

		for (int i = Bevel->StartOutputIndex; i <= Bevel->EndOutputIndex; i++)
		{
			MutablePoints[i] = OriginalPoint;
			Metadata->InitializeOnSet(MutablePoints[i].MetadataEntry);
		}

		FPCGPoint& StartPoint = PointIO->GetMutablePoint(Bevel->StartOutputIndex);
		FPCGPoint& EndPoint = PointIO->GetMutablePoint(Bevel->EndOutputIndex);

		StartPoint.Transform.SetLocation(Bevel->Arrive);
		EndPoint.Transform.SetLocation(Bevel->Leave);

		if (Bevel->Subdivisions.IsEmpty()) { return; }

		for (int i = 0; i < Bevel->Subdivisions.Num(); i++)
		{
			MutablePoints[Bevel->StartOutputIndex + i + 1].Transform.SetLocation(Bevel->Subdivisions[i]);
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

				Bevel->StartOutputIndex = NumOutPoints;
				NumOutPoints += Bevel->Subdivisions.Num() + 1;
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

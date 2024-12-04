// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExBevelPath.h"

#include "PCGExRandom.h"
#include "Data/PCGExPointFilter.h"


#define LOCTEXT_NAMESPACE "PCGExBevelPathElement"
#define PCGEX_NAMESPACE BevelPath

TArray<FPCGPinProperties> UPCGExBevelPathSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (Type == EPCGExBevelProfileType::Custom) { PCGEX_PIN_POINT(PCGExBevelPath::SourceCustomProfile, "Single path used as bevel profile", Required, {}) }
	return PinProperties;
}

PCGExData::EIOInit UPCGExBevelPathSettings::GetMainOutputInitMode() const { return PCGExData::EIOInit::None; }

PCGEX_INITIALIZE_ELEMENT(BevelPath)

void UPCGExBevelPathSettings::InitOutputFlags(const TSharedPtr<PCGExData::FPointIO>& InPointIO) const
{
	if (bFlagEndpoints) { InPointIO->FindOrCreateAttribute(EndpointsFlagName, false); }
	if (bFlagStartPoint) { InPointIO->FindOrCreateAttribute(StartPointFlagName, false); }
	if (bFlagEndPoint) { InPointIO->FindOrCreateAttribute(EndPointFlagName, false); }
	if (bFlagSubdivision) { InPointIO->FindOrCreateAttribute(SubdivisionFlagName, false); }
}

bool FPCGExBevelPathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BevelPath)

	if (Settings->bFlagEndpoints) { PCGEX_VALIDATE_NAME(Settings->EndpointsFlagName) }
	if (Settings->bFlagStartPoint) { PCGEX_VALIDATE_NAME(Settings->StartPointFlagName) }
	if (Settings->bFlagEndPoint) { PCGEX_VALIDATE_NAME(Settings->EndPointFlagName) }
	if (Settings->bFlagSubdivision) { PCGEX_VALIDATE_NAME(Settings->SubdivisionFlagName) }

	if (Settings->Type == EPCGExBevelProfileType::Custom)
	{
		const TSharedPtr<PCGExData::FPointIO> CustomProfileIO = PCGExData::TryGetSingleInput(Context, PCGExBevelPath::SourceCustomProfile, true);
		if (!CustomProfileIO) { return false; }

		if (CustomProfileIO->GetNum() < 2)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Custom profile must have at least two points."));
			return false;
		}

		Context->CustomProfileFacade = MakeShared<PCGExData::FFacade>(CustomProfileIO.ToSharedRef());

		const TArray<FPCGPoint>& ProfilePoints = CustomProfileIO->GetIn()->GetPoints();
		PCGEx::InitArray(Context->CustomProfilePositions, ProfilePoints.Num());

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
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 3 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExBevelPath::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					if (!Settings->bOmitInvalidPathsOutputs) { Entry->InitializeOutput(PCGExData::EIOInit::Forward); }
					bHasInvalidInputs = true;
					return false;
				}

				if (Entry->GetNum() < 3)
				{
					Entry->InitializeOutput(PCGExData::EIOInit::Duplicate);
					Settings->InitOutputFlags(Entry);
					bHasInvalidInputs = true;
					return false;
				}

				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExBevelPath::FProcessor>>& NewBatch)
			{
				NewBatch->bRequiresWriteStep = (Settings->bFlagEndpoints || Settings->bFlagSubdivision || Settings->bFlagEndPoint || Settings->bFlagStartPoint);
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to Bevel."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	Context->MainPoints->StageOutputs();

	return Context->TryComplete();
}

namespace PCGExBevelPath
{
	FBevel::FBevel(const int32 InIndex, const FProcessor* InProcessor):
		Index(InIndex)
	{
		const TArray<FPCGPoint>& InPoints = InProcessor->PointDataFacade->GetIn()->GetPoints();
		ArriveIdx = Index - 1 < 0 ? InPoints.Num() - 1 : Index - 1;
		LeaveIdx = Index + 1 == InPoints.Num() ? 0 : Index + 1;

		Corner = InPoints[InIndex].Transform.GetLocation();
		PrevLocation = InPoints[ArriveIdx].Transform.GetLocation();
		NextLocation = InPoints[LeaveIdx].Transform.GetLocation();

		// Pre-compute some data

		ArriveDir = (PrevLocation - Corner).GetSafeNormal();
		LeaveDir = (NextLocation - Corner).GetSafeNormal();

		Width = InProcessor->WidthGetter ? InProcessor->WidthGetter->Read(Index) : InProcessor->Settings->WidthConstant;

		const double ArriveLen = InProcessor->Len(ArriveIdx);
		const double LeaveLen = InProcessor->Len(Index);
		const double SmallestLength = FMath::Min(ArriveLen, LeaveLen);

		if (InProcessor->Settings->WidthMeasure == EPCGExMeanMeasure::Relative)
		{
			Width *= SmallestLength;
		}

		if (InProcessor->Settings->Mode == EPCGExBevelMode::Radius)
		{
			Width = Width / FMath::Sin(FMath::Acos(FVector::DotProduct(ArriveDir, LeaveDir)) / 2.0f);
		}

		if (InProcessor->Settings->Limit != EPCGExBevelLimit::None)
		{
			Width = FMath::Min(Width, SmallestLength);
		}

		ArriveAlpha = Width / ArriveLen;
		LeaveAlpha = Width / LeaveLen;
	}

	void FBevel::Balance(const FProcessor* InProcessor)
	{
		const TSharedPtr<FBevel>& PrevBevel = InProcessor->Bevels[ArriveIdx];
		const TSharedPtr<FBevel>& NextBevel = InProcessor->Bevels[LeaveIdx];

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
		if (InProcessor->Settings->Limit == EPCGExBevelLimit::Balanced) { Balance(InProcessor); }

		Arrive = Corner + Width * ArriveDir;
		Leave = Corner + Width * LeaveDir;

		// TODO : compute final subdivision count

		if (InProcessor->Settings->Type == EPCGExBevelProfileType::Custom)
		{
			SubdivideCustom(InProcessor);
			return;
		}

		if (!InProcessor->bSubdivide) { return; }

		const double Amount = InProcessor->SubdivAmountGetter ? InProcessor->SubdivAmountGetter->Read(Index) : InProcessor->ConstantSubdivAmount;

		if (!InProcessor->bArc) { SubdivideLine(Amount, InProcessor->bSubdivideCount); }
		else { SubdivideArc(Amount, InProcessor->bSubdivideCount); }
	}

	void FBevel::SubdivideLine(const double Factor, const bool bIsCount)
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
		PCGEx::InitArray(Subdivisions, SubdivCount);


		for (int i = 0; i < SubdivCount; i++) { Subdivisions[i] = Arrive + Dir * (StepSize + i * StepSize); }
	}

	void FBevel::SubdivideArc(const double Factor, const bool bIsCount)
	{
		const PCGExGeo::FExCenterArc Arc = PCGExGeo::FExCenterArc(Arrive, Corner, Leave);

		if (Arc.bIsLine)
		{
			// Fallback to line since we can't infer a proper radius
			SubdivideLine(Factor, bIsCount);
			return;
		}

		const int32 SubdivCount = bIsCount ? Factor : FMath::Floor(Arc.GetLength() / Factor);

		const double StepSize = 1 / static_cast<double>(SubdivCount + 1);
		PCGEx::InitArray(Subdivisions, SubdivCount);

		for (int i = 0; i < SubdivCount; i++) { Subdivisions[i] = Arc.GetLocationOnArc(StepSize + i * StepSize); }
	}

	void FBevel::SubdivideCustom(const FProcessor* InProcessor)
	{
		const TArray<FVector>& SourcePos = InProcessor->Context->CustomProfilePositions;
		const int32 SubdivCount = SourcePos.Num() - 2;

		PCGEx::InitArray(Subdivisions, SubdivCount);

		if (SubdivCount == 0) { return; }

		const double Factor = FVector::Dist(Leave, Arrive);
		const FVector ProjectionNormal = (Leave - Arrive).GetSafeNormal(1E-08, FVector::ForwardVector);
		const FQuat ProjectionQuat = FQuat::FindBetweenNormals(FVector::ForwardVector, ProjectionNormal);

		for (int i = 0; i < SubdivCount; i++)
		{
			Subdivisions[i] = Arrive + ProjectionQuat.RotateVector(SourcePos[i + 1] * Factor);
		}
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBevelPath::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		bInlineProcessPoints = true;
		bClosedLoop = Context->ClosedLoop.IsClosedLoop(PointDataFacade->Source);

		Bevels.Init(nullptr, PointDataFacade->GetNum());

		if (Settings->WidthInput == EPCGExInputValueType::Attribute)
		{
			WidthGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->WidthAttribute);
			if (!WidthGetter)
			{
				PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Width attribute data is invalid or missing."));
				return false;
			}
		}

		if (Settings->bSubdivide)
		{
			bSubdivide = !Settings->bKeepCornerPoint && Settings->Type != EPCGExBevelProfileType::Custom;
			if (bSubdivide)
			{
				bSubdivideCount = Settings->SubdivideMethod == EPCGExSubdivideMode::Count;

				if (Settings->SubdivisionAmountInput == EPCGExInputValueType::Attribute)
				{
					SubdivAmountGetter = PointDataFacade->GetScopedBroadcaster<double>(Settings->SubdivisionAmount);
					if (!SubdivAmountGetter)
					{
						PCGE_LOG_C(Error, GraphAndLog, ExecutionContext, FTEXT("Subdivision Amount attribute is invalid or missing."));
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

		const TArray<FPCGPoint>& InPoints = PointDataFacade->GetIn()->GetPoints();
		const int32 NumPoints = InPoints.Num();
		PCGEx::InitArray(Lengths, NumPoints);
		for (int i = 0; i < NumPoints; i++)
		{
			Lengths[i] = FVector::Distance(InPoints[i].Transform.GetLocation(), InPoints[i + 1 == NumPoints ? 0 : i + 1].Transform.GetLocation());
		}

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, Preparation)

		Preparation->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				if (!This->bClosedLoop)
				{
					// Ensure bevel is disabled on start/end points
					This->PointFilterCache[0] = false;
					This->PointFilterCache[This->PointFilterCache.Num() - 1] = false;
				}

				This->StartParallelLoopForPoints(PCGExData::ESource::In);
			};

		Preparation->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				PCGEX_ASYNC_THIS

				This->PointDataFacade->Fetch(StartIndex, Count);
				This->FilterScope(StartIndex, Count);

				if (!This->bClosedLoop)
				{
					// Ensure bevel is disabled on start/end points
					This->PointFilterCache[0] = false;
					This->PointFilterCache[This->PointFilterCache.Num() - 1] = false;
				}

				PCGEX_ASYNC_SUB_LOOP
				{
					if (!This->PointFilterCache[i]) { continue; }
					This->Bevels[i] = MakeShared<FBevel>(i, This.Get()); // no need for SharedThis
				}
			};

		Preparation->StartSubLoops(PointDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->PointsDefaultBatchChunkSize);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		const TSharedPtr<FBevel>& Bevel = Bevels[Index];
		if (!Bevel) { return; }

		Bevel->Compute(this);
	}

	void FProcessor::ProcessSingleRangeIteration(const int32 Iteration, const int32 LoopIdx, const int32 LoopCount)
	{
		const int32 StartIndex = StartIndices[Iteration];

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		const TSharedPtr<FBevel>& Bevel = Bevels[Iteration];
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

		PCGExRandom::ComputeSeed(StartPoint);
		PCGExRandom::ComputeSeed(EndPoint);

		if (Bevel->Subdivisions.IsEmpty()) { return; }

		for (int i = 0; i < Bevel->Subdivisions.Num(); i++)
		{
			FPCGPoint& Pt = MutablePoints[Bevel->StartOutputIndex + i + 1];
			Pt.Transform.SetLocation(Bevel->Subdivisions[i]);
			PCGExRandom::ComputeSeed(Pt);
		}
	}

	void FProcessor::WriteFlags(const int32 Index)
	{
		const TSharedPtr<FBevel>& Bevel = Bevels[Index];
		if (!Bevel) { return; }

		if (EndpointsWriter)
		{
			EndpointsWriter->GetMutable(Bevel->StartOutputIndex) = true;
			EndpointsWriter->GetMutable(Bevel->EndOutputIndex) = true;
		}

		if (StartPointWriter) { StartPointWriter->GetMutable(Bevel->StartOutputIndex) = true; }

		if (EndPointWriter) { EndPointWriter->GetMutable(Bevel->EndOutputIndex) = true; }

		if (SubdivisionWriter) { for (int i = 1; i <= Bevel->Subdivisions.Num(); i++) { SubdivisionWriter->GetMutable(Bevel->StartOutputIndex + i) = true; } }
	}

	void FProcessor::CompleteWork()
	{
		PCGEx::InitArray(StartIndices, PointDataFacade->GetNum());

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		int32 NumBevels = 0;
		int32 NumOutPoints = 0;

		for (int i = 0; i < StartIndices.Num(); i++)
		{
			StartIndices[i] = NumOutPoints;

			if (const TSharedPtr<FBevel>& Bevel = Bevels[i])
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
			PointIO->InitializeOutput(PCGExData::EIOInit::Duplicate);
			Settings->InitOutputFlags(PointIO);
			return;
		}

		PointIO->InitializeOutput(PCGExData::EIOInit::New);
		Settings->InitOutputFlags(PointIO);

		// Build output points

		TArray<FPCGPoint>& MutablePoints = PointDataFacade->GetOut()->GetMutablePoints();
		PCGEx::InitArray(MutablePoints, NumOutPoints);

		StartParallelLoopForRange(PointDataFacade->GetNum());
	}

	void FProcessor::Write()
	{
		if (Settings->bFlagEndpoints)
		{
			EndpointsWriter = PointDataFacade->GetWritable<bool>(Settings->EndpointsFlagName, false, true, PCGExData::EBufferInit::New);
		}

		if (Settings->bFlagStartPoint)
		{
			StartPointWriter = PointDataFacade->GetWritable<bool>(Settings->StartPointFlagName, false, true, PCGExData::EBufferInit::New);
		}

		if (Settings->bFlagEndPoint)
		{
			EndPointWriter = PointDataFacade->GetWritable<bool>(Settings->EndPointFlagName, false, true, PCGExData::EBufferInit::New);
		}

		if (Settings->bFlagSubdivision)
		{
			SubdivisionWriter = PointDataFacade->GetWritable<bool>(Settings->SubdivisionFlagName, false, true, PCGExData::EBufferInit::New);
		}

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, WriteFlagsTask)
		WriteFlagsTask->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				This->PointDataFacade->Write(This->AsyncManager);
			};

		WriteFlagsTask->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const int32 StartIndex, const int32 Count, const int32 LoopIdx)
			{
				PCGEX_ASYNC_THIS
				PCGEX_ASYNC_SUB_LOOP
				{
					if (!This->PointFilterCache[i]) { continue; }
					This->WriteFlags(i);
				}
			};

		WriteFlagsTask->StartSubLoops(PointDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

		FPointsProcessor::Write();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

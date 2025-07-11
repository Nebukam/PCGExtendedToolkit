// Copyright 2025 Timothé Lapetite and contributors
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

PCGEX_INITIALIZE_ELEMENT(BevelPath)

void UPCGExBevelPathSettings::InitOutputFlags(const TSharedPtr<PCGExData::FPointIO>& InPointIO) const
{
	if (bFlagPoles) { InPointIO->FindOrCreateAttribute(PoleFlagName, false); }
	if (bFlagStartPoint) { InPointIO->FindOrCreateAttribute(StartPointFlagName, false); }
	if (bFlagEndPoint) { InPointIO->FindOrCreateAttribute(EndPointFlagName, false); }
	if (bFlagSubdivision) { InPointIO->FindOrCreateAttribute(SubdivisionFlagName, false); }
}

bool FPCGExBevelPathElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BevelPath)

	if (Settings->bFlagPoles) { PCGEX_VALIDATE_NAME(Settings->PoleFlagName) }
	if (Settings->bFlagStartPoint) { PCGEX_VALIDATE_NAME(Settings->StartPointFlagName) }
	if (Settings->bFlagEndPoint) { PCGEX_VALIDATE_NAME(Settings->EndPointFlagName) }
	if (Settings->bFlagSubdivision) { PCGEX_VALIDATE_NAME(Settings->SubdivisionFlagName) }

	if (Settings->Type == EPCGExBevelProfileType::Custom)
	{
		const TSharedPtr<PCGExData::FPointIO> CustomProfileIO = PCGExData::TryGetSingleInput(Context, PCGExBevelPath::SourceCustomProfile, false, true);
		if (!CustomProfileIO) { return false; }

		if (CustomProfileIO->GetNum() < 2)
		{
			PCGE_LOG(Error, GraphAndLog, FTEXT("Custom profile must have at least two points."));
			return false;
		}

		Context->CustomProfileFacade = MakeShared<PCGExData::FFacade>(CustomProfileIO.ToSharedRef());

		TConstPCGValueRange<FTransform> ProfileTransforms = CustomProfileIO->GetIn()->GetConstTransformValueRange();
		PCGEx::InitArray(Context->CustomProfilePositions, ProfileTransforms.Num());

		const FVector Start = ProfileTransforms[0].GetLocation();
		const FVector End = ProfileTransforms[ProfileTransforms.Num() - 1].GetLocation();
		const double Factor = 1 / FVector::Dist(Start, End);

		const FVector ProjectionNormal = (End - Start).GetSafeNormal(1E-08, FVector::ForwardVector);
		const FQuat ProjectionQuat = FQuat::FindBetweenNormals(ProjectionNormal, FVector::ForwardVector);

		for (int i = 0; i < ProfileTransforms.Num(); i++)
		{
			Context->CustomProfilePositions[i] = ProjectionQuat.RotateVector((ProfileTransforms[i].GetLocation() - Start) * Factor);
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
				PCGEX_SKIP_INVALID_PATH_ENTRY

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
				NewBatch->bRequiresWriteStep = (Settings->bFlagPoles || Settings->bFlagSubdivision || Settings->bFlagEndPoint || Settings->bFlagStartPoint);
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
		const UPCGBasePointData* InPoints = InProcessor->PointDataFacade->GetIn();
		TConstPCGValueRange<FTransform> InTransforms = InPoints->GetConstTransformValueRange();

		ArriveIdx = Index - 1 < 0 ? InTransforms.Num() - 1 : Index - 1;
		LeaveIdx = Index + 1 == InTransforms.Num() ? 0 : Index + 1;


		Corner = InTransforms[InIndex].GetLocation();
		PrevLocation = InTransforms[ArriveIdx].GetLocation();
		NextLocation = InTransforms[LeaveIdx].GetLocation();

		// Pre-compute some data

		ArriveDir = (PrevLocation - Corner).GetSafeNormal();
		LeaveDir = (NextLocation - Corner).GetSafeNormal();

		Width = InProcessor->WidthGetter->Read(Index);

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
		Length = PCGExMath::GetPerpendicularDistance(Arrive, Leave, Corner);

		// TODO : compute final subdivision count

		if (InProcessor->Settings->Type == EPCGExBevelProfileType::Custom)
		{
			SubdivideCustom(InProcessor);
			return;
		}

		if (!InProcessor->bSubdivide) { return; }
		
		if (InProcessor->ManhattanDetails.IsValid())
		{
			SubdivideManhattan(InProcessor);
			return;
		}

		const double Amount = InProcessor->SubdivAmountGetter->Read(Index);

		if (!InProcessor->bArc) { SubdivideLine(Amount, InProcessor->bSubdivideCount, InProcessor->bKeepCorner); }
		else { SubdivideArc(Amount, InProcessor->bSubdivideCount); }
	}

	void FBevel::SubdivideLine(const double Factor, const bool bIsCount, const bool bKeepCorner)
	{
		const double Dist = FVector::Dist(Arrive, bKeepCorner ? Corner : Leave);

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

		if (bKeepCorner)
		{
			PCGEx::InitArray(Subdivisions, SubdivCount * 2 + 1);

			if (SubdivCount == 0)
			{
				Subdivisions[0] = Corner;
			}
			else
			{
				int32 WriteIndex = 0;
				FVector Dir = (Corner - Arrive).GetSafeNormal();
				for (int i = 0; i < SubdivCount; i++) { Subdivisions[WriteIndex++] = Arrive + Dir * (StepSize + i * StepSize); }

				Subdivisions[WriteIndex++] = Corner;

				Dir = (Leave - Corner).GetSafeNormal();
				for (int i = 0; i < SubdivCount; i++) { Subdivisions[WriteIndex++] = Corner + Dir * (StepSize + i * StepSize); }
			}
		}
		else
		{
			PCGEx::InitArray(Subdivisions, SubdivCount);
			const FVector Dir = (Leave - Arrive).GetSafeNormal();
			for (int i = 0; i < SubdivCount; i++) { Subdivisions[i] = Arrive + Dir * (StepSize + i * StepSize); }
		}
	}

	void FBevel::SubdivideArc(const double Factor, const bool bIsCount)
	{
		const PCGExGeo::FExCenterArc Arc = PCGExGeo::FExCenterArc(Arrive, Corner, Leave);

		if (Arc.bIsLine)
		{
			// Fallback to line since we can't infer a proper radius
			SubdivideLine(Factor, bIsCount, false);
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

		const double ProfileSize = FVector::Dist(Leave, Arrive);
		const FVector ProjectionNormal = (Leave - Arrive).GetSafeNormal(1E-08, FVector::ForwardVector);
		const FQuat ProjectionQuat = FRotationMatrix::MakeFromZX(PCGExMath::GetNormal(Arrive, Leave, Corner) * -1, ProjectionNormal).ToQuat();

		double MainAxisSize = ProfileSize;
		double CrossAxisSize = ProfileSize;

		if (InProcessor->Settings->MainAxisScaling == EPCGExBevelCustomProfileScaling::Scale)
		{
			MainAxisSize = Length * CustomMainAxisScale;
		}
		else if (InProcessor->Settings->MainAxisScaling == EPCGExBevelCustomProfileScaling::Distance)
		{
			MainAxisSize = CustomMainAxisScale;
		}

		if (InProcessor->Settings->CrossAxisScaling == EPCGExBevelCustomProfileScaling::Scale)
		{
			MainAxisSize = Length * CustomCrossAxisScale;
		}
		else if (InProcessor->Settings->CrossAxisScaling == EPCGExBevelCustomProfileScaling::Distance)
		{
			MainAxisSize = CustomCrossAxisScale;
		}

		for (int i = 0; i < SubdivCount; i++)
		{
			FVector Pos = SourcePos[i + 1];
			Pos.X *= ProfileSize;
			Pos.Y *= MainAxisSize;
			Pos.Z *= CrossAxisSize;
			Subdivisions[i] = Arrive + ProjectionQuat.RotateVector(Pos);
		}
	}

	void FBevel::SubdivideManhattan(const FProcessor* InProcessor)
	{
		double OutDist = 0;
		
		if (InProcessor->bKeepCorner)
		{
			InProcessor->ManhattanDetails.ComputeSubdivisions(Arrive, Corner, Index, Subdivisions, OutDist);
			Subdivisions.Emplace(Corner);
			InProcessor->ManhattanDetails.ComputeSubdivisions(Corner, Leave, Index, Subdivisions, OutDist);
		}
		else
		{
			InProcessor->ManhattanDetails.ComputeSubdivisions(Arrive, Leave, Index, Subdivisions, OutDist);
		}
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBevelPath::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IPointsProcessor::Process(InAsyncManager)) { return false; }

		const UPCGBasePointData* InPoints = PointDataFacade->GetIn();

		Path = PCGExPaths::MakePath(InPoints, 0);
		PathLength = Path->AddExtra<PCGExPaths::FPathEdgeLength>();

		if (Settings->Type == EPCGExBevelProfileType::Custom)
		{
			/*
			switch (Settings->ProfileNormal)
			{
			case EPCGExPathNormalDirection::Normal:
				PathDirection = StaticCastSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>>(Path->AddExtra<PCGExPaths::FPathEdgeNormal>(false, FVector::UpVector));
				break;
			case EPCGExPathNormalDirection::Binormal:
				PathDirection = StaticCastSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>>(Path->AddExtra<PCGExPaths::FPathEdgeBinormal>(false, FVector::UpVector));
				break;
			case EPCGExPathNormalDirection::AverageNormal:
				PathDirection = StaticCastSharedPtr<PCGExPaths::TPathEdgeExtra<FVector>>(Path->AddExtra<PCGExPaths::FPathEdgeAvgNormal>(false, FVector::UpVector));
				break;
			}
			*/
		}

		Path->ComputeAllEdgeExtra();

		bDaisyChainProcessPoints = true;

		Bevels.Init(nullptr, PointDataFacade->GetNum());

		WidthGetter = Settings->GetValueSettingWidth();
		if (!WidthGetter->Init(Context, PointDataFacade)) { return false; }

		bKeepCorner = Settings->bKeepCornerPoint;

		if (Settings->bSubdivide)
		{
			bSubdivide = Settings->Type != EPCGExBevelProfileType::Custom;
			if (bSubdivide)
			{
				bSubdivideCount = Settings->SubdivideMethod != EPCGExSubdivideMode::Distance;
				if (Settings->SubdivideMethod != EPCGExSubdivideMode::Manhattan)
				{
					SubdivAmountGetter = Settings->GetValueSettingSubdivisions();
					if (!SubdivAmountGetter->Init(Context, PointDataFacade)) { return false; }
				}
			}
		}
		if (bKeepCorner && Settings->Type == EPCGExBevelProfileType::Line)
		{
			// This is to force line to go through subdiv flow
			bSubdivide = true;
			bSubdivideCount = true;
			SubdivAmountGetter = PCGExDetails::MakeSettingValue<double>(0);
		}

		if (Settings->SubdivideMethod == EPCGExSubdivideMode::Manhattan)
		{
			ManhattanDetails = Settings->ManhattanDetails;
			if (!ManhattanDetails.Init(Context, PointDataFacade)) { return false; }
		}
		
		bArc = Settings->Type == EPCGExBevelProfileType::Arc;

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, Preparation)

		Preparation->OnCompleteCallback =
			[PCGEX_ASYNC_THIS_CAPTURE]()
			{
				PCGEX_ASYNC_THIS
				if (!This->Path->IsClosedLoop())
				{
					// Ensure bevel is disabled on start/end points
					This->PointFilterCache[0] = false;
					This->PointFilterCache[This->PointFilterCache.Num() - 1] = false;
				}

				This->StartParallelLoopForPoints(PCGExData::EIOSide::In);
			};

		Preparation->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS

				This->PointDataFacade->Fetch(Scope);
				This->FilterScope(Scope);

				if (!This->Path->IsClosedLoop())
				{
					// Ensure bevel is disabled on start/end points
					This->PointFilterCache[0] = false;
					This->PointFilterCache[This->PointFilterCache.Num() - 1] = false;
				}

				PCGEX_SCOPE_LOOP(i) { This->PrepareSinglePoint(i); }
			};

		Preparation->StartSubLoops(PointDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->PointsDefaultBatchChunkSize);

		return true;
	}

	void FProcessor::PrepareSinglePoint(const int32 Index)
	{
		if (!PointFilterCache[Index]) { return; }

		Bevels[Index] = MakeShared<FBevel>(Index, this);
		Bevels[Index]->CustomMainAxisScale = Settings->MainAxisScale;
		Bevels[Index]->CustomCrossAxisScale = Settings->CrossAxisScale;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::BevelPath::ProcessPoints);

		PCGEX_SCOPE_LOOP(Index)
		{
			const TSharedPtr<FBevel>& Bevel = Bevels[Index];
			if (!Bevel) { continue; }

			Bevel->Compute(this);
		}
	}

	void FProcessor::ProcessRange(const PCGExMT::FScope& Scope)
	{
		const UPCGBasePointData* InPointData = PointDataFacade->GetIn();
		UPCGBasePointData* OutPointData = PointDataFacade->GetOut();
		UPCGMetadata* Metadata = OutPointData->Metadata;

		// Only pin properties we will not be inheriting
		TConstPCGValueRange<FTransform> InTransform = InPointData->GetConstTransformValueRange();
		TConstPCGValueRange<int64> InMetadataEntry = InPointData->GetConstMetadataEntryValueRange();

		TPCGValueRange<FTransform> OutTransform = OutPointData->GetTransformValueRange(false);
		TPCGValueRange<int64> OutMetadataEntry = OutPointData->GetMetadataEntryValueRange(false);
		TPCGValueRange<int32> OutSeeds = OutPointData->GetSeedValueRange(false);

		TArray<int32>& IdxMapping = PointDataFacade->Source->GetIdxMapping();

		PCGEX_SCOPE_LOOP(Index)
		{
			const int32 StartIndex = StartIndices[Index];
			const TSharedPtr<FBevel>& Bevel = Bevels[Index];

			if (!Bevel)
			{
				IdxMapping[StartIndex] = Index;
				OutTransform[StartIndex] = InTransform[Index];
				OutMetadataEntry[StartIndex] = InMetadataEntry[Index];
				Metadata->InitializeOnSet(OutMetadataEntry[StartIndex]);
				continue;
			}

			const int32 A = Bevel->StartOutputIndex;
			const int32 B = Bevel->EndOutputIndex;

			for (int i = A; i <= B; i++)
			{
				IdxMapping[i] = Index;
				OutTransform[i] = InTransform[Index];
				OutMetadataEntry[i] = InMetadataEntry[Index];
				Metadata->InitializeOnSet(OutMetadataEntry[i]);
			}

			OutTransform[A].SetLocation(Bevel->Arrive);
			OutTransform[B].SetLocation(Bevel->Leave);

			OutSeeds[A] = PCGExRandom::ComputeSpatialSeed(OutTransform[A].GetLocation());
			OutSeeds[B] = PCGExRandom::ComputeSpatialSeed(OutTransform[B].GetLocation());

			if (Bevel->Subdivisions.IsEmpty()) { continue; }

			for (int i = 0; i < Bevel->Subdivisions.Num(); i++)
			{
				const int32 SubIndex = A + i + 1;
				OutTransform[SubIndex].SetLocation(Bevel->Subdivisions[i]);
				OutSeeds[SubIndex] = PCGExRandom::ComputeSpatialSeed(OutTransform[SubIndex].GetLocation());
			}
		}
	}

	void FProcessor::OnRangeProcessingComplete()
	{
		constexpr EPCGPointNativeProperties CarryOverProperties =
			static_cast<EPCGPointNativeProperties>(static_cast<uint8>(EPCGPointNativeProperties::All) &
				~static_cast<uint8>(EPCGPointNativeProperties::Transform | EPCGPointNativeProperties::Seed | EPCGPointNativeProperties::MetadataEntry));

		PointDataFacade->Source->ConsumeIdxMapping(CarryOverProperties);
	}

	void FProcessor::WriteFlags(const int32 Index)
	{
		const TSharedPtr<FBevel>& Bevel = Bevels[Index];
		if (!Bevel) { return; }

		if (EndpointsWriter)
		{
			EndpointsWriter->SetValue(Bevel->StartOutputIndex, true);
			EndpointsWriter->SetValue(Bevel->EndOutputIndex, true);
		}

		if (StartPointWriter) { StartPointWriter->SetValue(Bevel->StartOutputIndex, true); }

		if (EndPointWriter) { EndPointWriter->SetValue(Bevel->EndOutputIndex, true); }

		if (SubdivisionWriter) { for (int i = 1; i <= Bevel->Subdivisions.Num(); i++) { SubdivisionWriter->SetValue(Bevel->StartOutputIndex + i, true); } }
	}

	void FProcessor::CompleteWork()
	{
		PCGEx::InitArray(StartIndices, PointDataFacade->GetNum());

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		int32 NumBevels = 0;
		int32 NumOutPoints = 0;

		TArray<int32> ReadIndices;
		ReadIndices.Reserve(NumOutPoints * 4);

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
			PCGEX_INIT_IO_VOID(PointIO, PCGExData::EIOInit::Duplicate)
			Settings->InitOutputFlags(PointIO);
			return;
		}

		PCGEX_INIT_IO_VOID(PointIO, PCGExData::EIOInit::New)
		Settings->InitOutputFlags(PointIO);

		// Build output points

		UPCGBasePointData* MutablePoints = PointDataFacade->GetOut();
		PCGEx::SetNumPointsAllocated(MutablePoints, NumOutPoints, PointDataFacade->GetAllocations());

		StartParallelLoopForRange(PointDataFacade->GetNum());
	}

	void FProcessor::Write()
	{
		if (Settings->bFlagPoles)
		{
			EndpointsWriter = PointDataFacade->GetWritable<bool>(Settings->PoleFlagName, false, true, PCGExData::EBufferInit::New);
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
				This->PointDataFacade->WriteFastest(This->AsyncManager);
			};

		WriteFlagsTask->OnSubLoopStartCallback =
			[PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS
				PCGEX_SCOPE_LOOP(i)
				{
					if (!This->PointFilterCache[i]) { continue; }
					This->WriteFlags(i);
				}
			};

		WriteFlagsTask->StartSubLoops(PointDataFacade->GetNum(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

		IPointsProcessor::Write();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

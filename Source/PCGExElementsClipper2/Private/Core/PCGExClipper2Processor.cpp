// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExClipper2Processor.h"

#include "Core/PCGExClipper2Common.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"
#include "Helpers/PCGExAsyncHelpers.h"
#include "Helpers/PCGExDataMatcher.h"
#include "Helpers/PCGExMatchingHelpers.h"
#include "Math/PCGExBestFitPlane.h"
#include "Paths/PCGExPath.h"
#include "Paths/PCGExPathsHelpers.h"
#include "Blenders/PCGExUnionBlender.h"
#include "Clipper2Lib/clipper.h"
#include "Containers/PCGExIndexLookup.h"
#include "Core/PCGExUnionData.h"
#include "Math/PCGExMathDistances.h"

#define LOCTEXT_NAMESPACE "PCGExClipper2ProcessorElement"

namespace PCGExClipper2
{
#pragma region FOpData

	FOpData::FOpData(const int32 InReserve)
	{
		AddReserve(InReserve);
	}

	void FOpData::AddReserve(const int32 InReserve)
	{
		const int32 Reserve = Paths.Num() + InReserve;
		Facades.Reserve(Reserve);
		Paths.Reserve(Reserve);
		IsClosedLoop.Reserve(Reserve);
		Projections.Reserve(Reserve);
	}

	void FOpData::RegisterFacade(const TSharedPtr<PCGExData::FFacade>& Facade, int32 ArrayIndex)
	{
		FacadeIdxToArrayIndex.Add(Facade->Idx, ArrayIndex);
	}

#pragma endregion

#pragma region FProcessingGroup

	void FProcessingGroup::Prepare(const TSharedPtr<FOpData>& AllOpData)
	{
		// Cache subject paths
		SubjectPaths.reserve(SubjectIndices.Num());
		for (const int32 Idx : SubjectIndices)
		{
			if (Idx < AllOpData->Paths.Num()) { SubjectPaths.push_back(AllOpData->Paths[Idx]); }
		}

		// Cache operand paths
		OperandPaths.reserve(OperandIndices.Num());
		for (const int32 Idx : OperandIndices)
		{
			if (Idx < AllOpData->Paths.Num()) { OperandPaths.push_back(AllOpData->Paths[Idx]); }
		}

		// Build combined source indices
		AllSourceIndices.Reserve(SubjectIndices.Num() + OperandIndices.Num());
		AllSourceIndices.Append(SubjectIndices);
		AllSourceIndices.Append(OperandIndices);
	}

	void FProcessingGroup::AddIntersectionBlendInfo(int64_t X, int64_t Y, const FIntersectionBlendInfo& Info)
	{
		const uint64 Key = PCGEx::H64(static_cast<uint32>(X & 0xFFFFFFFF), static_cast<uint32>(Y & 0xFFFFFFFF));
		FScopeLock Lock(&IntersectionLock);
		IntersectionBlendInfos.Add(Key, Info);
	}

	const FIntersectionBlendInfo* FProcessingGroup::GetIntersectionBlendInfo(int64_t X, int64_t Y) const
	{
		const uint64 Key = PCGEx::H64(static_cast<uint32>(X & 0xFFFFFFFF), static_cast<uint32>(Y & 0xFFFFFFFF));
		return IntersectionBlendInfos.Find(Key);
	}

	PCGExClipper2Lib::ZCallback64 FProcessingGroup::CreateZCallback()
	{
		TWeakPtr<FProcessingGroup> WeakSelf = AsWeak();

		return [WeakSelf](
			const PCGExClipper2Lib::Point64& e1bot, const PCGExClipper2Lib::Point64& e1top,
			const PCGExClipper2Lib::Point64& e2bot, const PCGExClipper2Lib::Point64& e2top,
			PCGExClipper2Lib::Point64& pt)
		{
			TSharedPtr<FProcessingGroup> Group = WeakSelf.Pin();
			if (!Group) { return; }

			// Decode the source info from each vertex
			uint32 E1BotPtIdx, E1BotSrcIdx;
			uint32 E1TopPtIdx, E1TopSrcIdx;
			uint32 E2BotPtIdx, E2BotSrcIdx;
			uint32 E2TopPtIdx, E2TopSrcIdx;

			PCGEx::H64(static_cast<uint64>(e1bot.z), E1BotPtIdx, E1BotSrcIdx);
			PCGEx::H64(static_cast<uint64>(e1top.z), E1TopPtIdx, E1TopSrcIdx);
			PCGEx::H64(static_cast<uint64>(e2bot.z), E2BotPtIdx, E2BotSrcIdx);
			PCGEx::H64(static_cast<uint64>(e2top.z), E2TopPtIdx, E2TopSrcIdx);

			// Calculate alpha along each edge
			auto CalcAlpha = [](const PCGExClipper2Lib::Point64& Bot, const PCGExClipper2Lib::Point64& Top, const PCGExClipper2Lib::Point64& Pt) -> double
			{
				const double DX = static_cast<double>(Top.x - Bot.x);
				const double DY = static_cast<double>(Top.y - Bot.y);
				const double Len = FMath::Sqrt(DX * DX + DY * DY);
				if (Len < SMALL_NUMBER) { return 0.5; }

				const double PtDX = static_cast<double>(Pt.x - Bot.x);
				const double PtDY = static_cast<double>(Pt.y - Bot.y);
				return FMath::Clamp((PtDX * DX + PtDY * DY) / (Len * Len), 0.0, 1.0);
			};

			FIntersectionBlendInfo Info;
			Info.E1BotPointIdx = E1BotPtIdx;
			Info.E1BotSourceIdx = E1BotSrcIdx;
			Info.E1TopPointIdx = E1TopPtIdx;
			Info.E1TopSourceIdx = E1TopSrcIdx;
			Info.E2BotPointIdx = E2BotPtIdx;
			Info.E2BotSourceIdx = E2BotSrcIdx;
			Info.E2TopPointIdx = E2TopPtIdx;
			Info.E2TopSourceIdx = E2TopSrcIdx;
			Info.E1Alpha = CalcAlpha(e1bot, e1top, pt);
			Info.E2Alpha = CalcAlpha(e2bot, e2top, pt);

			// Store intersection info
			Group->AddIntersectionBlendInfo(pt.x, pt.y, Info);

			// Encode intersection marker in Z - use a special pattern
			// We mark it as an intersection point; the actual blend info is stored in the map
			pt.z = static_cast<int64_t>(PCGEx::H64(INTERSECTION_MARKER, INTERSECTION_MARKER));
		};
	}

#pragma endregion
}

UPCGExClipper2ProcessorSettings::UPCGExClipper2ProcessorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UPCGExClipper2ProcessorSettings::IsPinUsedByNodeExecution(const UPCGPin* InPin) const
{
	if (InPin->Properties.Label == PCGExClipper2::Labels::SourceOperandsLabel) { return NeedsOperands(); }
	return Super::IsPinUsedByNodeExecution(InPin);
}

TArray<FPCGPinProperties> UPCGExClipper2ProcessorSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	PCGExMatching::Helpers::DeclareMatchingRulesInputs(MainDataMatching, PinProperties);

	if (NeedsOperands())
	{
		PCGEX_PIN_POINTS(PCGExClipper2::Labels::SourceOperandsLabel, "Operands", Required)
		PCGExMatching::Helpers::DeclareMatchingRulesInputs(OperandsDataMatching, PinProperties, PCGExClipper2::Labels::SourceOperandsMatchRulesLabel);
	}
	else
	{
		PCGEX_PIN_POINTS(PCGExClipper2::Labels::SourceOperandsLabel, "Operands", Advanced)

		FPCGExMatchingDetails OperandsDataMatchingCopy = OperandsDataMatching;
		OperandsDataMatchingCopy.Mode = EPCGExMapMatchMode::Disabled;
		PCGExMatching::Helpers::DeclareMatchingRulesInputs(OperandsDataMatchingCopy, PinProperties, PCGExClipper2::Labels::SourceOperandsMatchRulesLabel);
	}

	return PinProperties;
}

bool UPCGExClipper2ProcessorSettings::NeedsOperands() const
{
	return false;
}

FPCGExGeo2DProjectionDetails UPCGExClipper2ProcessorSettings::GetProjectionDetails() const
{
	return FPCGExGeo2DProjectionDetails();
}

PCGExClipper2::EMainGroupingPolicy UPCGExClipper2ProcessorSettings::GetGroupingPolicy() const
{
	return PCGExClipper2::EMainGroupingPolicy::Split;
}

void FPCGExClipper2ProcessorContext::OutputPaths64(
	PCGExClipper2Lib::Paths64& InPaths,
	const TSharedPtr<PCGExClipper2::FProcessingGroup>& Group,
	const FPCGExBlendingDetails* InBlendingDetails,
	const FPCGExCarryOverDetails* InCarryOverDetails,
	TArray<TSharedPtr<PCGExData::FPointIO>>& OutPaths)
{
	const UPCGExClipper2ProcessorSettings* Settings = GetInputSettings<UPCGExClipper2ProcessorSettings>();

	if (InPaths.empty()) { return; }

	// Build index lookup and sources list for blending
	TArray<TSharedRef<PCGExData::FFacade>> BlendSources;
	BlendSources.Reserve(Group->AllSourceIndices.Num());

	EPCGPointNativeProperties Allocations = EPCGPointNativeProperties::None;

	int32 MaxIOIndex = 0;
	for (const int32 SrcIdx : Group->AllSourceIndices)
	{
		if (SrcIdx < AllOpData->Facades.Num())
		{
			const TSharedPtr<PCGExData::FFacade>& Facade = AllOpData->Facades[SrcIdx];
			Allocations |= Facade->GetAllocations();
			BlendSources.Add(Facade.ToSharedRef());
			MaxIOIndex = FMath::Max(MaxIOIndex, Facade->Idx);
		}
	}

	// Create IO index lookup
	TSharedPtr<PCGEx::FIndexLookup> IOLookup = MakeShared<PCGEx::FIndexLookup>(MaxIOIndex + 1);
	for (int32 i = 0; i < BlendSources.Num(); i++) { IOLookup->Set(BlendSources[i]->Idx, i); }

	// Process each output path
	OutPaths.Reserve(InPaths.size());

	for (PCGExClipper2Lib::Path64& Path : InPaths)
	{
		if (Path.size() < 2) { continue; }

		// Simplify if requested
		if (Settings->bSimplifyPaths)
		{
			Path = PCGExClipper2Lib::SimplifyPath(Path, Settings->Precision * 0.5, true);
			if (Path.size() < 2) { continue; }
		}

		// Determine if this is a hole (counter-clockwise winding)
		const bool bIsHole = !PCGExClipper2Lib::IsPositive(Path);

		// Find the dominant source for this path (most points from same source)
		TMap<int32, int32> SourceCounts;
		for (const PCGExClipper2Lib::Point64& Pt : Path)
		{
			uint32 PointIdx, SourceIdx;
			PCGEx::H64(static_cast<uint64>(Pt.z), PointIdx, SourceIdx);

			// Skip intersection markers
			if (PointIdx == PCGExClipper2::INTERSECTION_MARKER) { continue; }

			const int32 LocalIdx = AllOpData->FindSourceIndex(SourceIdx);
			if (LocalIdx != INDEX_NONE) { SourceCounts.FindOrAdd(LocalIdx)++; }
		}

		int32 DominantSourceIdx = Group->AllSourceIndices.IsEmpty() ? INDEX_NONE : Group->AllSourceIndices[0];
		int32 MaxCount = 0;
		for (const auto& Pair : SourceCounts)
		{
			if (Pair.Value > MaxCount)
			{
				MaxCount = Pair.Value;
				DominantSourceIdx = Pair.Key;
			}
		}

		// Create new output point data from template
		TSharedPtr<PCGExData::FPointIO> NewPointIO;

		if (DominantSourceIdx != INDEX_NONE && DominantSourceIdx < AllOpData->Facades.Num())
		{
			const TSharedPtr<PCGExData::FFacade>& TemplateFacade = AllOpData->Facades[DominantSourceIdx];
			NewPointIO = MainPoints->Emplace_GetRef(TemplateFacade->Source, PCGExData::EIOInit::New);
		}

		if (!NewPointIO)
		{
			NewPointIO = MainPoints->Emplace_GetRef(PCGExData::EIOInit::New);
		}

		const int32 NumPoints = static_cast<int32>(Path.size());
		UPCGBasePointData* OutPoints = NewPointIO->GetOut();
		PCGExPointArrayDataHelpers::SetNumPointsAllocated(OutPoints, NumPoints, Allocations);

		TPCGValueRange<FTransform> OutTransforms = OutPoints->GetTransformValueRange(false);

		// Check if we need blending
		const bool bNeedsBlending = InBlendingDetails && SourceCounts.Num() > 1;

		TSharedPtr<PCGExData::FFacade> OutputFacade;
		TSharedPtr<PCGExBlending::FUnionBlender> Blender;
		TSharedPtr<PCGExData::FUnionMetadata> UnionMetadata;

		if (bNeedsBlending)
		{
			OutputFacade = MakeShared<PCGExData::FFacade>(NewPointIO.ToSharedRef());

			Blender = MakeShared<PCGExBlending::FUnionBlender>(InBlendingDetails, InCarryOverDetails, PCGExMath::GetDistances());
			Blender->AddSources(BlendSources, nullptr, [](const TSharedPtr<PCGExData::FFacade>& InFacade) { return InFacade->Idx; });

			UnionMetadata = MakeShared<PCGExData::FUnionMetadata>();
			UnionMetadata->SetNum(NumPoints);

			if (!Blender->Init(this, OutputFacade, UnionMetadata, false))
			{
				Blender.Reset();
			}
		}

		// Process each point in the path
		for (int32 i = 0; i < NumPoints; i++)
		{
			const PCGExClipper2Lib::Point64& Pt = Path[i];
			FTransform& OutTransform = OutTransforms[i];

			// Decode source info from Z
			uint32 OriginalPointIdx, SourceFacadeIdx;
			PCGEx::H64(static_cast<uint64>(Pt.z), OriginalPointIdx, SourceFacadeIdx);

			const bool bIsIntersection = (OriginalPointIdx == PCGExClipper2::INTERSECTION_MARKER);

			if (bIsIntersection)
			{
				// This is an intersection point - get blend info and interpolate transform
				const PCGExClipper2::FIntersectionBlendInfo* BlendInfo = Group->GetIntersectionBlendInfo(Pt.x, Pt.y);

				if (BlendInfo)
				{
					// Get the 4 source transforms
					auto GetSourceTransform = [this](uint32 PtIdx, uint32 SrcIdx, FTransform& OutT) -> bool
					{
						const int32 LocalIdx = AllOpData->FindSourceIndex(SrcIdx);
						if (LocalIdx == INDEX_NONE || LocalIdx >= AllOpData->Facades.Num()) { return false; }

						const TSharedPtr<PCGExData::FFacade>& SrcFacade = AllOpData->Facades[LocalIdx];
						const int32 NumPts = SrcFacade->Source->GetNum(PCGExData::EIOSide::In);
						if (static_cast<int32>(PtIdx) >= NumPts) { return false; }

						TConstPCGValueRange<FTransform> SrcTransforms = SrcFacade->Source->GetIn()->GetConstTransformValueRange();
						OutT = SrcTransforms[PtIdx];
						return true;
					};

					FTransform E1Bot, E1Top, E2Bot, E2Top;
					bool bHasE1Bot = GetSourceTransform(BlendInfo->E1BotPointIdx, BlendInfo->E1BotSourceIdx, E1Bot);
					bool bHasE1Top = GetSourceTransform(BlendInfo->E1TopPointIdx, BlendInfo->E1TopSourceIdx, E1Top);
					bool bHasE2Bot = GetSourceTransform(BlendInfo->E2BotPointIdx, BlendInfo->E2BotSourceIdx, E2Bot);
					bool bHasE2Top = GetSourceTransform(BlendInfo->E2TopPointIdx, BlendInfo->E2TopSourceIdx, E2Top);

					// Interpolate along each edge
					FTransform E1Interp = E1Bot;
					if (bHasE1Bot && bHasE1Top) { E1Interp.Blend(E1Bot, E1Top, BlendInfo->E1Alpha); }

					FTransform E2Interp = E2Bot;
					if (bHasE2Bot && bHasE2Top) { E2Interp.Blend(E2Bot, E2Top, BlendInfo->E2Alpha); }

					// Average the two edge interpolations
					OutTransform.Blend(E1Interp, E2Interp, 0.5);

					// Add all 4 vertices to union for metadata blending
					if (Blender && UnionMetadata)
					{
						TSharedPtr<PCGExData::IUnionData> Union = UnionMetadata->NewEntryAt_Unsafe(i);

						auto AddToUnion = [&](uint32 PtIdx, uint32 SrcIdx)
						{
							const int32 LocalIdx = AllOpData->FindSourceIndex(SrcIdx);

							if (LocalIdx == INDEX_NONE || LocalIdx >= BlendSources.Num()) { return; }

							const TSharedRef<PCGExData::FFacade> SourceFacade = BlendSources[LocalIdx];
							const int32 IOIdx = SourceFacade->Idx;
							const int32 NumPts = SourceFacade->Source->GetNum(PCGExData::EIOSide::In);

							if (static_cast<int32>(PtIdx) >= NumPts) { return; }

							Union->Add_Unsafe(static_cast<int32>(PtIdx), IOIdx);
						};

						AddToUnion(BlendInfo->E1BotPointIdx, BlendInfo->E1BotSourceIdx);
						AddToUnion(BlendInfo->E1TopPointIdx, BlendInfo->E1TopSourceIdx);
						AddToUnion(BlendInfo->E2BotPointIdx, BlendInfo->E2BotSourceIdx);
						AddToUnion(BlendInfo->E2TopPointIdx, BlendInfo->E2TopSourceIdx);
					}
				}
			}
			else
			{
				// Regular point - get transform directly from source
				const int32 SourceLocalIdx = AllOpData->FindSourceIndex(SourceFacadeIdx);

				if (SourceLocalIdx != INDEX_NONE && SourceLocalIdx < AllOpData->Facades.Num())
				{
					const TSharedPtr<PCGExData::FFacade>& SrcFacade = AllOpData->Facades[SourceLocalIdx];
					const int32 SrcNumPoints = SrcFacade->Source->GetNum(PCGExData::EIOSide::In);

					if (static_cast<int32>(OriginalPointIdx) < SrcNumPoints)
					{
						TConstPCGValueRange<FTransform> SrcTransforms = SrcFacade->Source->GetIn()->GetConstTransformValueRange();
						OutTransform = SrcTransforms[OriginalPointIdx];
					}
				}

				// Add to union for blending
				if (Blender && UnionMetadata)
				{
					TSharedPtr<PCGExData::IUnionData> Union = UnionMetadata->NewEntryAt_Unsafe(i);

					if (SourceLocalIdx != INDEX_NONE && SourceLocalIdx < BlendSources.Num())
					{
						const int32 IOIdx = IOLookup->Get(BlendSources[SourceLocalIdx]->Idx);
						if (IOIdx != INDEX_NONE)
						{
							const int32 SrcNumPts = BlendSources[SourceLocalIdx]->Source->GetNum(PCGExData::EIOSide::In);
							const int32 Pt1 = FMath::Clamp(static_cast<int32>(OriginalPointIdx), 0, SrcNumPts - 1);
							Union->Add_Unsafe(Pt1, IOIdx);
						}
					}
				}
			}
		}

		// Perform blending
		if (Blender && UnionMetadata)
		{
			TArray<PCGExData::FWeightedPoint> WeightedPoints;
			TArray<PCGEx::FOpStats> Trackers;
			Blender->InitTrackers(Trackers);

			for (int32 i = 0; i < NumPoints; i++)
			{
				WeightedPoints.Reset();
				Blender->MergeSingle(i, WeightedPoints, Trackers);
			}

			OutputFacade->WriteFastest(GetTaskManager());
		}

		// Tag as hole if applicable
		if (bIsHole && Settings->bTagHoles) { NewPointIO->Tags->AddRaw(Settings->HoleTag); }

		// Mark as closed loop
		PCGExPaths::Helpers::SetClosedLoop(NewPointIO, true);

		OutPaths.Add(NewPointIO);
	}
}

void FPCGExClipper2ProcessorContext::Process(const TSharedPtr<PCGExClipper2::FProcessingGroup>& Group)
{
	// Base implementation does nothing - derived classes override this
}


bool FPCGExClipper2ProcessorElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(Clipper2Processor)

	// Initialize AllOpData early
	Context->AllOpData = MakeShared<PCGExClipper2::FOpData>(0);

	// Initialize projection
	Context->ProjectionDetails = Settings->GetProjectionDetails();

	// Build main data
	TArray<int32> MainIndices;
	int32 NumInputs = BuildDataFromCollection(Context, Settings, Context->MainPoints, MainIndices);

	if (NumInputs != Context->MainPoints->Num())
	{
		PCGEX_LOG_INVALID_INPUT(Context, FTEXT("Some inputs have less than 2 points and won't be processed."))
	}

	if (!NumInputs)
	{
		PCGE_LOG(Warning, GraphAndLog, FTEXT("No valid paths found in main input."));
		return false;
	}

	// Build operand data if needed
	TArray<int32> OperandIndices;
	if (Settings->NeedsOperands())
	{
		Context->OperandsCollection = MakeShared<PCGExData::FPointIOCollection>(InContext, PCGExClipper2::Labels::SourceOperandsLabel, PCGExData::EIOInit::NoInit, false);

		if (Context->OperandsCollection->IsEmpty())
		{
			PCGEX_LOG_MISSING_INPUT(Context, FTEXT("Operands input is required for this operation mode."));
			return false;
		}

		NumInputs = BuildDataFromCollection(Context, Settings, Context->OperandsCollection, OperandIndices);

		if (NumInputs != Context->OperandsCollection->Num())
		{
			PCGEX_LOG_INVALID_INPUT(Context, FTEXT("Some operands have less than 2 points and won't be processed."))
		}

		if (!NumInputs)
		{
			PCGE_LOG(Warning, GraphAndLog, FTEXT("No valid operands found in operands input."));
			return false;
		}
	}

	// Build processing groups
	BuildProcessingGroups(Context, Settings, MainIndices, OperandIndices);

	if (Context->ProcessingGroups.IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog, FTEXT("No valid processing groups could be formed."));
		return false;
	}

	return true;
}

bool FPCGExClipper2ProcessorElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExClipper2ProcessorElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(Clipper2Processor)

	PCGEX_EXECUTION_CHECK

	PCGEX_ON_INITIAL_EXECUTION
	{
		Context->SetState(PCGExCommon::States::State_Processing);
		PCGEX_ASYNC_GROUP_CHKD_RET(Context->GetTaskManager(), WorkTasks, true)

		TWeakPtr<FPCGContextHandle> WeakHandle = Context->GetOrCreateHandle();

		for (int32 i = 0; i < Context->ProcessingGroups.Num(); i++)
		{
			WorkTasks->AddSimpleCallback([WeakHandle, Index = i]
			{
				FPCGContext::FSharedContext<FPCGExClipper2ProcessorContext> SharedContext(WeakHandle);
				if (!SharedContext.Get()) { return; }

				SharedContext.Get()->Process(SharedContext.Get()->ProcessingGroups[Index]);
			});
		}

		WorkTasks->StartSimpleCallbacks();
	}

	PCGEX_ON_ASYNC_STATE_READY(PCGExCommon::States::State_Processing)
	{
		PCGEX_OUTPUT_VALID_PATHS(MainPoints)
		Context->Done();
	}

	return Context->TryComplete();
}

int32 FPCGExClipper2ProcessorElement::BuildDataFromCollection(
	FPCGExClipper2ProcessorContext* Context,
	const UPCGExClipper2ProcessorSettings* Settings,
	const TSharedPtr<PCGExData::FPointIOCollection>& Collection,
	TArray<int32>& OutIndices) const
{
	if (!Collection) { return 0; }

	const int32 NumInputs = Collection->Num();
	if (NumInputs == 0) { return 0; }

	struct FBuildResult
	{
		bool bValid = false;
		PCGExClipper2Lib::Path64 Path64;
		TSharedPtr<PCGExData::FFacade> Facade;
		bool bIsClosedLoop = false;
		FPCGExGeo2DProjectionDetails Projection;
	};

	TArray<FBuildResult> Results;
	Results.SetNum(NumInputs);

	Context->AllOpData->AddReserve(NumInputs);

	{
		PCGExAsyncHelpers::FAsyncExecutionScope BuildScope(NumInputs);

		for (int32 i = 0; i < NumInputs; ++i)
		{
			BuildScope.Execute([&, IO = (*Collection)[i], i]()
			{
				FBuildResult& Result = Results[i];

				// Skip paths with insufficient points
				if (IO->GetNum() < 2) { return; }

				// Check if closed (required for boolean ops)
				const bool bIsClosed = PCGExPaths::Helpers::GetClosedLoop(IO->GetIn());
				if (!bIsClosed && Settings->bSkipOpenPaths) return;

				TSharedPtr<PCGExData::FFacade> Facade = MakeShared<PCGExData::FFacade>(IO.ToSharedRef());

				// Allocate unique path ID
				const int32 Idx = Context->AllocateSourceIdx();
				Facade->Idx = Idx;

				// Initialize projection for this path
				FPCGExGeo2DProjectionDetails LocalProjection = Context->ProjectionDetails;
				if (LocalProjection.Method == EPCGExProjectionMethod::Normal) { if (!LocalProjection.Init(Facade)) { return; } }
				else { LocalProjection.Init(PCGExMath::FBestFitPlane(IO->GetIn()->GetConstTransformValueRange())); }

				const int32 Scale = Settings->Precision;

				TConstPCGValueRange<FTransform> InTransforms = IO->GetIn()->GetConstTransformValueRange();

				// Build path with Z value mapped to source ID
				const int32 NumPoints = InTransforms.Num();
				Result.Path64.reserve(NumPoints);
				for (int32 j = 0; j < NumPoints; j++)
				{
					FVector Location = LocalProjection.Project(InTransforms[j].GetLocation(), j);
					Result.Path64.emplace_back(
						static_cast<int64_t>(Location.X * Scale),
						static_cast<int64_t>(Location.Y * Scale),
						static_cast<int64_t>(PCGEx::H64(j, Idx)) // retrieve using H64(const uint64 Hash, uint32& A, uint32& B)
					);
				}

				Result.bValid = true;
				Result.Facade = Facade;
				Result.Projection = LocalProjection;
				Result.bIsClosedLoop = bIsClosed;
			});
		}
	}

	int32 TotalDataNum = 0;
	OutIndices.Reserve(Results.Num());

	// Collect results (single-threaded)
	for (FBuildResult& Result : Results)
	{
		if (!Result.bValid) { continue; }

		const int32 ArrayIndex = Context->AllOpData->Facades.Num();
		OutIndices.Add(ArrayIndex);

		Context->AllOpData->Facades.Add(Result.Facade);
		Context->AllOpData->Paths.Add(MoveTemp(Result.Path64));
		Context->AllOpData->Projections.Add(MoveTemp(Result.Projection));
		Context->AllOpData->IsClosedLoop.Add(Result.bIsClosedLoop);
		Context->AllOpData->RegisterFacade(Result.Facade, ArrayIndex);

		TotalDataNum++;
	}

	return TotalDataNum;
}

void FPCGExClipper2ProcessorElement::BuildProcessingGroups(
	FPCGExClipper2ProcessorContext* Context,
	const UPCGExClipper2ProcessorSettings* Settings,
	const TArray<int32>& MainIndices,
	const TArray<int32>& OperandIndices) const
{
	const TArray<TSharedPtr<PCGExData::FFacade>>& AllFacades = Context->AllOpData->Facades;

	// Collect main facades for matching
	TArray<TSharedPtr<PCGExData::FFacade>> MainFacades;
	MainFacades.Reserve(MainIndices.Num());
	for (const int32 Idx : MainIndices)
	{
		if (Idx < AllFacades.Num()) { MainFacades.Add(AllFacades[Idx]); }
	}

	// Determine main data partitions
	TArray<TArray<int32>> MainPartitions;

	bool bDoMainMatching = false;
	if (Settings->MainDataMatching.IsEnabled() && Settings->MainDataMatching.Mode != EPCGExMapMatchMode::Disabled)
	{
		auto Matcher = MakeShared<PCGExMatching::FDataMatcher>();
		Matcher->SetDetails(&Settings->MainDataMatching);

		bDoMainMatching = Matcher->Init(Context, MainFacades, true);

		if (bDoMainMatching)
		{
			PCGExMatching::Helpers::GetMatchingSourcePartitions(Matcher, MainFacades, MainPartitions, true);
		}
	}

	if (!bDoMainMatching)
	{
		// No matching - each main input is its own group
		switch (Settings->GetGroupingPolicy())
		{
		case PCGExClipper2::EMainGroupingPolicy::Split:
			MainPartitions.Reserve(MainIndices.Num());
			for (const int32 Index : MainIndices) { MainPartitions.Add({Index}); }
			break;
		case PCGExClipper2::EMainGroupingPolicy::Consolidate:
			{
				TArray<int32>& Consolidated = MainPartitions.Emplace_GetRef();
				Consolidated.Append(MainIndices);
			}
			break;
		}
	}
	else
	{
		// Convert facade indices to AllOpData indices
		for (TArray<int32>& Partition : MainPartitions)
		{
			for (int32& Idx : Partition)
			{
				if (Idx < MainFacades.Num())
				{
					// Find the actual index in MainIndices
					const TSharedPtr<PCGExData::FFacade>& Facade = MainFacades[Idx];
					Idx = Context->AllOpData->FindSourceIndex(Facade->Idx);
					if (Idx == INDEX_NONE) { Idx = 0; } // Fallback
				}
			}
		}
	}

	// Handle operand matching if we have operands
	TArray<TArray<int32>> OperandPartitions;

	if (!OperandIndices.IsEmpty())
	{
		TArray<TSharedPtr<PCGExData::FFacade>> OperandFacades;
		OperandFacades.Reserve(OperandIndices.Num());
		for (const int32 Idx : OperandIndices)
		{
			if (Idx < AllFacades.Num()) { OperandFacades.Add(AllFacades[Idx]); }
		}

		bool bDoOperandMatching = false;
		if (Settings->OperandsDataMatching.IsEnabled() && Settings->OperandsDataMatching.Mode != EPCGExMapMatchMode::Disabled)
		{
			auto Matcher = MakeShared<PCGExMatching::FDataMatcher>();
			Matcher->SetDetails(&Settings->OperandsDataMatching);

			bDoOperandMatching = Matcher->Init(Context, OperandFacades, true, PCGExClipper2::Labels::SourceOperandsMatchRulesLabel);

			if (bDoOperandMatching)
			{
				PCGExMatching::FScope Scope(OperandFacades.Num(), true);
				OperandPartitions.Reserve(MainPartitions.Num());

				for (int i = 0; i < MainPartitions.Num(); i++)
				{
					const TArray<int32>& MainPartition = MainPartitions[i];
					TArray<int32>& Matches = OperandPartitions.Emplace_GetRef();

					for (const int32 MainIndex : MainPartition)
					{
						if (MainIndex < AllFacades.Num())
						{
							Matcher->GetMatchingSourcesIndices(AllFacades[MainIndex]->Source->GetTaggedData(), Scope, Matches);
						}
					}

					// Convert to AllOpData indices
					for (int32& Idx : Matches)
					{
						if (Idx < OperandFacades.Num())
						{
							Idx = Context->AllOpData->FindSourceIndex(OperandFacades[Idx]->Idx);
							if (Idx == INDEX_NONE) { Idx = OperandIndices[0]; }
						}
					}

					if (Matches.IsEmpty())
					{
						// Remove this partition - no matching operands
						OperandPartitions.Pop();
						MainPartitions.RemoveAt(i);
						i--;
					}
				}
			}
		}

		if (!bDoOperandMatching)
		{
			// All operands match all main groups
			OperandPartitions.Reserve(MainPartitions.Num());
			for (int i = 0; i < MainPartitions.Num(); i++)
			{
				OperandPartitions.Add(OperandIndices);
			}
		}
	}

	// Build processing groups
	Context->ProcessingGroups.Reserve(MainPartitions.Num());

	for (int32 i = 0; i < MainPartitions.Num(); i++)
	{
		TSharedPtr<PCGExClipper2::FProcessingGroup> Group = MakeShared<PCGExClipper2::FProcessingGroup>();
		Group->SubjectIndices = MoveTemp(MainPartitions[i]);

		if (i < OperandPartitions.Num()) { Group->OperandIndices = MoveTemp(OperandPartitions[i]); }

		Group->Prepare(Context->AllOpData);

		if (Group->IsValid()) { Context->ProcessingGroups.Add(Group); }
	}
}

#undef LOCTEXT_NAMESPACE

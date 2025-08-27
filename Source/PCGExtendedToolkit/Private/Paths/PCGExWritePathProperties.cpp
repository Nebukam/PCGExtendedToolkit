﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExWritePathProperties.h"
#include "MinVolumeBox3.h"
#include "OrientedBoxTypes.h"
#include "Data/PCGExDataTag.h"

#define LOCTEXT_NAMESPACE "PCGExWritePathPropertiesElement"
#define PCGEX_NAMESPACE WritePathProperties

bool UPCGExWritePathPropertiesSettings::WriteAnyPathData() const
{
#define PCGEX_PATH_MARK_TRUE(_NAME, _TYPE, _DEFAULT) if(bWrite##_NAME){return true;}
	PCGEX_FOREACH_FIELD_PATH(PCGEX_PATH_MARK_TRUE)
#undef PCGEX_PATH_MARK_TRUE

	return false;
}

TArray<FPCGPinProperties> UPCGExWritePathPropertiesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	if (WriteAnyPathData()) { PCGEX_PIN_PARAMS(PCGExWritePathProperties::OutputPathProperties, "...", Advanced, {}) }
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(WritePathProperties)
PCGEX_ELEMENT_BATCH_POINT_IMPL(WritePathProperties)

bool FPCGExWritePathPropertiesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPathProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(WritePathProperties)

	PCGEX_FOREACH_FIELD_PATH_POINT(PCGEX_OUTPUT_VALIDATE_NAME)
	PCGEX_FOREACH_FIELD_PATH(PCGEX_OUTPUT_VALIDATE_NAME)

	if (Settings->PathAttributePackingMode == EPCGExAttributeSetPackingMode::Merged &&
		Settings->WriteAnyPathData())
	{
		Context->PathAttributeSet = Context->ManagedObjects->New<UPCGParamData>();
		PCGEx::InitArray(Context->MergedAttributeSetKeys, Context->MainPoints->Num());
	}

	return true;
}

bool FPCGExWritePathPropertiesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExWritePathPropertiesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(WritePathProperties)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some input have less than 2 points and will be ignored."))
		if (!Context->StartBatchProcessingPoints(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry)
			{
				if (Entry->GetNum() < 2)
				{
					bHasInvalidInputs = true;
					return false;
				}
				if (Context->PathAttributeSet)
				{
					Context->MergedAttributeSetKeys[Entry->IOIndex] = Context->PathAttributeSet->Metadata->AddEntry();
				}

				return true;
			},
			[&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
			{
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any valid path."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::State_Done)

	PCGEX_OUTPUT_VALID_PATHS(MainPoints)

	if (Settings->WriteAnyPathData())
	{
		if (Context->PathAttributeSet)
		{
			FPCGTaggedData& StagedData = Context->StageOutput(Context->PathAttributeSet, false, false);
			StagedData.Pin = PCGExWritePathProperties::OutputPathProperties;
		}
		else
		{
			Context->MainBatch->Output();
		}
	}

	return Context->TryComplete();
}

namespace PCGExWritePathProperties
{
	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExWritePathProperties::Process);

		// Must be set before process for filters
		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!IProcessor::Process(InAsyncManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::Duplicate)

		ProjectionDetails = Settings->ProjectionDetails;
		if (ProjectionDetails.Method == EPCGExProjectionMethod::Normal) { if (!ProjectionDetails.Init(PointDataFacade)) { return false; } }
		else { ProjectionDetails.Init(PCGExGeo::FBestFitPlane(PointDataFacade->GetIn()->GetConstTransformValueRange())); }

		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		Path = MakeShared<PCGExPaths::FPath>(PointDataFacade->GetIn(), 0);
		bClosedLoop = Path->IsClosedLoop();

		Path->IOIndex = PointDataFacade->Source->IOIndex;
		PathLength = Path->AddExtra<PCGExPaths::FPathEdgeLength>(true); // Force compute length
		if (Settings->bWritePointNormal || Settings->bWritePointBinormal) { PathBinormal = Path->AddExtra<PCGExPaths::FPathEdgeBinormal>(false, Settings->UpVector); }
		if (Settings->bWritePointAvgNormal) { PathAvgNormal = Path->AddExtra<PCGExPaths::FPathEdgeAvgNormal>(false, Settings->UpVector); }

		{
			const TSharedRef<PCGExData::FFacade>& OutputFacade = PointDataFacade;
			PCGEX_FOREACH_FIELD_PATH_POINT(PCGEX_OUTPUT_INIT)
		}

		///

		const int32 NumPoints = PointIO->GetIn()->GetNumPoints();

		PCGEx::InitArray(Details, NumPoints);

		for (int i = 0; i < NumPoints; i++)
		{
			Details[i] = {
				i,
				FVector::ZeroVector,
				FVector::ZeroVector,
				FVector::ZeroVector,
				FVector::ZeroVector
			};
		}

		StartParallelLoopForPoints();

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::WritePathProperties::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		PCGEX_SCOPE_LOOP(Index)
		{
			FPointDetails& Current = Details[Index];

			Current.ToPrev = Path->DirToPrevPoint(Index);
			Current.ToNext = Path->DirToNextPoint(Index);

			int32 ExtraIndex = !bClosedLoop && Index == Path->LastIndex ? Path->LastEdge : Index;
			Path->ComputeEdgeExtra(ExtraIndex);

			PCGEX_OUTPUT_VALUE(PointNormal, Index, PathBinormal->Normals[ExtraIndex]);
			PCGEX_OUTPUT_VALUE(PointBinormal, Index, PathBinormal->Get(ExtraIndex));
			PCGEX_OUTPUT_VALUE(PointAvgNormal, Index, PathAvgNormal->Get(ExtraIndex));

			PCGEX_OUTPUT_VALUE(DirectionToNext, Index, Current.ToNext);
			PCGEX_OUTPUT_VALUE(DirectionToPrev, Index, Current.ToPrev);

			PCGEX_OUTPUT_VALUE(DistanceToNext, Index, !Path->IsClosedLoop() && Index == Path->LastIndex ? 0 : PathLength->Get(Index))
			PCGEX_OUTPUT_VALUE(DistanceToPrev, Index, Index == 0 ? Path->IsClosedLoop() ? PathLength->Get(Path->LastEdge) : 0 : PathLength->Get(Index-1))

			PCGEX_OUTPUT_VALUE(Dot, Index, FVector::DotProduct(Current.ToPrev*-1, Current.ToNext));
			PCGEX_OUTPUT_VALUE(Angle, Index, PCGExSampling::GetAngle(Settings->AngleRange, Current.ToPrev, Current.ToNext));
		}
	}

	void FProcessor::CompleteWork()
	{
		const TSharedRef<PCGExData::FPointIO>& PointIO = PointDataFacade->Source;

		FVector PathCentroid = FVector::ZeroVector;
		FVector PathDir = Details[0].ToNext;

		// Compute path-wide data

		// Compute path-wide, per-point stuff
		double TraversedDistance = 0;
		for (int i = 0; i < Path->NumPoints; i++)
		{
			if (Settings->bTagConcave || Settings->bTagConvex) { Path->UpdateConvexity(i); }

			const FPointDetails& Detail = Details[i];
			PathDir += Detail.ToNext;

			PCGEX_OUTPUT_VALUE(PointTime, i, Settings->bTimeOneMinus ? 1 - (TraversedDistance / PathLength->TotalLength) : TraversedDistance / PathLength->TotalLength);

			PCGEX_OUTPUT_VALUE(DistanceToStart, i, TraversedDistance);
			PCGEX_OUTPUT_VALUE(DistanceToEnd, i, PathLength->TotalLength - TraversedDistance);

			TraversedDistance += !Path->IsClosedLoop() && i == Path->LastIndex ? 0 : PathLength->Get(i);
			PathCentroid += Path->GetPos_Unsafe(i);
		}

		if (!bClosedLoop)
		{
			const FPointDetails& First = Details[0];
			const FPointDetails& Last = Details[Path->LastIndex];

			PCGEX_OUTPUT_VALUE(Dot, 0, -1);
			PCGEX_OUTPUT_VALUE(Angle, 0, PCGExSampling::GetAngle(Settings->AngleRange, First.ToNext *-1, First.ToNext));

			PCGEX_OUTPUT_VALUE(Dot, Path->LastIndex, -1);
			PCGEX_OUTPUT_VALUE(Angle, Path->LastIndex, PCGExSampling::GetAngle(Settings->AngleRange, Last.ToPrev *-1, Last.ToPrev));
		}

		if (Settings->WriteAnyPathData())
		{
			TArray<FVector2D> WindedPoints;
			ProjectionDetails.ProjectFlat(PointDataFacade, WindedPoints);

			const PCGExGeo::FPolygonInfos PolyInfos = PCGExGeo::FPolygonInfos(WindedPoints);

			PathAttributeSet = Context->PathAttributeSet ? Context->PathAttributeSet.Get() : Context->ManagedObjects->New<UPCGParamData>();
			const int64 Key = Context->PathAttributeSet ? Context->MergedAttributeSetKeys[PointDataFacade->Source->IOIndex] : PathAttributeSet->Metadata->AddEntry();

#define PCGEX_OUTPUT_PATH_VALUE(_NAME, _TYPE, _VALUE) if(Context->bWrite##_NAME){\
	if (Settings->bWritePathDataToPoints) { WriteMark(PointIO, Settings->_NAME##AttributeName, _VALUE);}\
			PathAttributeSet->Metadata->FindOrCreateAttribute<_TYPE>(PCGEx::GetAttributeIdentifier(Settings->_NAME##AttributeName, PathAttributeSet).Name, _VALUE)->SetValue(Key, _VALUE); }

			PCGEX_OUTPUT_PATH_VALUE(PathLength, double, PathLength->TotalLength)
			PCGEX_OUTPUT_PATH_VALUE(PathDirection, FVector, (PathDir / Path->NumPoints).GetSafeNormal())
			PCGEX_OUTPUT_PATH_VALUE(PathCentroid, FVector, (PathCentroid / Path->NumPoints))
			PCGEX_OUTPUT_PATH_VALUE(IsClockwise, bool, PolyInfos.bIsClockwise)
			PCGEX_OUTPUT_PATH_VALUE(Area, double, PolyInfos.Area * 0.01)
			PCGEX_OUTPUT_PATH_VALUE(Perimeter, double, PolyInfos.Perimeter)
			PCGEX_OUTPUT_PATH_VALUE(Compactness, double, PolyInfos.Compactness)

			if (Settings->bWriteBoundingBoxCenter ||
				Settings->bWriteBoundingBoxExtent ||
				Settings->bWriteBoundingBoxOrientation)
			{
				UE::Geometry::TMinVolumeBox3<double> Box;
				if (Box.Solve(Path->NumPoints, [PathPtr = Path.Get()](int32 i) { return PathPtr->GetPos_Unsafe(i); }))
				{
					UE::Geometry::FOrientedBox3d Result;
					Box.GetResult(Result);

					PCGEX_OUTPUT_PATH_VALUE(BoundingBoxCenter, FVector, Result.Center());
					PCGEX_OUTPUT_PATH_VALUE(BoundingBoxExtent, FVector, Result.Extents);
					PCGEX_OUTPUT_PATH_VALUE(BoundingBoxOrientation, FQuat, FQuat(Result.Frame.Rotation));
				}
				else
				{
					const FBox Bounds = PointIO->GetIn()->GetBounds();
					PCGEX_OUTPUT_PATH_VALUE(BoundingBoxCenter, FVector, Bounds.GetCenter());
					PCGEX_OUTPUT_PATH_VALUE(BoundingBoxExtent, FVector, Bounds.GetExtent());
					PCGEX_OUTPUT_PATH_VALUE(BoundingBoxOrientation, FQuat, FQuat::Identity);
				}
			}


#undef PCGEX_OUTPUT_PATH_VALUE
		}

		///

		if (Path->ConvexitySign != 0)
		{
			if (Settings->bTagConcave && !Path->bIsConvex) { PointIO->Tags->AddRaw(Settings->ConcaveTag); }
			if (Settings->bTagConvex && Path->bIsConvex) { PointIO->Tags->AddRaw(Settings->ConvexTag); }
		}

		PointDataFacade->WriteFastest(AsyncManager);
	}

	void FProcessor::Output()
	{
		TProcessor<FPCGExWritePathPropertiesContext, UPCGExWritePathPropertiesSettings>::Output();
		if (PathAttributeSet && !Context->PathAttributeSet)
		{
			FPCGTaggedData& StagedData = Context->StageOutput(PathAttributeSet, false, false);
			StagedData.Pin = OutputPathProperties;
		}
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

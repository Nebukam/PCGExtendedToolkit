// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Elements/Diagrams/PCGExBuildVoronoiGraph2D.h"


#include "Helpers/PCGExRandomHelpers.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTags.h"
#include "Data/PCGExPointIO.h"


#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Math/Geo/PCGExDelaunay.h"
#include "Math/Geo/PCGExVoronoi.h"
#include "Clusters/PCGExCluster.h"
#include "Core/PCGExMT.h"
#include "Data/PCGExClusterData.h"
#include "Graphs/PCGExGraph.h"
#include "Graphs/PCGExGraphBuilder.h"
#include "Math/PCGExBestFitPlane.h"

#define LOCTEXT_NAMESPACE "PCGExGraphs"
#define PCGEX_NAMESPACE BuildVoronoiGraph2D

bool FPCGExVoronoiSitesOutputDetails::Validate(FPCGExContext* InContext) const
{
	if (bWriteInfluencesCount) { PCGEX_VALIDATE_NAME_C(InContext, InfluencesCountAttributeName) }
	if (bWriteMinRadius) { PCGEX_VALIDATE_NAME_C(InContext, MinRadiusAttributeName) }
	if (bWriteMaxRadius) { PCGEX_VALIDATE_NAME_C(InContext, MaxRadiusAttributeName) }
	return true;
}

void FPCGExVoronoiSitesOutputDetails::Init(const TSharedPtr<PCGExData::FFacade>& InSiteFacade)
{
	InTransforms = InSiteFacade->GetIn()->GetConstTransformValueRange();
	const int32 NumSites = InTransforms.Num();

	Locations.Init(FVector::ZeroVector, NumSites);
	Influences.Init(0, NumSites);

	if (bWriteMinRadius)
	{
		MinRadiusWriter = InSiteFacade->GetWritable<double>(MinRadiusAttributeName, 0, true, PCGExData::EBufferInit::New);
		MinRadius = StaticCastSharedPtr<PCGExData::TArrayBuffer<double>>(MinRadiusWriter)->GetOutValues();
		bWantsDist = true;
	}

	if (bWriteMaxRadius)
	{
		MaxRadiusWriter = InSiteFacade->GetWritable<double>(MaxRadiusAttributeName, 0, true, PCGExData::EBufferInit::New);
		MaxRadius = StaticCastSharedPtr<PCGExData::TArrayBuffer<double>>(MaxRadiusWriter)->GetOutValues();
		bWantsDist = true;
	}

	if (bWriteInfluencesCount)
	{
		InfluenceCountWriter = InSiteFacade->GetWritable<int32>(InfluencesCountAttributeName, 0, true, PCGExData::EBufferInit::New);
	}
}

void FPCGExVoronoiSitesOutputDetails::AddInfluence(const int32 SiteIndex, const FVector& SitePosition)
{
	Locations[SiteIndex] += SitePosition;
	Influences[SiteIndex] += 1;

	if (bWantsDist)
	{
		const double Dist = FVector::Distance(SitePosition, InTransforms[SiteIndex].GetLocation());

		if (bWriteMinRadius)
		{
			double& Min = *(MinRadius->GetData() + SiteIndex);
			Min = FMath::Min(Min, Dist);
		}

		if (bWriteMaxRadius)
		{
			double& Max = *(MaxRadius->GetData() + SiteIndex);
			Max = FMath::Max(Max, Dist);
		}
	}
}

void FPCGExVoronoiSitesOutputDetails::Output(const int32 SiteIndex)
{
	if (InfluenceCountWriter) { InfluenceCountWriter->SetValue(SiteIndex, Influences[SiteIndex]); }
}

TArray<FPCGPinProperties> UPCGExBuildVoronoiGraph2DSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::OutputPinProperties();
	PCGEX_PIN_POINTS(PCGExClusters::Labels::OutputEdgesLabel, "Point data representing edges.", Required)
	if (bOutputSites) { PCGEX_PIN_POINTS(PCGExClusters::Labels::OutputSitesLabel, "Updated Delaunay sites.", Required) }
	return PinProperties;
}

PCGEX_INITIALIZE_ELEMENT(BuildVoronoiGraph2D)
PCGEX_ELEMENT_BATCH_POINT_IMPL(BuildVoronoiGraph2D)

bool FPCGExBuildVoronoiGraph2DElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExPointsProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(BuildVoronoiGraph2D)

	PCGEX_VALIDATE_NAME(Settings->HullAttributeName)
	if (!Settings->SitesOutputDetails.Validate(Context)) { return false; }

	if (Settings->bOutputSites)
	{
		if (!Settings->bPruneOpenSites) { PCGEX_VALIDATE_NAME(Settings->OpenSiteFlag) }

		Context->SitesOutput = MakeShared<PCGExData::FPointIOCollection>(Context);
		Context->SitesOutput->OutputPin = PCGExClusters::Labels::OutputSitesLabel;

		for (const TSharedPtr<PCGExData::FPointIO>& IO : Context->MainPoints->Pairs)
		{
			Context->SitesOutput->Emplace_GetRef(IO, PCGExData::EIOInit::NoInit);
		}
	}

	return true;
}

bool FPCGExBuildVoronoiGraph2DElement::AdvanceWork(FPCGExContext* InContext, const UPCGExSettings* InSettings) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExBuildVoronoiGraph2DElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(BuildVoronoiGraph2D)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		PCGEX_ON_INVALILD_INPUTS(FTEXT("Some inputs have less than 3 points and won't be processed."))

		if (!Context->StartBatchProcessingPoints([&](const TSharedPtr<PCGExData::FPointIO>& Entry)
		                                         {
			                                         if (Entry->GetNum() < 3)
			                                         {
				                                         bHasInvalidInputs = true;
				                                         return false;
			                                         }
			                                         return true;
		                                         }, [&](const TSharedPtr<PCGExPointsMT::IBatch>& NewBatch)
		                                         {
			                                         NewBatch->bRequiresWriteStep = true;
		                                         }))
		{
			return Context->CancelExecution(TEXT("Could not find any valid inputs to build from."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGExCommon::States::State_Done)

	Context->MainPoints->StageOutputs();
	if (Context->SitesOutput) { Context->SitesOutput->StageOutputs(); }
	Context->MainBatch->Output();

	return Context->TryComplete();
}

namespace PCGExBuildVoronoiGraph2D
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InTaskManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExBuildVoronoiGraph2D::Process);

		if (!IProcessor::Process(InTaskManager)) { return false; }

		PCGEX_INIT_IO(PointDataFacade->Source, PCGExData::EIOInit::New)

		SitesOutputDetails = Settings->SitesOutputDetails;

		ProjectionDetails = Settings->ProjectionDetails;
		if (ProjectionDetails.Method == EPCGExProjectionMethod::Normal) { if (!ProjectionDetails.Init(PointDataFacade)) { return false; } }
		else { ProjectionDetails.Init(PCGExMath::FBestFitPlane(PointDataFacade->GetIn()->GetConstTransformValueRange())); }

		// Build voronoi

		TArray<FVector> ActivePositions;
		PCGExPointArrayDataHelpers::PointsToPositions(PointDataFacade->GetIn(), ActivePositions);

		Voronoi = MakeShared<PCGExMath::Geo::TVoronoi2>();

		const FBox Bounds = PointDataFacade->GetIn()->GetBounds().ExpandBy(Settings->ExpandBounds);
		bool bSuccess = false;

		bSuccess = Voronoi->Process(ActivePositions, ProjectionDetails, Bounds, WithinBounds);

		if (!bSuccess)
		{
			PCGE_LOG_C(Warning, GraphAndLog, ExecutionContext, FTEXT("Some inputs generated invalid results."));
			return false;
		}

		const int32 NumSites = Voronoi->Centroids.Num();

		ActivePositions.Empty();

		SitesPositions.SetNumUninitialized(NumSites);


		using UpdatePositionCallback = std::function<void(const int32)>;
		UpdatePositionCallback UpdateSitePosition = [](const int32 SiteIndex)
		{
		};

		auto MarkOOB = [&](const int32 SiteIndex)
		{
			const PCGExMath::Geo::FDelaunaySite2& Site = Voronoi->Delaunay->Sites[SiteIndex];
			for (int i = 0; i < 3; i++) { IsVtxValid[Site.Vtx[i]] = false; }
		};

		const int32 DelaunaySitesNum = PointDataFacade->GetNum(PCGExData::EIOSide::In);

		if (Settings->bOutputSites)
		{
			IsVtxValid.Init(true, DelaunaySitesNum);

			for (int i = 0; i < IsVtxValid.Num(); i++) { IsVtxValid[i] = !Voronoi->Delaunay->DelaunayHull.Contains(i); }

			UpdateSitePosition = [&](const int32 SiteIndex)
			{
				const PCGExMath::Geo::FDelaunaySite2& Site = Voronoi->Delaunay->Sites[SiteIndex];
				const FVector& SitePos = SitesPositions[SiteIndex];
				for (int i = 0; i < 3; i++) { SitesOutputDetails.AddInfluence(Site.Vtx[i], SitePos); }
			};

			SiteDataFacade = MakeShared<PCGExData::FFacade>(Context->SitesOutput->Pairs[PointDataFacade->Source->IOIndex].ToSharedRef());
			PCGEX_INIT_IO(SiteDataFacade->Source, PCGExData::EIOInit::Duplicate)
			SiteDataFacade->GetOut()->AllocateProperties(EPCGPointNativeProperties::Transform);

			SitesOutputDetails.Init(SiteDataFacade);

			if (Settings->bPruneOutOfBounds && !Settings->bPruneOpenSites) { OpenSiteWriter = SiteDataFacade->GetWritable<bool>(Settings->OpenSiteFlag, PCGExData::EBufferInit::New); }
		}

		if (!PointDataFacade->Source->InitializeOutput<UPCGExClusterNodesData>(PCGExData::EIOInit::New)) { return false; }

		if (Settings->Method == EPCGExCellCenter::Circumcenter && Settings->bPruneOutOfBounds)
		{
			int32 NumCentroids = 0;

			TArray<int32> RemappedIndices;
			RemappedIndices.Init(-1, NumSites);

			for (int i = 0; i < NumSites; i++)
			{
				SitesPositions[i] = Voronoi->Circumcenters[i];
				if (!WithinBounds[i]) { continue; }
				RemappedIndices[i] = NumCentroids++;
			}

			UPCGBasePointData* CentroidsPoints = PointDataFacade->GetOut();
			(void)PCGExPointArrayDataHelpers::SetNumPointsAllocated(CentroidsPoints, NumCentroids, PointDataFacade->GetAllocations());

			TPCGValueRange<FTransform> OutTransforms = CentroidsPoints->GetTransformValueRange(true);
			TPCGValueRange<int32> OutSeeds = CentroidsPoints->GetSeedValueRange(true);

			for (int i = 0; i < NumSites; i++)
			{
				if (const int32 Idx = RemappedIndices[i]; Idx != -1)
				{
					OutTransforms[Idx].SetLocation(SitesPositions[i]);
					OutSeeds[Idx] = PCGExRandomHelpers::ComputeSpatialSeed(SitesPositions[i]);
				}
			}

			TArray<uint64> ValidEdges;
			ValidEdges.Reserve(Voronoi->VoronoiEdges.Num());

			if (Settings->bOutputSites)
			{
				if (Settings->bPruneOpenSites)
				{
					for (const uint64 Hash : Voronoi->VoronoiEdges)
					{
						const int32 HA = PCGEx::H64A(Hash);
						const int32 HB = PCGEx::H64B(Hash);
						const int32 A = RemappedIndices[HA];
						const int32 B = RemappedIndices[HB];

						if (A == -1 || B == -1)
						{
							if (A == -1) { MarkOOB(HA); }
							if (B == -1) { MarkOOB(HB); }
							continue;
						}
						ValidEdges.Add(PCGEx::H64(A, B));

						UpdateSitePosition(HA);
						UpdateSitePosition(HB);
					}
				}
				else
				{
					for (const uint64 Hash : Voronoi->VoronoiEdges)
					{
						const int32 HA = PCGEx::H64A(Hash);
						const int32 HB = PCGEx::H64B(Hash);
						const int32 A = RemappedIndices[HA];
						const int32 B = RemappedIndices[HB];

						UpdateSitePosition(HA);
						UpdateSitePosition(HB);

						if (A == -1 || B == -1)
						{
							if (A == -1) { MarkOOB(HA); }
							if (B == -1) { MarkOOB(HB); }
							continue;
						}
						ValidEdges.Add(PCGEx::H64(A, B));
					}
				}
			}
			else
			{
				for (const uint64 Hash : Voronoi->VoronoiEdges)
				{
					const int32 A = RemappedIndices[PCGEx::H64A(Hash)];
					const int32 B = RemappedIndices[PCGEx::H64B(Hash)];
					if (A == -1 || B == -1) { continue; }
					ValidEdges.Add(PCGEx::H64(A, B));
				}
			}

			RemappedIndices.Empty();

			GraphBuilder = MakeShared<PCGExGraphs::FGraphBuilder>(PointDataFacade, &Settings->GraphBuilderDetails);
			GraphBuilder->Graph->InsertEdges(ValidEdges, -1);
		}
		else
		{
			UPCGBasePointData* Centroids = PointDataFacade->GetOut();
			const int32 NumCentroids = Voronoi->Centroids.Num();
			(void)PCGExPointArrayDataHelpers::SetNumPointsAllocated(Centroids, NumCentroids, PointDataFacade->GetAllocations());

			TPCGValueRange<FTransform> OutTransforms = Centroids->GetTransformValueRange(true);
			TPCGValueRange<int32> OutSeeds = Centroids->GetSeedValueRange(true);

#define PCGEX_UPDATE_VSITE_DATA\
			SitesPositions[i] = CC;\
			OutTransforms[i].SetLocation(CC);\
			OutSeeds[i] = PCGExRandomHelpers::ComputeSpatialSeed(CC);
			if (Settings->Method == EPCGExCellCenter::Circumcenter)
			{
				for (int i = 0; i < NumCentroids; i++)
				{
					const FVector CC = Voronoi->Circumcenters[i];
					PCGEX_UPDATE_VSITE_DATA
				}
			}
			else if (Settings->Method == EPCGExCellCenter::Centroid)
			{
				for (int i = 0; i < NumCentroids; i++)
				{
					const FVector CC = Voronoi->Centroids[i];
					PCGEX_UPDATE_VSITE_DATA
				}
			}
			else if (Settings->Method == EPCGExCellCenter::Balanced)
			{
				for (int i = 0; i < NumCentroids; i++)
				{
					const FVector CC = WithinBounds[i] ? Voronoi->Circumcenters[i] : Voronoi->Centroids[i];
					PCGEX_UPDATE_VSITE_DATA
				}
			}

#undef PCGEX_UPDATE_VSITE_DATA

			if (Settings->bOutputSites)
			{
				for (const uint64 Hash : Voronoi->VoronoiEdges)
				{
					const int32 HA = PCGEx::H64A(Hash);
					const int32 HB = PCGEx::H64B(Hash);

					UpdateSitePosition(HA);
					UpdateSitePosition(HB);

					if (!WithinBounds[HA]) { MarkOOB(HA); }
					if (!WithinBounds[HB]) { MarkOOB(HB); }
				}
			}


			GraphBuilder = MakeShared<PCGExGraphs::FGraphBuilder>(PointDataFacade, &Settings->GraphBuilderDetails);
			GraphBuilder->Graph->InsertEdges(Voronoi->VoronoiEdges, -1);
		}

		Voronoi.Reset();

		GraphBuilder->bInheritNodeData = false;
		GraphBuilder->CompileAsync(TaskManager, false);

		if (Settings->bOutputSites)
		{
			PCGEX_ASYNC_GROUP_CHKD(TaskManager, OutputSites)

			OutputSites->OnSubLoopStartCallback = [PCGEX_ASYNC_THIS_CAPTURE](const PCGExMT::FScope& Scope)
			{
				PCGEX_ASYNC_THIS

				TPCGValueRange<FTransform> OutTransforms = This->SiteDataFacade->GetOut()->GetTransformValueRange(false);

				const TArray<FVector>& SitesPositionsDetails = This->SitesOutputDetails.Locations;
				const TArray<int32>& SitesInfluenceCountDetails = This->SitesOutputDetails.Influences;

				PCGEX_SCOPE_LOOP(Index)
				{
					const bool bIsWithinBounds = This->IsVtxValid[Index];
					if (This->OpenSiteWriter) { This->OpenSiteWriter->SetValue(Index, bIsWithinBounds); }
					This->SitesOutputDetails.Output(Index);
					if (SitesInfluenceCountDetails[Index] == 0) { continue; }
					OutTransforms[Index].SetLocation(SitesPositionsDetails[Index] / SitesInfluenceCountDetails[Index]);
				}
			};

			OutputSites->StartSubLoops(DelaunaySitesNum, PCGEX_CORE_SETTINGS.GetPointsBatchChunkSize());
		}

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		//HullMarkPointWriter->Values[Index] = Voronoi->Delaunay->DelaunayHull.Contains(Index);
	}

	void FProcessor::CompleteWork()
	{
		if (!GraphBuilder->bCompiledSuccessfully)
		{
			bIsProcessorValid = false;
			PCGEX_CLEAR_IO_VOID(PointDataFacade->Source)
			return;
		}

		if (SiteDataFacade)
		{
			SiteDataFacade->WriteFastest(TaskManager);
			if (Settings->bPruneOpenSites)
			{
				const int32 Iterations = SiteDataFacade->GetOut()->GetNumPoints();

				TArray<int8> Mask;
				Mask.Init(0, Iterations);

				for (int32 i = 0; i < Iterations; i++) { if (IsVtxValid[i]) { Mask[i] = 1; } }

				(void)SiteDataFacade->Source->Gather(Mask);
			}
		}

		if (SiteDataFacade) { SiteDataFacade->Source->Tags->Append(PointDataFacade->Source->Tags.ToSharedRef()); }
	}

	void FProcessor::Write()
	{
		PointDataFacade->WriteFastest(TaskManager);
	}

	void FProcessor::Output()
	{
		GraphBuilder->StageEdgesOutputs();
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

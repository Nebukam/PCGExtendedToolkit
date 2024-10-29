// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Shapes/PCGExCreateShapes.h"

#include "Data/PCGExData.h"
#include "Shapes/PCGExShapeBuilderOperation.h"


#define LOCTEXT_NAMESPACE "PCGExCreateShapesElement"
#define PCGEX_NAMESPACE CreateShapes

PCGEX_INITIALIZE_ELEMENT(CreateShapes)

bool FPCGExCreateShapesElement::Boot(FPCGExContext* InContext) const
{
	if (!FPCGExShapeProcessorElement::Boot(InContext)) { return false; }

	PCGEX_CONTEXT_AND_SETTINGS(CreateShapes)

	return true;
}

bool FPCGExCreateShapesElement::ExecuteInternal(FPCGContext* InContext) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGExCreateShapesElement::Execute);

	PCGEX_CONTEXT_AND_SETTINGS(CreateShapes)
	PCGEX_EXECUTION_CHECK
	PCGEX_ON_INITIAL_EXECUTION
	{
		if (!Context->StartBatchProcessingPoints<PCGExPointsMT::TBatch<PCGExCreateShapes::FProcessor>>(
			[&](const TSharedPtr<PCGExData::FPointIO>& Entry) { return true; },
			[&](const TSharedPtr<PCGExPointsMT::TBatch<PCGExCreateShapes::FProcessor>>& NewBatch)
			{
				//NewBatch->bRequiresWriteStep = true;
			}))
		{
			return Context->CancelExecution(TEXT("Could not find any paths to subdivide."));
		}
	}

	PCGEX_POINTS_BATCH_PROCESSING(PCGEx::State_Done)

	if (Settings->OutputMode == EPCGExShapeOutputMode::PerSeed)
	{
		Context->MainBatch->Output();
	}
	else
	{
		Context->MainPoints->StageOutputs();
	}

	return Context->TryComplete();
}

namespace PCGExCreateShapes
{
	FProcessor::~FProcessor()
	{
	}

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager> InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCreateShapes::Process);

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		Builders.Reserve(Context->BuilderFactories.Num());
		for (const TObjectPtr<const UPCGExShapeBuilderFactoryBase> Factory : Context->BuilderFactories)
		{
			UPCGExShapeBuilderOperation* Op = Factory->CreateOperation(Context);
			Op->PrepareForSeeds(PointDataFacade);
			Builders.Add(Op);
		}

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const int32 LoopIdx, const int32 LoopCount)
	{
		const PCGExData::FPointRef PointRef = PointDataFacade->Source->GetInPointRef(Index);
		for (UPCGExShapeBuilderOperation* Op : Builders) { Op->PrepareShape(PointRef); }
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		const int32 NumBuilders = Builders.Num();
		const int32 NumSeeds = PointDataFacade->GetNum();

		if (Settings->OutputMode == EPCGExShapeOutputMode::PerDataset)
		{
			PointDataFacade->Source->InitializeOutput(Context, PCGExData::EInit::NewOutput);
			int32 StartIndex = 0;
			int32 NumPoints = 0;

			for (int i = 0; i < NumSeeds; i++)
			{
				for (int j = 0; j < NumBuilders; j++)
				{
					TSharedPtr<PCGExShapes::FShape> Shape = Builders[j]->Shapes[i];

					if (!Shape->IsValid()) { continue; }

					Shape->StartIndex = StartIndex;
					StartIndex += Shape->NumPoints;
					NumPoints += Shape->NumPoints;
				}
			}

			TArray<FPCGPoint>& MutablePoints = PointDataFacade->GetMutablePoints();
			PCGEx::InitArray(MutablePoints, NumPoints);

			for (int i = 0; i < NumSeeds; i++)
			{
				for (int j = 0; j < NumBuilders; j++)
				{
					TSharedPtr<PCGExShapes::FShape> Shape = Builders[j]->Shapes[i];
					if (!Shape->IsValid()) { continue; }
					Context->GetAsyncManager()->Start<FBuildShape>(i * j, nullptr, false, Builders[j], PointDataFacade, Shape);
				}
			}
		}
		else
		{
			const int32 NumOutputs = NumSeeds * NumBuilders;
			PerSeedFacades.Reserve(NumOutputs);

			for (int i = 0; i < NumSeeds; i++)
			{
				for (int j = 0; j < NumBuilders; j++)
				{
					TSharedPtr<PCGExShapes::FShape> Shape = Builders[j]->Shapes[i];

					if (!Shape->IsValid()) { continue; }

					TSharedPtr<PCGExData::FPointIO> IO = MakeShared<PCGExData::FPointIO>(Context, PointDataFacade->Source->GetIn());
					IO->SetInfos(i, Settings->GetMainOutputLabel());

					TSharedPtr<PCGExData::FFacade> IOFacade = MakeShared<PCGExData::FFacade>(IO.ToSharedRef());
					PerSeedFacades.Add(IOFacade);

					Context->GetAsyncManager()->Start<FBuildShape>(i * j, nullptr, true, Builders[j], IOFacade.ToSharedRef(), Shape);
				}
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		if (Settings->OutputMode == EPCGExShapeOutputMode::PerDataset)
		{
			PointDataFacade->Source->GetOutKeys(true);
		}
		else
		{
			for (const TSharedPtr<PCGExData::FFacade> IO : PerSeedFacades) { IO->Source->GetOutKeys(true); }
		}
	}

	void FProcessor::Output()
	{
		for (const TSharedPtr<PCGExData::FFacade> IO : PerSeedFacades) { IO->Source->StageOutput(); }
	}

	bool FBuildShape::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		if (bSoloShape)
		{
			ShapeDataFacade->Source->InitializeOutput(AsyncManager->Context, PCGExData::EInit::NewOutput);
			TArray<FPCGPoint>& MutablePoints = ShapeDataFacade->GetMutablePoints();
			PCGEx::InitArray(MutablePoints, Shape->NumPoints);
		}

		TArrayView<FPCGPoint> ShapePoints = MakeArrayView(ShapeDataFacade->GetMutablePoints().GetData() + Shape->StartIndex, Shape->NumPoints);
		for (int i = 0; i < ShapePoints.Num(); i++) { ShapePoints[i] = *Shape->Seed.Point; }

		Operation->BuildShape(Shape, ShapeDataFacade, ShapePoints);

		return true;
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

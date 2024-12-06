// Copyright 2024 Timothé Lapetite and contributors
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

	if (Settings->bWriteShapeId) { PCGEX_VALIDATE_NAME(Settings->ShapeIdAttributeName) }

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

	if (Settings->OutputMode == EPCGExShapeOutputMode::PerDataset)
	{
		Context->MainPoints->StageOutputs();
	}
	else
	{
		Context->MainBatch->Output();
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
		for (const TObjectPtr<const UPCGExShapeBuilderFactoryBase>& Factory : Context->BuilderFactories)
		{
			UPCGExShapeBuilderOperation* Op = Factory->CreateOperation(Context);
			if (!Op->PrepareForSeeds(Context, PointDataFacade)) { return false; }
			Builders.Add(Op);
		}

		StartParallelLoopForPoints(PCGExData::ESource::In);

		return true;
	}

	void FProcessor::ProcessSinglePoint(const int32 Index, FPCGPoint& Point, const PCGExMT::FScope& Scope)
	{
		const PCGExData::FPointRef PointRef = PointDataFacade->Source->GetInPointRef(Index);
		for (UPCGExShapeBuilderOperation* Op : Builders) { Op->PrepareShape(PointRef); }
	}

	void FProcessor::OnPointsProcessingComplete()
	{
		const int32 NumBuilders = Builders.Num();
		const int32 NumSeeds = PointDataFacade->GetNum();

		auto IsShapeValid = [&](const TSharedPtr<PCGExShapes::FShape>& Shape)-> bool
		{
			if (!Shape->IsValid()) { return false; }
			if (Settings->bRemoveBelow && Shape->NumPoints < Settings->MinPointCount) { return false; }
			if (Settings->bRemoveAbove && Shape->NumPoints > Settings->MaxPointCount) { return false; }
			return true;
		};

		if (Settings->OutputMode == EPCGExShapeOutputMode::PerDataset)
		{
			PointDataFacade->Source->InitializeOutput(PCGExData::EIOInit::New);
			int32 StartIndex = 0;
			int32 NumPoints = 0;

			for (int i = 0; i < NumSeeds; i++)
			{
				for (int j = 0; j < NumBuilders; j++)
				{
					TSharedPtr<PCGExShapes::FShape> Shape = Builders[j]->Shapes[i];

					if (!IsShapeValid(Shape)) { continue; }

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

					if (!IsShapeValid(Shape)) { continue; }

					Context->GetAsyncManager()->Start<FBuildShape>(i * j, nullptr, Builders[j], PointDataFacade, Shape);
				}
			}
		}
		else
		{
			const int32 NumOutputs = NumSeeds * NumBuilders;
			PerSeedFacades.Reserve(NumOutputs);

			for (int i = 0; i < NumSeeds; i++)
			{
				int32 StartIndex = 0;
				int32 NumPoints = 0;

				for (int j = 0; j < NumBuilders; j++)
				{
					TSharedPtr<PCGExShapes::FShape> Shape = Builders[j]->Shapes[i];

					if (!IsShapeValid(Shape)) { continue; }

					Shape->StartIndex = StartIndex;
					StartIndex += Shape->NumPoints;
					NumPoints += Shape->NumPoints;
				}

				if (NumPoints <= 0) { continue; }

				TSharedPtr<PCGExData::FPointIO> IO = NewPointIO(PointDataFacade->Source, Settings->GetMainOutputPin(), i);
				IO->InitializeOutput(PCGExData::EIOInit::New);

				TSharedPtr<PCGExData::FFacade> IOFacade = MakeShared<PCGExData::FFacade>(IO.ToSharedRef());
				PerSeedFacades.Add(IOFacade);

				TArray<FPCGPoint>& MutablePoints = IOFacade->GetMutablePoints();
				PCGEx::InitArray(MutablePoints, NumPoints);

				for (int j = 0; j < NumBuilders; j++)
				{
					TSharedPtr<PCGExShapes::FShape> Shape = Builders[j]->Shapes[i];

					if (!IsShapeValid(Shape)) { continue; }
					Context->GetAsyncManager()->Start<FBuildShape>(i * j, nullptr, Builders[j], IOFacade.ToSharedRef(), Shape);
				}
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		if (Settings->OutputMode == EPCGExShapeOutputMode::PerDataset)
		{
			PointDataFacade->Write(AsyncManager);
		}
		else
		{
			for (const TSharedPtr<PCGExData::FFacade>& Facade : PerSeedFacades)
			{
				Facade->Write(AsyncManager);
			}
		}
	}

	void FProcessor::Output()
	{
		for (const TSharedPtr<PCGExData::FFacade>& IO : PerSeedFacades) { IO->Source->StageOutput(); }
	}

	bool FBuildShape::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		FPCGExCreateShapesContext* Context = AsyncManager->GetContext<FPCGExCreateShapesContext>();
		PCGEX_SETTINGS(CreateShapes);

		TArrayView<FPCGPoint> ShapePoints = MakeArrayView(ShapeDataFacade->GetMutablePoints().GetData() + Shape->StartIndex, Shape->NumPoints);

		for (int i = 0; i < ShapePoints.Num(); i++)
		{
			ShapePoints[i] = *Shape->Seed.Point;
			ShapePoints[i].BoundsMin = Shape->Extents * -1;
			ShapePoints[i].BoundsMax = Shape->Extents;
		}

		Operation->BuildShape(Shape, ShapeDataFacade, ShapePoints);

		if (Settings->bWriteShapeId)
		{
			const TSharedPtr<PCGExData::TBuffer<double>> ShapeIdBuffer = ShapeDataFacade->GetWritable<double>(Settings->ShapeIdAttributeName, PCGExData::EBufferInit::New);
			const int32 MaxIndex = Shape->StartIndex + Shape->NumPoints;
			for (int i = Shape->StartIndex; i < MaxIndex; i++) { ShapeIdBuffer->GetMutable(i) = Operation->BaseConfig.ShapeId; }
		}

		FTransform TRA = Operation->Transform;
		FTransform TRB = Shape->Seed.Point->Transform;

		TRB.SetScale3D(FVector::OneVector);

		PCGEX_ASYNC_GROUP_CHKD(AsyncManager, TransformPointsTask);

		TransformPointsTask->OnSubLoopStartCallback =
			[ShapePoints, TRA, TRB](const PCGExMT::FScope& Scope)
			{
				for (int i = Scope.Start; i < Scope.End; i++)
				{
					FPCGPoint& Point = ShapePoints[i];
					Point.Transform = (Point.Transform * TRB) * TRA;
					Point.Transform.SetScale3D(FVector::OneVector);
					Point.Seed = PCGExRandom::ComputeSeed(Point, TRB.GetLocation());
				}
			};

		TransformPointsTask->StartSubLoops(ShapePoints.Num(), GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());

		return true;
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

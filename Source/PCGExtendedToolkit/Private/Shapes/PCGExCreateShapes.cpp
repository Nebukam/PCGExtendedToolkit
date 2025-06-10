// Copyright 2025 Timothé Lapetite and contributors
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

	bool FProcessor::Process(const TSharedPtr<PCGExMT::FTaskManager>& InAsyncManager)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGExCreateShapes::Process);

		PointDataFacade->bSupportsScopedGet = Context->bScopedAttributeGet;

		if (!FPointsProcessor::Process(InAsyncManager)) { return false; }

		Builders.Reserve(Context->BuilderFactories.Num());
		for (const TObjectPtr<const UPCGExShapeBuilderFactoryData>& Factory : Context->BuilderFactories)
		{
			TSharedPtr<FPCGExShapeBuilderOperation> Op = Factory->CreateOperation(Context);
			if (!Op->PrepareForSeeds(Context, PointDataFacade)) { return false; }
			Builders.Add(Op);
		}

		StartParallelLoopForPoints(PCGExData::EIOSide::In);

		return true;
	}

	void FProcessor::ProcessPoints(const PCGExMT::FScope& Scope)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(PCGEx::CreateShapes::ProcessPoints);

		PointDataFacade->Fetch(Scope);

		PCGEX_SCOPE_LOOP(Index)
		{
			const PCGExData::FConstPoint PointRef = PointDataFacade->GetInPoint(Index);
			for (const TSharedPtr<FPCGExShapeBuilderOperation>& Op : Builders) { Op->PrepareShape(PointRef); }
		}
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
			PCGEX_INIT_IO_VOID(PointDataFacade->Source, PCGExData::EIOInit::New)

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

			UPCGBasePointData* OutPointData = PointDataFacade->GetOut();
			PCGEx::SetNumPointsAllocated(OutPointData, NumPoints);

			for (int i = 0; i < NumSeeds; i++)
			{
				for (int j = 0; j < NumBuilders; j++)
				{
					TSharedPtr<PCGExShapes::FShape> Shape = Builders[j]->Shapes[i];

					if (!IsShapeValid(Shape)) { continue; }

					PCGEX_LAUNCH(FBuildShape, Builders[j], PointDataFacade, Shape)
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
				PCGEX_INIT_IO_VOID(IO, PCGExData::EIOInit::New)

				PCGEX_MAKE_SHARED(IOFacade, PCGExData::FFacade, IO.ToSharedRef())
				PerSeedFacades.Add(IOFacade);

				PCGEx::SetNumPointsAllocated(IOFacade->GetOut(), NumPoints);

				for (int j = 0; j < NumBuilders; j++)
				{
					TSharedPtr<PCGExShapes::FShape> Shape = Builders[j]->Shapes[i];

					if (!IsShapeValid(Shape)) { continue; }

					PCGEX_LAUNCH(FBuildShape, Builders[j], IOFacade.ToSharedRef(), Shape)
				}
			}
		}
	}

	void FProcessor::CompleteWork()
	{
		if (Settings->OutputMode == EPCGExShapeOutputMode::PerDataset)
		{
			PointDataFacade->WriteFastest(AsyncManager);
		}
		else
		{
			for (const TSharedPtr<PCGExData::FFacade>& Facade : PerSeedFacades)
			{
				Facade->WriteFastest(AsyncManager);
			}
		}
	}

	void FProcessor::Output()
	{
		for (const TSharedPtr<PCGExData::FFacade>& IO : PerSeedFacades) { (void)IO->Source->StageOutput(Context); }
	}

	void FBuildShape::ExecuteTask(const TSharedPtr<PCGExMT::FTaskManager>& AsyncManager)
	{
		FPCGExCreateShapesContext* Context = AsyncManager->GetContext<FPCGExCreateShapesContext>();
		PCGEX_SETTINGS(CreateShapes);

		UPCGBasePointData* ShapePoints = ShapeDataFacade->GetOut();

		ShapeDataFacade->Source->RepeatPoint(Shape->Seed.Index, Shape->StartIndex, Shape->NumPoints);
		TPCGValueRange<FVector> BoundsMin = ShapePoints->GetBoundsMinValueRange(false);
		TPCGValueRange<FVector> BoundsMax = ShapePoints->GetBoundsMaxValueRange(false);

		PCGExData::FScope SubScope = ShapeDataFacade->Source->GetOutScope(Shape->StartIndex, Shape->NumPoints);
		PCGEX_SUBSCOPE_LOOP(Index)
		{
			BoundsMin[Index] = Shape->Extents * -1;
			BoundsMax[Index] = Shape->Extents;
		}

		Operation->BuildShape(Shape, ShapeDataFacade, SubScope); // TODO : Use PointIO->GetOutRange so as to get a scope with a reference to the data

		if (Settings->bWriteShapeId)
		{
			const TSharedPtr<PCGExData::TBuffer<double>> ShapeIdBuffer = ShapeDataFacade->GetWritable<double>(Settings->ShapeIdAttributeName, PCGExData::EBufferInit::New);
			const int32 MaxIndex = Shape->StartIndex + Shape->NumPoints;
			for (int i = Shape->StartIndex; i < MaxIndex; i++) { ShapeIdBuffer->SetValue(i, Operation->BaseConfig.ShapeId); }
		}

		const FTransform& TRA = Operation->Transform;
		FTransform TRB = Shape->Seed.GetTransform();

		TRB.SetScale3D(FVector::OneVector);

		PCGEX_ASYNC_GROUP_CHKD_VOID(AsyncManager, TransformPointsTask);

		TransformPointsTask->OnSubLoopStartCallback =
			[TRA, TRB, SubScope](const PCGExMT::FScope& Scope)
			{
				TPCGValueRange<FTransform> OutTransforms = SubScope.Data->GetTransformValueRange(false);
				TPCGValueRange<int32> OutSeeds = SubScope.Data->GetSeedValueRange(false);

				PCGEX_SCOPE_LOOP(Index)
				{
					const int32 PointIndex = Index + SubScope.Start;

					OutTransforms[PointIndex] = (OutTransforms[PointIndex] * TRB) * TRA;
					OutTransforms[PointIndex].SetScale3D(FVector::OneVector);
					OutSeeds[PointIndex] = PCGExRandom::ComputeSpatialSeed(OutTransforms[PointIndex].GetLocation(), TRB.GetLocation());
				}
			};

		TransformPointsTask->StartSubLoops(SubScope.Count, GetDefault<UPCGExGlobalSettings>()->GetPointsBatchChunkSize());
	}
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Transform/Tensors/PCGExTensorSplineFactoryProvider.h"

#include "Paths/PCGExPaths.h"
#include "Paths/PCGExSplineToPath.h"
#include "Transform/Tensors/PCGExTensorOperation.h"

#define LOCTEXT_NAMESPACE "PCGExCreateTensor"
#define PCGEX_NAMESPACE CreateTensor


bool UPCGExTensorSplineFactoryData::InitInternalFacade(FPCGExContext* InContext)
{
	// TODO : Implement
	// TODO : Preload tangents attributes etc
	return true;
}

bool UPCGExTensorSplineFactoryData::InitInternalData(FPCGExContext* InContext)
{
	if (!Super::InitInternalData(InContext)) { return false; }

	if (bBuildFromPaths)
	{
		if (!InitInternalFacade(InContext)) { return false; }

		TArray<FPCGTaggedData> Targets = InContext->InputData.GetInputsByPin(PCGExGraph::SourcePathsLabel);
		ClosedLoop.Init();

		if (!Targets.IsEmpty())
		{
			for (const FPCGTaggedData& TaggedData : Targets)
			{
				const UPCGPointData* PathData = Cast<UPCGPointData>(TaggedData.Data);
				if (!PathData) { continue; }

				const bool bIsClosedLoop = ClosedLoop.IsClosedLoop(TaggedData);
				if (SampleInputs == EPCGExSplineSamplingIncludeMode::ClosedLoopOnly && !bIsClosedLoop) { continue; }
				if (SampleInputs == EPCGExSplineSamplingIncludeMode::OpenSplineOnly && bIsClosedLoop) { continue; }

				if (TSharedPtr<FPCGSplineStruct> SplineStruct = PCGExPaths::MakeSplineFromPoints(PathData, PointType, bIsClosedLoop))
				{
					ManagedSplines.Add(SplineStruct);
				}
			}
		}

		if (ManagedSplines.IsEmpty())
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No splines (no input matches criteria or empty dataset)"));
			return false;
		}
	}
	else
	{
		TArray<FPCGTaggedData> Targets = InContext->InputData.GetInputsByPin(FName("Splines"));
		if (!Targets.IsEmpty())
		{
			for (const FPCGTaggedData& TaggedData : Targets)
			{
				const UPCGSplineData* SplineData = Cast<UPCGSplineData>(TaggedData.Data);
				if (!SplineData) { continue; }

				const bool bIsClosedLoop = SplineData->SplineStruct.bClosedLoop;
				if (SampleInputs == EPCGExSplineSamplingIncludeMode::ClosedLoopOnly && !bIsClosedLoop) { continue; }
				if (SampleInputs == EPCGExSplineSamplingIncludeMode::OpenSplineOnly && bIsClosedLoop) { continue; }

				Splines.Add(SplineData->SplineStruct);
			}
		}

		if (Splines.IsEmpty())
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No splines (no input matches criteria or empty dataset)"));
			return false;
		}
	}

	return true;
}

void UPCGExTensorSplineFactoryData::BeginDestroy()
{
	Super::BeginDestroy();
	ManagedSplines.Empty();
	Splines.Empty();
}

TArray<FPCGPinProperties> UPCGExTensorSplineFactoryProviderSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties = Super::InputPinProperties();
	if (GetBuildFromPoints())
	{
		PCGEX_PIN_POINTS(FName("Paths"), "Path data", Required, {})
	}
	else
	{
		PCGEX_PIN_POLYLINES(FName("Splines"), "Spline data", Required, {})
	}
	return PinProperties;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

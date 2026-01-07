// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Core/PCGExTensorSplineFactoryProvider.h"

#include "Data/PCGSplineData.h"
#include "Paths/PCGExPathsHelpers.h"

#define LOCTEXT_NAMESPACE "PCGExCreateTensor"
#define PCGEX_NAMESPACE CreateTensor


bool UPCGExTensorSplineFactoryData::InitInternalFacade(FPCGExContext* InContext)
{
	// TODO : Implement
	// TODO : Preload tangents attributes etc
	return true;
}

PCGExFactories::EPreparationResult UPCGExTensorSplineFactoryData::InitInternalData(FPCGExContext* InContext)
{
	PCGExFactories::EPreparationResult Result = Super::InitInternalData(InContext);
	if (Result != PCGExFactories::EPreparationResult::Success) { return Result; }

	if (bBuildFromPaths)
	{
		if (!InitInternalFacade(InContext)) { return PCGExFactories::EPreparationResult::Fail; }

		TArray<FPCGTaggedData> Targets = InContext->InputData.GetInputsByPin(PCGExPaths::Labels::SourcePathsLabel);

		if (!Targets.IsEmpty())
		{
			for (const FPCGTaggedData& TaggedData : Targets)
			{
				const UPCGBasePointData* PathData = Cast<UPCGBasePointData>(TaggedData.Data);
				if (!PathData) { continue; }

				const bool bIsClosedLoop = PCGExPaths::Helpers::GetClosedLoop(PathData);
				if (SampleInputs == EPCGExSplineSamplingIncludeMode::ClosedLoopOnly && !bIsClosedLoop) { continue; }
				if (SampleInputs == EPCGExSplineSamplingIncludeMode::OpenSplineOnly && bIsClosedLoop) { continue; }

				if (TSharedPtr<FPCGSplineStruct> SplineStruct = PCGExPaths::Helpers::MakeSplineFromPoints(PathData->GetConstTransformValueRange(), PointType, bIsClosedLoop, bSmoothLinear))
				{
					ManagedSplines.Add(SplineStruct);
				}
			}
		}

		if (ManagedSplines.IsEmpty())
		{
			PCGEX_LOG_MISSING_INPUT(InContext, FTEXT("No splines (no input matches criteria or empty dataset)"))
			return PCGExFactories::EPreparationResult::Fail;
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
				if (!SplineData || SplineData->SplineStruct.GetNumberOfSplineSegments() <= 0) { continue; }

				const bool bIsClosedLoop = SplineData->SplineStruct.bClosedLoop;
				if (SampleInputs == EPCGExSplineSamplingIncludeMode::ClosedLoopOnly && !bIsClosedLoop) { continue; }
				if (SampleInputs == EPCGExSplineSamplingIncludeMode::OpenSplineOnly && bIsClosedLoop) { continue; }

				Splines.Add(SplineData->SplineStruct);
			}
		}

		if (Splines.IsEmpty())
		{
			PCGEX_LOG_MISSING_INPUT(InContext, FTEXT("No splines (no input matches criteria or empty dataset)"))
			return PCGExFactories::EPreparationResult::Fail;
		}
	}

	return Result;
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
		PCGEX_PIN_POINTS(FName("Paths"), "Path data", Required)
	}
	else
	{
		PCGEX_PIN_POLYLINES(FName("Splines"), "Spline data", Required)
	}
	return PinProperties;
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

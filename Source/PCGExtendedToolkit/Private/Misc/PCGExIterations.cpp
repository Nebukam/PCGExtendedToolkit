// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExIterations.h"

#include "PCGGraph.h"
#include "PCGPin.h"
#include "Data/PCGSplineData.h"
#include "Data/PCGTextureData.h"

#define LOCTEXT_NAMESPACE "PCGExGraphSettings"
#define PCGEX_NAMESPACE Iterations

#pragma region UPCGSettings interface

TArray<FPCGPinProperties> UPCGExIterationsSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	return PinProperties;
}

TArray<FPCGPinProperties> UPCGExIterationsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;

	const FName OutputLabel = FName("Iterations");
	switch (Type)
	{
	default:
	case EPCGExIterationDataType::Any:
		PCGEX_PIN_ANY(OutputLabel, TEXT("Iterations."), Required, {})
		break;
	case EPCGExIterationDataType::Params:
		PCGEX_PIN_PARAMS(OutputLabel, TEXT("Iterations."), Required, {})
		break;
	case EPCGExIterationDataType::Points:
		PCGEX_PIN_POINTS(OutputLabel, TEXT("Iterations."), Required, {})
		break;
	case EPCGExIterationDataType::Spline:
		PCGEX_PIN_POLYLINES(OutputLabel, TEXT("Iterations."), Required, {})
		break;
	case EPCGExIterationDataType::Texture:
		PCGEX_PIN_TEXTURES(OutputLabel, TEXT("Iterations."), Required, {})
		break;
	}

	return PinProperties;
}

FPCGElementPtr UPCGExIterationsSettings::CreateElement() const { return MakeShared<FPCGExIterationsElement>(); }

#pragma endregion

FPCGContext* FPCGExIterationsElement::Initialize(
	const FPCGDataCollection& InputData,
	const TWeakObjectPtr<UPCGComponent> SourceComponent,
	const UPCGNode* Node)
{
	FPCGExContext* Context = new FPCGExContext();

	Context->InputData = InputData;
	Context->SourceComponent = SourceComponent;
	Context->Node = Node;

	return Context;
}

bool FPCGExIterationsElement::ExecuteInternal(FPCGContext* InContext) const
{
	PCGEX_CONTEXT()
	PCGEX_SETTINGS(Iterations)

	const FName OutputLabel = FName("Iterations");
	UPCGData* Data = nullptr;

	switch (Settings->Type)
	{
	default:
	case EPCGExIterationDataType::Params:
		Data = Context->ManagedObjects->New<UPCGParamData>();
		break;
	case EPCGExIterationDataType::Points:
		Data = Context->ManagedObjects->New<UPCGPointData>();
		break;
	case EPCGExIterationDataType::Spline:
		Data = Context->ManagedObjects->New<UPCGSplineData>();
		break;
	case EPCGExIterationDataType::Texture:
		Data = Context->ManagedObjects->New<UPCGTextureData>();
		break;
	}

	int32 NumIterations = FMath::Max(0, Settings->Iterations);
	Context->StagedOutputReserve(NumIterations);
	for (int i = 0; i < NumIterations; i++)
	{
		Context->StageOutput(OutputLabel, Data, {FString::Printf(TEXT("Iteration:%u"), i)}, false, false);
	}

	Context->Done();
	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

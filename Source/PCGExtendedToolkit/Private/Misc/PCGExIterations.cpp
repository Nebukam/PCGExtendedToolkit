﻿// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Misc/PCGExIterations.h"


#include "PCGExHelpers.h"
#include "PCGGraph.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGPointArrayData.h"
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

	FPCGPinProperties& Pin = PinProperties.Emplace_GetRef(FName("Iterations"));
	Pin.SetRequiredPin();

	switch (Type)
	{
	default:
	case EPCGExIterationDataType::Any:
		Pin.AllowedTypes = EPCGDataType::Any;
		break;
	case EPCGExIterationDataType::Params:
		Pin.AllowedTypes = EPCGDataType::Param;
		break;
	case EPCGExIterationDataType::Points:
		Pin.AllowedTypes = EPCGDataType::Point;
		break;
	case EPCGExIterationDataType::Spline:
		Pin.AllowedTypes = EPCGDataType::Spline;
		break;
	case EPCGExIterationDataType::Texture:
		Pin.AllowedTypes = EPCGDataType::BaseTexture;
		break;
	}
	return PinProperties;
}

FPCGElementPtr UPCGExIterationsSettings::CreateElement() const { return MakeShared<FPCGExIterationsElement>(); }

#pragma endregion

bool FPCGExIterationsElement::ExecuteInternal(FPCGContext* InContext) const
{
	PCGEX_CONTEXT()
	PCGEX_SETTINGS(Iterations)

	const FName OutputLabel = FName("Iterations");

	const int32 NumIterations = FMath::Max(0, Settings->Iterations);
	FString NumIterationsTag = FString::Printf(TEXT("NumIterations:%u"), NumIterations);
	Context->IncreaseStagedOutputReserve(NumIterations);

	if (Settings->bOutputUtils && Settings->Type == EPCGExIterationDataType::Params)
	{
		for (int i = 0; i < NumIterations; i++)
		{
			UPCGParamData* Data = Context->ManagedObjects->New<UPCGParamData>();
			UPCGMetadata* Metadata = Data->Metadata;

			Metadata->FindOrCreateAttribute<int32>(FName("Iteration"), i);
			Metadata->FindOrCreateAttribute<int32>(FName("NumIterations"), NumIterations);
			const double Progress = static_cast<double>(i) / static_cast<double>(NumIterations - 1);
			Metadata->FindOrCreateAttribute<double>(FName("OneMinusProgress"), 1 - Progress);
			Metadata->FindOrCreateAttribute<double>(FName("Progress"), Progress);

			Metadata->AddEntry();

			FPCGTaggedData& StagedData = Context->StageOutput(Data, false, false);
			StagedData.Pin = OutputLabel;
			StagedData.Tags.Add(FString::Printf(TEXT("Iteration:%u"), i));
			StagedData.Tags.Add(NumIterationsTag);
		}
	}
	else
	{
		UPCGData* Data = nullptr;

		switch (Settings->Type)
		{
		default:
		case EPCGExIterationDataType::Params:
			Data = Context->ManagedObjects->New<UPCGParamData>();
			break;
		case EPCGExIterationDataType::Points:
			Data = Context->ManagedObjects->New<UPCGPointArrayData>();
			break;
		case EPCGExIterationDataType::Spline:
			Data = Context->ManagedObjects->New<UPCGSplineData>();
			break;
		case EPCGExIterationDataType::Texture:
			Data = Context->ManagedObjects->New<UPCGTextureData>();
			break;
		}

		for (int i = 0; i < NumIterations; i++)
		{
			FPCGTaggedData& StagedData = Context->StageOutput(Data, false, false);
			StagedData.Pin = OutputLabel;
			StagedData.Tags.Add(FString::Printf(TEXT("Iteration:%u"), i));
			StagedData.Tags.Add(NumIterationsTag);
		}
	}

	Context->Done();
	return Context->TryComplete();
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

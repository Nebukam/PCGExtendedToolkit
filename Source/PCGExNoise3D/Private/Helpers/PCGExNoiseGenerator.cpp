// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Helpers/PCGExNoiseGenerator.h"
#include "Core/PCGExNoise3DFactoryProvider.h"
#include "Async/ParallelFor.h"
#include "Core/PCGExNoise3DOperation.h"

#define LOCTEXT_NAMESPACE "PCGExNoise3D"
#define PCGEX_NAMESPACE Noise3D

namespace PCGExNoise3D
{
	bool FNoiseGenerator::Init(FPCGExContext* InContext, const bool bThrowError)
	{
		TArray<TObjectPtr<const UPCGExNoise3DFactoryData>> Factories;
		if (!PCGExFactories::GetInputFactories(InContext, PCGExNoise3D::Labels::SourceNoise3DLabel, Factories, {PCGExFactories::EType::Noise3D}))
		{
			if (bThrowError)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("No noise factories found"));
			}
			return false;
		}

		const int32 Num = Factories.Num();
		Operations.Reserve(Num);
		OperationsPtr.Reserve(Num);
		Weights.Reserve(Num);
		BlendModes.Reserve(Num);
		TotalWeight = 0.0;

		for (const TObjectPtr<const UPCGExNoise3DFactoryData>& Factory : Factories)
		{
			TSharedPtr<FPCGExNoise3DOperation> Operation = Factory->CreateOperation(InContext);
			if (!Operation) { continue; }

			const double Weight = FMath::Max(0.0, Factory->ConfigBase.WeightFactor);

			Operations.Add(Operation);
			OperationsPtr.Add(Operation.Get());
			Weights.Add(Weight);
			BlendModes.Add(Operation->BlendMode);
			TotalWeight += Weight;
		}

		InvTotalWeight = TotalWeight > SMALL_NUMBER ? 1.0 / TotalWeight : 1.0;

		return Operations.Num() > 0;
	}

	//
	// Blend helpers
	//

	FORCEINLINE double FNoiseGenerator::BlendValues(const EPCGExNoiseBlendMode BlendMode, const double A, const double B, const double WeightA, const double WeightB) const
	{
		// Normalize to [0,1] for blend operations that expect it
		const double An = A * 0.5 + 0.5;
		const double Bn = B * 0.5 + 0.5 * WeightB;
		double Result;

		switch (BlendMode)
		{
		case EPCGExNoiseBlendMode::Blend:
			Result = (A * WeightA + B * WeightB) * InvTotalWeight;
			return Result;

		case EPCGExNoiseBlendMode::Add:
			Result = FMath::Clamp(A + B * WeightB, -1.0, 1.0);
			return Result;

		case EPCGExNoiseBlendMode::Multiply:
			Result = An * Bn;
			return Result * 2.0 - 1.0;

		case EPCGExNoiseBlendMode::Min:
			return FMath::Min(A, B);

		case EPCGExNoiseBlendMode::Max:
			return FMath::Max(A, B);

		case EPCGExNoiseBlendMode::Subtract:
			Result = FMath::Clamp(A - B * WeightB, -1.0, 1.0);
			return Result;

		case EPCGExNoiseBlendMode::Screen:
			Result = ScreenBlend(An, Bn);
			return Result * 2.0 - 1.0;

		case EPCGExNoiseBlendMode::Overlay:
			Result = OverlayBlend(An, Bn);
			return Result * 2.0 - 1.0;

		case EPCGExNoiseBlendMode::SoftLight:
			Result = SoftLightBlend(An, Bn);
			return Result * 2.0 - 1.0;

		case EPCGExNoiseBlendMode::First:
			return FMath::Abs(A) > SMALL_NUMBER ? A : B;

		default:
			return A;
		}
	}

	FORCEINLINE FVector2D FNoiseGenerator::BlendValues(const EPCGExNoiseBlendMode BlendMode, const FVector2D& A, const FVector2D& B, const double WeightA, const double WeightB) const
	{
		return FVector2D(
			BlendValues(BlendMode, A.X, B.X, WeightA, WeightB),
			BlendValues(BlendMode, A.Y, B.Y, WeightA, WeightB)
		);
	}

	FORCEINLINE FVector FNoiseGenerator::BlendValues(const EPCGExNoiseBlendMode BlendMode, const FVector& A, const FVector& B, const double WeightA, const double WeightB) const
	{
		return FVector(
			BlendValues(BlendMode, A.X, B.X, WeightA, WeightB),
			BlendValues(BlendMode, A.Y, B.Y, WeightA, WeightB),
			BlendValues(BlendMode, A.Z, B.Z, WeightA, WeightB)
		);
	}

	FORCEINLINE FVector4 FNoiseGenerator::BlendValues(const EPCGExNoiseBlendMode BlendMode, const FVector4& A, const FVector4& B, const double WeightA, const double WeightB) const
	{
		return FVector4(
			BlendValues(BlendMode, A.X, B.X, WeightA, WeightB),
			BlendValues(BlendMode, A.Y, B.Y, WeightA, WeightB),
			BlendValues(BlendMode, A.Z, B.Z, WeightA, WeightB),
			BlendValues(BlendMode, A.W, B.W, WeightA, WeightB)
		);
	}

	//
	// Single-point generation
	//

	double FNoiseGenerator::GetDouble(const FVector& Position) const
	{
		if (Operations.IsEmpty()) { return 0.0; }

		double Result = OperationsPtr[0]->GetDouble(Position);
		double AccumWeight = Weights[0];

		for (int32 i = 1; i < Operations.Num(); ++i)
		{
			const double Value = OperationsPtr[i]->GetDouble(Position);
			Result = BlendValues(BlendModes[i], Result, Value, AccumWeight, Weights[i]);
			AccumWeight += Weights[i];
		}

		return Result;
	}

	FVector2D FNoiseGenerator::GetVector2D(const FVector& Position) const
	{
		if (Operations.IsEmpty()) { return FVector2D::ZeroVector; }

		FVector2D Result = OperationsPtr[0]->GetVector2D(Position);
		double AccumWeight = Weights[0];

		for (int32 i = 1; i < Operations.Num(); ++i)
		{
			const FVector2D Value = OperationsPtr[i]->GetVector2D(Position);
			Result = BlendValues(BlendModes[i], Result, Value, AccumWeight, Weights[i]);
			AccumWeight += Weights[i];
		}

		return Result;
	}

	FVector FNoiseGenerator::GetVector(const FVector& Position) const
	{
		if (Operations.IsEmpty()) { return FVector::ZeroVector; }

		FVector Result = OperationsPtr[0]->GetVector(Position);
		double AccumWeight = Weights[0];

		for (int32 i = 1; i < Operations.Num(); ++i)
		{
			const FVector Value = OperationsPtr[i]->GetVector(Position);
			Result = BlendValues(BlendModes[i], Result, Value, AccumWeight, Weights[i]);
			AccumWeight += Weights[i];
		}

		return Result;
	}

	FVector4 FNoiseGenerator::GetVector4(const FVector& Position) const
	{
		if (Operations.IsEmpty()) { return FVector4::Zero(); }

		FVector4 Result = OperationsPtr[0]->GetVector4(Position);
		double AccumWeight = Weights[0];

		for (int32 i = 1; i < Operations.Num(); ++i)
		{
			const FVector4 Value = OperationsPtr[i]->GetVector4(Position);
			Result = BlendValues(BlendModes[i], Result, Value, AccumWeight, Weights[i]);
			AccumWeight += Weights[i];
		}

		return Result;
	}

	//
	// Batch generation
	//

	void FNoiseGenerator::Generate(const TArrayView<const FVector> Positions, TArrayView<double> OutResults) const
	{
		check(Positions.Num() == OutResults.Num());

		if (Operations.IsEmpty())
		{
			FMemory::Memzero(OutResults.GetData(), OutResults.Num() * sizeof(double));
			return;
		}

		const int32 Count = Positions.Num();

		// Fast path for single operation
		if (Operations.Num() == 1)
		{
			OperationsPtr[0]->Generate(Positions, OutResults);
			return;
		}

		// Multiple operations - need temp storage
		TArray<double> TempBuffer;
		TempBuffer.SetNumUninitialized(Count);

		// First operation
		OperationsPtr[0]->Generate(Positions, OutResults);
		double AccumWeight = Weights[0];

		// Blend subsequent operations
		for (int32 OpIdx = 1; OpIdx < Operations.Num(); ++OpIdx)
		{
			OperationsPtr[OpIdx]->Generate(Positions, TempBuffer);
			const double OpWeight = Weights[OpIdx];
			const EPCGExNoiseBlendMode BlendMode = BlendModes[OpIdx];

			for (int32 i = 0; i < Count; ++i)
			{
				OutResults[i] = BlendValues(BlendMode, OutResults[i], TempBuffer[i], AccumWeight, OpWeight);
			}
			AccumWeight += OpWeight;
		}
	}

	void FNoiseGenerator::Generate(const TArrayView<const FVector> Positions, TArrayView<FVector2D> OutResults) const
	{
		check(Positions.Num() == OutResults.Num());

		if (Operations.IsEmpty())
		{
			FMemory::Memzero(OutResults.GetData(), OutResults.Num() * sizeof(FVector2D));
			return;
		}

		const int32 Count = Positions.Num();

		if (Operations.Num() == 1)
		{
			OperationsPtr[0]->Generate(Positions, OutResults);
			return;
		}

		TArray<FVector2D> TempBuffer;
		TempBuffer.SetNumUninitialized(Count);

		OperationsPtr[0]->Generate(Positions, OutResults);
		double AccumWeight = Weights[0];

		for (int32 OpIdx = 1; OpIdx < Operations.Num(); ++OpIdx)
		{
			OperationsPtr[OpIdx]->Generate(Positions, TempBuffer);
			const double OpWeight = Weights[OpIdx];
			const EPCGExNoiseBlendMode BlendMode = BlendModes[OpIdx];

			for (int32 i = 0; i < Count; ++i)
			{
				OutResults[i] = BlendValues(BlendMode, OutResults[i], TempBuffer[i], AccumWeight, OpWeight);
			}
			AccumWeight += OpWeight;
		}
	}

	void FNoiseGenerator::Generate(const TArrayView<const FVector> Positions, TArrayView<FVector> OutResults) const
	{
		check(Positions.Num() == OutResults.Num());

		if (Operations.IsEmpty())
		{
			FMemory::Memzero(OutResults.GetData(), OutResults.Num() * sizeof(FVector));
			return;
		}

		const int32 Count = Positions.Num();

		if (Operations.Num() == 1)
		{
			OperationsPtr[0]->Generate(Positions, OutResults);
			return;
		}

		TArray<FVector> TempBuffer;
		TempBuffer.SetNumUninitialized(Count);

		OperationsPtr[0]->Generate(Positions, OutResults);
		double AccumWeight = Weights[0];

		for (int32 OpIdx = 1; OpIdx < Operations.Num(); ++OpIdx)
		{
			OperationsPtr[OpIdx]->Generate(Positions, TempBuffer);
			const double OpWeight = Weights[OpIdx];
			const EPCGExNoiseBlendMode BlendMode = BlendModes[OpIdx];

			for (int32 i = 0; i < Count; ++i)
			{
				OutResults[i] = BlendValues(BlendMode, OutResults[i], TempBuffer[i], AccumWeight, OpWeight);
			}
			AccumWeight += OpWeight;
		}
	}

	void FNoiseGenerator::Generate(const TArrayView<const FVector> Positions, TArrayView<FVector4> OutResults) const
	{
		check(Positions.Num() == OutResults.Num());

		if (Operations.IsEmpty())
		{
			FMemory::Memzero(OutResults.GetData(), OutResults.Num() * sizeof(FVector4));
			return;
		}

		const int32 Count = Positions.Num();

		if (Operations.Num() == 1)
		{
			OperationsPtr[0]->Generate(Positions, OutResults);
			return;
		}

		TArray<FVector4> TempBuffer;
		TempBuffer.SetNumUninitialized(Count);

		OperationsPtr[0]->Generate(Positions, OutResults);
		double AccumWeight = Weights[0];

		for (int32 OpIdx = 1; OpIdx < Operations.Num(); ++OpIdx)
		{
			OperationsPtr[OpIdx]->Generate(Positions, TempBuffer);
			const double OpWeight = Weights[OpIdx];
			const EPCGExNoiseBlendMode BlendMode = BlendModes[OpIdx];

			for (int32 i = 0; i < Count; ++i)
			{
				OutResults[i] = BlendValues(BlendMode, OutResults[i], TempBuffer[i], AccumWeight, OpWeight);
			}
			AccumWeight += OpWeight;
		}
	}

	//
	// Parallel batch generation
	//

	void FNoiseGenerator::GenerateParallel(const TArrayView<const FVector> Positions, TArrayView<double> OutResults, const int32 MinBatchSize) const
	{
		check(Positions.Num() == OutResults.Num());

		const int32 Count = Positions.Num();
		if (Count < MinBatchSize * 2 || Operations.IsEmpty())
		{
			Generate(Positions, OutResults);
			return;
		}

		ParallelFor(Count, [&](const int32 Index)
		{
			OutResults[Index] = GetDouble(Positions[Index]);
		}, Count < MinBatchSize);
	}

	void FNoiseGenerator::GenerateParallel(const TArrayView<const FVector> Positions, TArrayView<FVector2D> OutResults, const int32 MinBatchSize) const
	{
		check(Positions.Num() == OutResults.Num());

		const int32 Count = Positions.Num();
		if (Count < MinBatchSize * 2 || Operations.IsEmpty())
		{
			Generate(Positions, OutResults);
			return;
		}

		ParallelFor(Count, [&](const int32 Index)
		{
			OutResults[Index] = GetVector2D(Positions[Index]);
		}, Count < MinBatchSize);
	}

	void FNoiseGenerator::GenerateParallel(const TArrayView<const FVector> Positions, TArrayView<FVector> OutResults, const int32 MinBatchSize) const
	{
		check(Positions.Num() == OutResults.Num());

		const int32 Count = Positions.Num();
		if (Count < MinBatchSize * 2 || Operations.IsEmpty())
		{
			Generate(Positions, OutResults);
			return;
		}

		ParallelFor(Count, [&](const int32 Index)
		{
			OutResults[Index] = GetVector(Positions[Index]);
		}, Count < MinBatchSize);
	}

	void FNoiseGenerator::GenerateParallel(const TArrayView<const FVector> Positions, TArrayView<FVector4> OutResults, const int32 MinBatchSize) const
	{
		check(Positions.Num() == OutResults.Num());

		const int32 Count = Positions.Num();
		if (Count < MinBatchSize * 2 || Operations.IsEmpty())
		{
			Generate(Positions, OutResults);
			return;
		}

		ParallelFor(Count, [&](const int32 Index)
		{
			OutResults[Index] = GetVector4(Positions[Index]);
		}, Count < MinBatchSize);
	}
}

#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

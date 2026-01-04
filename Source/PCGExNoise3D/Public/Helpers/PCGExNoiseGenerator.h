// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Core/PCGExNoise3DCommon.h"

class FPCGExNoise3DOperation;
struct FPCGExContext;

namespace PCGExNoise3D
{
	/**
	 * Noise generator that combines multiple noise operations
	 * Thread-safe after initialization
	 */
	class PCGEXNOISE3D_API FNoiseGenerator : public TSharedFromThis<FNoiseGenerator>
	{
	protected:
		TArray<TSharedPtr<FPCGExNoise3DOperation>> Operations;
		TArray<const FPCGExNoise3DOperation*> OperationsPtr;
		TArray<double> Weights;
		TArray<EPCGExNoiseBlendMode> BlendModes;
		double TotalWeight = 0.0;
		double InvTotalWeight = 1.0;

	public:
		FNoiseGenerator() = default;

		/**
		 * Initialize from factories in context
		 * @param InContext PCG context containing factory inputs
		 * @param bThrowError Whether to log errors if no factories found
		 * @return true if at least one operation was created
		 */
		bool Init(FPCGExContext* InContext, bool bThrowError = true);

		/**
		 * Get number of operations
		 */
		int32 Num() const { return Operations.Num(); }

		/**
		 * Check if generator has any operations
		 */
		bool IsValid() const { return Operations.Num() > 0; }

		//
		// Single-point generation
		//

		double GetDouble(const FVector& Position) const;
		FVector2D GetVector2D(const FVector& Position) const;
		FVector GetVector(const FVector& Position) const;
		FVector4 GetVector4(const FVector& Position) const;

		//
		// Batch generation (optimized for parallel processing)
		//

		void Generate(TArrayView<const FVector> Positions, TArrayView<double> OutResults) const;
		void Generate(TArrayView<const FVector> Positions, TArrayView<FVector2D> OutResults) const;
		void Generate(TArrayView<const FVector> Positions, TArrayView<FVector> OutResults) const;
		void Generate(TArrayView<const FVector> Positions, TArrayView<FVector4> OutResults) const;

		//
		// Parallel batch generation
		//

		void GenerateParallel(TArrayView<const FVector> Positions, TArrayView<double> OutResults, int32 MinBatchSize = 256) const;
		void GenerateParallel(TArrayView<const FVector> Positions, TArrayView<FVector2D> OutResults, int32 MinBatchSize = 256) const;
		void GenerateParallel(TArrayView<const FVector> Positions, TArrayView<FVector> OutResults, int32 MinBatchSize = 256) const;
		void GenerateParallel(TArrayView<const FVector> Positions, TArrayView<FVector4> OutResults, int32 MinBatchSize = 256) const;

	private:
		//
		// Blend helpers
		//

		FORCEINLINE double BlendValues(const EPCGExNoiseBlendMode BlendMode, double A, double B, double WeightA, double WeightB) const;
		FORCEINLINE FVector2D BlendValues(const EPCGExNoiseBlendMode BlendMode, const FVector2D& A, const FVector2D& B, double WeightA, double WeightB) const;
		FORCEINLINE FVector BlendValues(const EPCGExNoiseBlendMode BlendMode, const FVector& A, const FVector& B, double WeightA, double WeightB) const;
		FORCEINLINE FVector4 BlendValues(const EPCGExNoiseBlendMode BlendMode, const FVector4& A, const FVector4& B, double WeightA, double WeightB) const;

		/** Screen blend helper */
		FORCEINLINE double ScreenBlend(double A, double B) const
		{
			return 1.0 - (1.0 - A) * (1.0 - B);
		}

		/** Overlay blend helper */
		FORCEINLINE double OverlayBlend(double A, double B) const
		{
			return A < 0.5 ? 2.0 * A * B : 1.0 - 2.0 * (1.0 - A) * (1.0 - B);
		}

		/** Soft light blend helper */
		FORCEINLINE double SoftLightBlend(double A, double B) const
		{
			return B < 0.5 ? A - (1.0 - 2.0 * B) * A * (1.0 - A) : A + (2.0 * B - 1.0) * (((A - 0.5) * 2.0) - A * (1.0 - A));
		}
	};
}

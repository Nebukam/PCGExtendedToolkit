// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataBlending.h"

namespace PCGExDataBlending
{
	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingAverage final : public FDataBlendingOperation<T>
	{
	public:
		virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::Average; };
		virtual bool GetIsInterpolation() const override { return true; }
		virtual bool GetRequiresPreparation() const override { return true; }
		virtual bool GetRequiresFinalization() const override { return true; }

		FORCEINLINE virtual void SinglePrepare(T& A) const override { A = this->Writer->GetZeroedValue(); }
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Add(A, B); }
		FORCEINLINE virtual void SingleFinalize(T& A, const int32 Count, const double Weight) const override { A = PCGExMath::Div(A, static_cast<double>(Count)); }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingCopy final : public FDataBlendingOperation<T>
	{
	public:
		virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::Copy; };
		virtual T SingleOperation(T A, T B, double Weight) const override { return B; }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingSum final : public FDataBlendingOperation<T>
	{
	public:
		virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::Sum; };
		virtual bool GetIsInterpolation() const override { return true; }
		virtual bool GetRequiresPreparation() const override { return true; }
		virtual bool GetRequiresFinalization() const override { return false; }

		FORCEINLINE virtual void SinglePrepare(T& A) const override { A = this->Writer->GetZeroedValue(); }
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Add(A, B); }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingMax final : public FDataBlendingOperationWithScratchCheck<T>
	{
	public:
		virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::Max; };
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Max(A, B); }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingMin final : public FDataBlendingOperationWithScratchCheck<T>
	{
	public:
		virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::Min; };
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Min(A, B); }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingWeight final : public FDataBlendingOperation<T>
	{
	public:
		virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::Weight; };
		virtual bool GetIsInterpolation() const override { return true; }
		virtual bool GetRequiresPreparation() const override { return true; }
		virtual bool GetRequiresFinalization() const override { return true; }

		FORCEINLINE virtual void SinglePrepare(T& A) const override { A = this->Writer->GetZeroedValue(); }
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::WeightedAdd(A, B, Weight); } // PCGExMath::Lerp(A, B, Alpha); }
		FORCEINLINE virtual void SingleFinalize(T& A, const int32 Count, const double Weight) const override { A = PCGExMath::Div(A, Weight); }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingWeightedSum final : public FDataBlendingOperation<T>
	{
	public:
		virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::WeightedSum; };
		virtual bool GetIsInterpolation() const override { return true; }
		virtual bool GetRequiresPreparation() const override { return true; }
		virtual bool GetRequiresFinalization() const override { return false; }

		FORCEINLINE virtual void SinglePrepare(T& A) const override { A = this->Writer->GetZeroedValue(); }
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::WeightedAdd(A, B, Weight); }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingLerp final : public FDataBlendingOperation<T>
	{
	public:
		virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::Lerp; };
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Lerp(A, B, Weight); }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingNone final : public FDataBlendingOperationWithScratchCheck<T>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return A; }
	};
}

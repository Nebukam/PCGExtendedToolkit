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
		virtual bool GetIsInterpolation() const override { return true; }
		virtual bool GetRequiresPreparation() const override { return true; }
		virtual bool GetRequiresFinalization() const override { return true; }

		virtual void SinglePrepare(T& A) const override { A = this->Writer->GetDefaultValue(); }
		virtual T SingleOperation(T A, T B, double Alpha) const override { return PCGExMath::Add(A, B); }
		virtual void SingleFinalize(T& A, double Alpha) const override { A = PCGExMath::Div(A, Alpha); }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingCopy final : public FDataBlendingOperation<T>
	{
	public:
		virtual T SingleOperation(T A, T B, double Alpha) const override { return B; }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingSum final : public FDataBlendingOperation<T>
	{
	public:
		virtual bool GetIsInterpolation() const override { return true; }
		virtual bool GetRequiresPreparation() const override { return true; }
		virtual bool GetRequiresFinalization() const override { return false; }

		virtual void SinglePrepare(T& A) const override { A = this->Writer->GetDefaultValue(); }
		virtual T SingleOperation(T A, T B, double Alpha) const override { return PCGExMath::Add(A, B); }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingWeightedSum final : public FDataBlendingOperation<T>
	{
	public:
		virtual bool GetIsInterpolation() const override { return true; }
		virtual bool GetRequiresPreparation() const override { return true; }
		virtual bool GetRequiresFinalization() const override { return false; }

		virtual void SinglePrepare(T& A) const override { A = this->Writer->GetDefaultValue(); }
		virtual T SingleOperation(T A, T B, double Alpha) const override { return PCGExMath::WeightedAdd(A, B, Alpha); }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingMax final : public FDataBlendingOperation<T>
	{
	public:
		virtual T SingleOperation(T A, T B, double Alpha) const override { return PCGExMath::Max(A, B); }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingMin final : public FDataBlendingOperation<T>
	{
	public:
		virtual T SingleOperation(T A, T B, double Alpha) const override { return PCGExMath::Min(A, B); }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingWeight final : public FDataBlendingOperation<T>
	{
	public:
		virtual bool GetIsInterpolation() const override { return true; }
		virtual bool GetRequiresPreparation() const override { return true; }
		virtual bool GetRequiresFinalization() const override { return true; }

		virtual void SinglePrepare(T& A) const override { A = this->Writer->GetDefaultValue(); }
		virtual T SingleOperation(T A, T B, double Alpha) const override { return PCGExMath::WeightedAdd(A, B, Alpha); } // PCGExMath::Lerp(A, B, Alpha); }
		virtual void SingleFinalize(T& A, double Alpha) const override { A = PCGExMath::Div(A, Alpha); }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingNone final : public FDataBlendingOperation<T>
	{
	public:
		virtual T SingleOperation(T A, T B, double Alpha) const override { return A; }
	};
}

// Copyright Timothé Lapetite 2023
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataBlending.h"
#include "Data/PCGExData.h"

namespace PCGExDataBlending
{
	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingAverage final : public FDataBlendingOperation<T>
	{
	public:
		virtual bool GetRequiresPreparation() const override { return true; }
		virtual bool GetRequiresFinalization() const override { return true; }

		virtual void SinglePrepare(T& A) const override { A = this->PrimaryAccessor->GetDefaultValue(); }
		virtual T SingleOperation(T A, T B, double Alpha) const override { return PCGExDataBlending::Add(A, B); }
		virtual void SingleFinalize(T& A, double Alpha) const override { A = PCGExDataBlending::Div(A, Alpha); }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingCopy final : public FDataBlendingOperation<T>
	{
	public:
		virtual T SingleOperation(T A, T B, double Alpha) const override { return B; }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingMax final : public FDataBlendingOperation<T>
	{
	public:
		virtual T SingleOperation(T A, T B, double Alpha) const override { return PCGExDataBlending::Max(A, B); }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingMin final : public FDataBlendingOperation<T>
	{
	public:
		virtual T SingleOperation(T A, T B, double Alpha) const override { return PCGExDataBlending::Min(A, B); }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingWeight final : public FDataBlendingOperation<T>
	{
	public:
		virtual T SingleOperation(T A, T B, double Alpha) const override { return PCGExDataBlending::Lerp(A, B, Alpha); }
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingNone final : public FDataBlendingOperation<T>
	{
	public:
		virtual T SingleOperation(T A, T B, double Alpha) const override { return B; }
	};
}

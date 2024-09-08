// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataBlending.h"

namespace PCGExDataBlending
{
	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingAverage final : public TDataBlendingOperation<T>
	{
	public:
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::Average; };
		FORCEINLINE virtual bool GetIsInterpolation() const override { return true; }
		FORCEINLINE virtual bool GetRequiresPreparation() const override { return true; }
		FORCEINLINE virtual bool GetRequiresFinalization() const override { return true; }

		FORCEINLINE virtual void SinglePrepare(T& A) const override { A = this->Writer->GetZeroedValue(); }
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Add(A, B); }
		FORCEINLINE virtual void SingleFinalize(T& A, const int32 Count, const double Weight) const override { A = PCGExMath::Div(A, static_cast<double>(Count)); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingCopy final : public TDataBlendingOperation<T>
	{
	public:
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::Copy; };
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return B; }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingAdd final : public TDataBlendingOperation<T>
	{
	public:
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::Add; };
		FORCEINLINE virtual bool GetIsInterpolation() const override { return true; }
		FORCEINLINE virtual bool GetRequiresPreparation() const override { return true; }
		FORCEINLINE virtual bool GetRequiresFinalization() const override { return false; }

		FORCEINLINE virtual void SinglePrepare(T& A) const override { A = this->Writer->GetZeroedValue(); }
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Add(A, B); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingSubtract final : public TDataBlendingOperation<T>
	{
	public:
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::Subtract; };
		FORCEINLINE virtual bool GetIsInterpolation() const override { return true; }
		FORCEINLINE virtual bool GetRequiresPreparation() const override { return true; }
		FORCEINLINE virtual bool GetRequiresFinalization() const override { return false; }

		FORCEINLINE virtual void SinglePrepare(T& A) const override { A = this->Writer->GetZeroedValue(); }
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Subtract(A, B); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingMax final : public FDataBlendingOperationWithFirstInit<T>
	{
	public:
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::Max; };
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Max(A, B); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingMin final : public FDataBlendingOperationWithFirstInit<T>
	{
	public:
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::Min; };
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Min(A, B); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingWeight final : public TDataBlendingOperation<T>
	{
	public:
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::Weight; };
		FORCEINLINE virtual bool GetIsInterpolation() const override { return true; }
		FORCEINLINE virtual bool GetRequiresPreparation() const override { return true; }
		FORCEINLINE virtual bool GetRequiresFinalization() const override { return true; }

		FORCEINLINE virtual void SinglePrepare(T& A) const override { A = this->Writer->GetZeroedValue(); }
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::WeightedAdd(A, B, Weight); } // PCGExMath::Lerp(A, B, Alpha); }
		FORCEINLINE virtual void SingleFinalize(T& A, const int32 Count, const double Weight) const override { A = PCGExMath::Div(A, Weight); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingWeightedAdd final : public TDataBlendingOperation<T>
	{
	public:
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::WeightedAdd; };
		FORCEINLINE virtual bool GetIsInterpolation() const override { return true; }
		FORCEINLINE virtual bool GetRequiresPreparation() const override { return true; }
		FORCEINLINE virtual bool GetRequiresFinalization() const override { return false; }

		FORCEINLINE virtual void SinglePrepare(T& A) const override { A = this->Writer->GetZeroedValue(); }
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::WeightedAdd(A, B, Weight); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingLerp final : public TDataBlendingOperation<T>
	{
	public:
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::Lerp; };
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Lerp(A, B, Weight); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingNone final : public FDataBlendingOperationWithFirstInit<T>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return A; }
	};

	static FDataBlendingOperationBase* CreateOperation(const EPCGExDataBlendingType Type, const PCGEx::FAttributeIdentity& Identity)
	{
#define PCGEX_SAO_NEW(_TYPE, _NAME, _ID) case EPCGMetadataTypes::_NAME : NewOperation = new TDataBlending##_ID<_TYPE>(); break;
#define PCGEX_BLEND_CASE(_ID) case EPCGExDataBlendingType::_ID: switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_NEW, _ID) } break;
#define PCGEX_FOREACH_BLEND(MACRO)\
PCGEX_BLEND_CASE(None)\
PCGEX_BLEND_CASE(Copy)\
PCGEX_BLEND_CASE(Average)\
PCGEX_BLEND_CASE(Weight)\
PCGEX_BLEND_CASE(WeightedAdd)\
PCGEX_BLEND_CASE(Min)\
PCGEX_BLEND_CASE(Max)\
PCGEX_BLEND_CASE(Add)\
PCGEX_BLEND_CASE(Lerp)

		FDataBlendingOperationBase* NewOperation = nullptr;

		switch (Type)
		{
		default:
		PCGEX_FOREACH_BLEND(PCGEX_BLEND_CASE)
		}

		if (NewOperation) { NewOperation->SetAttributeName(Identity.Name); }
		return NewOperation;

#undef PCGEX_SAO_NEW
#undef PCGEX_BLEND_CASE
#undef PCGEX_FOREACH_BLEND
	}
}

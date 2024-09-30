// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataBlending.h"

#define PCGEX_FOREACH_BLEND(MACRO)\
PCGEX_BLEND_CASE(None)\
PCGEX_BLEND_CASE(Copy)\
PCGEX_BLEND_CASE(Average)\
PCGEX_BLEND_CASE(Weight)\
PCGEX_BLEND_CASE(WeightedSum)\
PCGEX_BLEND_CASE(Min)\
PCGEX_BLEND_CASE(Max)\
PCGEX_BLEND_CASE(Sum)\
PCGEX_BLEND_CASE(Lerp)\
PCGEX_BLEND_CASE(UnsignedMin)\
PCGEX_BLEND_CASE(UnsignedMax)\
PCGEX_BLEND_CASE(AbsoluteMin)\
PCGEX_BLEND_CASE(AbsoluteMax)\
PCGEX_BLEND_CASE(WeightedSubtract)

namespace PCGExDataBlending
{
	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingAverage final : public TDataBlendingOperation<T>
	{
	public:
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::Average; };
		FORCEINLINE virtual bool GetRequiresPreparation() const override { return true; }
		FORCEINLINE virtual bool GetRequiresFinalization() const override { return true; }

		FORCEINLINE virtual void SinglePrepare(T& A) const override { A = T{}; }
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
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingSum final : public TDataBlendingOperation<T>
	{
	public:
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::Sum; };
		FORCEINLINE virtual bool GetRequiresPreparation() const override { return true; }
		FORCEINLINE virtual bool GetRequiresFinalization() const override { return false; }

		FORCEINLINE virtual void SinglePrepare(T& A) const override { A = T{}; }
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Add(A, B); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingSubtract final : public TDataBlendingOperation<T>
	{
	public:
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::Subtract; };
		FORCEINLINE virtual bool GetRequiresPreparation() const override { return true; }
		FORCEINLINE virtual bool GetRequiresFinalization() const override { return false; }

		FORCEINLINE virtual void SinglePrepare(T& A) const override { A = T{}; }
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Sub(A, B); }
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
		FORCEINLINE virtual bool GetRequiresPreparation() const override { return true; }
		FORCEINLINE virtual bool GetRequiresFinalization() const override { return true; }

		FORCEINLINE virtual void SinglePrepare(T& A) const override { A = A = T{}; }
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::WeightedAdd(A, B, Weight); } // PCGExMath::Lerp(A, B, Alpha); }
		FORCEINLINE virtual void SingleFinalize(T& A, const int32 Count, const double Weight) const override { A = PCGExMath::Div(A, Weight); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingWeightedSum final : public TDataBlendingOperation<T>
	{
	public:
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::WeightedSum; };
		FORCEINLINE virtual bool GetRequiresPreparation() const override { return true; }
		FORCEINLINE virtual bool GetRequiresFinalization() const override { return false; }

		FORCEINLINE virtual void SinglePrepare(T& A) const override { A = T{}; }
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

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingUnsignedMax final : public FDataBlendingOperationWithFirstInit<T>
	{
	public:
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::UnsignedMax; };
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::UnsignedMax(A, B); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingUnsignedMin final : public FDataBlendingOperationWithFirstInit<T>
	{
	public:
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::UnsignedMin; };
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::UnsignedMin(A, B); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingAbsoluteMax final : public FDataBlendingOperationWithFirstInit<T>
	{
	public:
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::AbsoluteMax; };
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::AbsoluteMax(A, B); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingAbsoluteMin final : public FDataBlendingOperationWithFirstInit<T>
	{
	public:
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::AbsoluteMin; };
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::AbsoluteMin(A, B); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingWeightedSubtract final : public TDataBlendingOperation<T>
	{
	public:
		FORCEINLINE virtual EPCGExDataBlendingType GetBlendingType() const override { return EPCGExDataBlendingType::WeightedSubtract; };
		FORCEINLINE virtual bool GetRequiresPreparation() const override { return true; }
		FORCEINLINE virtual bool GetRequiresFinalization() const override { return false; }

		FORCEINLINE virtual void SinglePrepare(T& A) const override { A = T{}; }
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::WeightedSub(A, B, Weight); }
	};

	static TSharedPtr<FDataBlendingOperationBase> CreateOperation(const EPCGExDataBlendingType Type, const PCGEx::FAttributeIdentity& Identity)
	{
#define PCGEX_SAO_NEW(_TYPE, _NAME, _ID) case EPCGMetadataTypes::_NAME : NewOperation = MakeShared<TDataBlending##_ID<_TYPE>>(); break;
#define PCGEX_BLEND_CASE(_ID) case EPCGExDataBlendingType::_ID: switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_NEW, _ID) } break;

		TSharedPtr<FDataBlendingOperationBase> NewOperation;

		switch (Type)
		{
		default:
		PCGEX_FOREACH_BLEND(PCGEX_BLEND_CASE)
		}

		if (NewOperation) { NewOperation->SetAttributeName(Identity.Name); }
		return NewOperation;

#undef PCGEX_SAO_NEW
#undef PCGEX_BLEND_CASE
	}

	static TSharedPtr<FDataBlendingOperationBase> CreateOperationWithDefaults(const EPCGExDataBlendingType DefaultType, const PCGEx::FAttributeIdentity& Identity)
	{
		EPCGExDataBlendingTypeDefault GlobalDefaultType = EPCGExDataBlendingTypeDefault::Default;

#define PCGEX_DEF_SET(_TYPE, _NAME, _ID) case EPCGMetadataTypes::_NAME : GlobalDefaultType = GetDefault<UPCGExGlobalSettings>()->Default##_NAME##BlendMode; break;

		switch (Identity.UnderlyingType)
		{
		default:
			break;
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_DEF_SET)
		}

		if (GlobalDefaultType == EPCGExDataBlendingTypeDefault::Default) { return CreateOperation(DefaultType, Identity); }
		return CreateOperation(static_cast<EPCGExDataBlendingType>(static_cast<uint8>(GlobalDefaultType)), Identity);

#undef PCGEX_DEF_SET
#undef PCGEX_BLEND_CASE
	}


	static TSharedPtr<FDataBlendingOperationBase> CreateOperation(const EPCGExDataBlendingType* Type, const EPCGExDataBlendingType DefaultType, const PCGEx::FAttributeIdentity& Identity)
	{
		return Type ? CreateOperation(*Type, Identity) : CreateOperationWithDefaults(DefaultType, Identity);
	}

#undef PCGEX_FOREACH_BLEND
}

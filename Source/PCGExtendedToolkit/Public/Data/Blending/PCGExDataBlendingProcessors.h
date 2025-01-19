// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataBlending.h"

#define PCGEX_FOREACH_BLEND(MACRO)\
PCGEX_FOREACH_BLENDMODE(PCGEX_BLEND_CASE)

namespace PCGExDataBlending
{
	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingAverage final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Average, true, true>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Add(A, B); }
		FORCEINLINE virtual void SingleComplete(T& A, const int32 Count, const double Weight) const override { A = PCGExMath::Div(A, static_cast<double>(Count)); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingCopy final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Copy>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return B; }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingCopyOther final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Copy>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return A; }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingSum final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Sum, true, false>
	{
	public:
		FORCEINLINE virtual void SinglePrepare(T& A) const override { A = T{}; }
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Add(A, B); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingSubtract final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Subtract, true, false>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Sub(A, B); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingMax final : public FDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::Max>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Max(A, B); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingMin final : public FDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::Min>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Min(A, B); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingWeight final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Weight, true, true>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::WeightedAdd(A, B, Weight); } // PCGExMath::Lerp(A, B, Alpha); }
		FORCEINLINE virtual void SingleComplete(T& A, const int32 Count, const double Weight) const override { A = PCGExMath::Div(A, Weight); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingWeightedSum final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::WeightedSum, true, false>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::WeightedAdd(A, B, Weight); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingLerp final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Lerp>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::Lerp(A, B, Weight); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingNone final : public FDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::None>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return A; }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingUnsignedMax final : public FDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::UnsignedMax>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::UnsignedMax(A, B); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingUnsignedMin final : public FDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::UnsignedMin>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::UnsignedMin(A, B); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingAbsoluteMax final : public FDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::AbsoluteMax>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::AbsoluteMax(A, B); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingAbsoluteMin final : public FDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::AbsoluteMin>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::AbsoluteMin(A, B); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingWeightedSubtract final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::WeightedSubtract>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::WeightedSub(A, B, Weight); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingHash final : public FDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::Hash>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::NaiveHash(A, B); }
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingUnsignedHash final : public FDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::UnsignedHash>
	{
	public:
		FORCEINLINE virtual T SingleOperation(T A, T B, double Weight) const override { return PCGExMath::NaiveUnsignedHash(A, B); }
	};

	static TSharedPtr<FDataBlendingProcessorBase> CreateProcessor(const EPCGExDataBlendingType Type, const PCGEx::FAttributeIdentity& Identity)
	{
#define PCGEX_SAO_NEW(_TYPE, _NAME, _ID) case EPCGMetadataTypes::_NAME : NewProcessor = MakeShared<TDataBlending##_ID<_TYPE>>(); break;
#define PCGEX_BLEND_CASE(_ID) case EPCGExDataBlendingType::_ID: switch (Identity.UnderlyingType) { PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SAO_NEW, _ID) } break;

		TSharedPtr<FDataBlendingProcessorBase> NewProcessor;

		switch (Type)
		{
		default:
		PCGEX_FOREACH_BLEND(PCGEX_BLEND_CASE)
		}

		if (NewProcessor) { NewProcessor->SetAttributeName(Identity.Name); }
		return NewProcessor;

#undef PCGEX_SAO_NEW
#undef PCGEX_BLEND_CASE
	}

	static TSharedPtr<FDataBlendingProcessorBase> CreateProcessorWithDefaults(const EPCGExDataBlendingType DefaultType, const PCGEx::FAttributeIdentity& Identity)
	{
		EPCGExDataBlendingTypeDefault GlobalDefaultType = EPCGExDataBlendingTypeDefault::Default;

#define PCGEX_DEF_SET(_TYPE, _NAME, _ID) case EPCGMetadataTypes::_NAME : GlobalDefaultType = GetDefault<UPCGExGlobalSettings>()->Default##_NAME##BlendMode; break;

		switch (Identity.UnderlyingType)
		{
		default:
			break;
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_DEF_SET)
		}

		if (GlobalDefaultType == EPCGExDataBlendingTypeDefault::Default) { return CreateProcessor(DefaultType, Identity); }
		return CreateProcessor(static_cast<EPCGExDataBlendingType>(static_cast<uint8>(GlobalDefaultType)), Identity);

#undef PCGEX_DEF_SET
#undef PCGEX_BLEND_CASE
	}


	static TSharedPtr<FDataBlendingProcessorBase> CreateProcessor(const EPCGExDataBlendingType* Type, const EPCGExDataBlendingType DefaultType, const PCGEx::FAttributeIdentity& Identity)
	{
		return Type ? CreateProcessor(*Type, Identity) : CreateProcessorWithDefaults(DefaultType, Identity);
	}

#undef PCGEX_FOREACH_BLEND
}

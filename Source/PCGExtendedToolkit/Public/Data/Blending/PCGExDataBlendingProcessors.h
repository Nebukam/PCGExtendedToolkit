// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataBlending.h"

namespace PCGExDataBlending
{
	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingAverage final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Average, true, true>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
		virtual void SingleComplete(T& A, const int32 Count, const double Weight) const override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingCopy final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Copy>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingCopyOther final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Copy>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingSum final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Sum, true, false>
	{
	public:
		virtual void SinglePrepare(T& A) const override;
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingSubtract final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Subtract, true, false>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingMax final : public FDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::Max>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingMin final : public FDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::Min>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingWeight final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Weight, true, true>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
		virtual void SingleComplete(T& A, const int32 Count, const double Weight) const override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingWeightedSum final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::WeightedSum, true, false>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingLerp final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Lerp>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingNone final : public FDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::None>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingUnsignedMax final : public FDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::UnsignedMax>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingUnsignedMin final : public FDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::UnsignedMin>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingAbsoluteMax final : public FDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::AbsoluteMax>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingAbsoluteMin final : public FDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::AbsoluteMin>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingWeightedSubtract final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::WeightedSubtract>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingHash final : public FDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::Hash>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class /*PCGEXTENDEDTOOLKIT_API*/ TDataBlendingUnsignedHash final : public FDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::UnsignedHash>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	TSharedPtr<FDataBlendingProcessorBase> CreateProcessor(const EPCGExDataBlendingType Type, const PCGEx::FAttributeIdentity& Identity);
	TSharedPtr<FDataBlendingProcessorBase> CreateProcessorWithDefaults(const EPCGExDataBlendingType DefaultType, const PCGEx::FAttributeIdentity& Identity);
	TSharedPtr<FDataBlendingProcessorBase> CreateProcessor(const EPCGExDataBlendingType* Type, const EPCGExDataBlendingType DefaultType, const PCGEx::FAttributeIdentity& Identity);
}

#define PCGEX_EXTERN_DECL_TYPED(_TYPE, _ID, _BLEND) extern template class PCGExDataBlending::TDataBlending##_BLEND<_TYPE>;
#define PCGEX_EXTERN_DECL(_BLEND) PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_EXTERN_DECL_TYPED, _BLEND)

PCGEX_FOREACH_BLENDMODE(PCGEX_EXTERN_DECL)

#undef PCGEX_EXTERN_DECL_TYPED
#undef PCGEX_EXTERN_DECL

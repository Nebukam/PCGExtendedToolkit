// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExDataBlending.h"

namespace PCGExDataBlending
{
	template <typename T>
	class TDataBlendingAverage final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Average, true, true>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
		virtual void SingleComplete(T& A, const int32 Count, const double Weight) const override;
	};

	template <typename T>
	class TDataBlendingCopy final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Copy>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class TDataBlendingCopyOther final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Copy>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class TDataBlendingSum final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Sum, true, false>
	{
	public:
		virtual void SinglePrepare(T& A) const override;
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class TDataBlendingSubtract final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Subtract, true, false>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class TDataBlendingMax final : public TDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::Max>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class TDataBlendingMin final : public TDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::Min>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class TDataBlendingWeight final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Weight, true, true>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
		virtual void SingleComplete(T& A, const int32 Count, const double Weight) const override;
	};

	template <typename T>
	class TDataBlendingWeightedSum final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::WeightedSum, true, false>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class TDataBlendingLerp final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::Lerp>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class TDataBlendingNone final : public TDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::None>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class TDataBlendingUnsignedMax final : public TDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::UnsignedMax>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class TDataBlendingUnsignedMin final : public TDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::UnsignedMin>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class TDataBlendingAbsoluteMax final : public TDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::AbsoluteMax>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class TDataBlendingAbsoluteMin final : public TDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::AbsoluteMin>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class TDataBlendingWeightedSubtract final : public TDataBlendingProcessor<T, EPCGExDataBlendingType::WeightedSubtract>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class TDataBlendingHash final : public TDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::Hash>
	{
	public:
		virtual T SingleOperation(T A, T B, double Weight) const override;
	};

	template <typename T>
	class TDataBlendingUnsignedHash final : public TDataBlendingProcessorWithFirstInit<T, EPCGExDataBlendingType::UnsignedHash>
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

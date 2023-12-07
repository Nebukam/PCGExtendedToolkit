// Copyright Timothé Lapetite 2023
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
		virtual bool GetRequiresPreparation() const override { return true; }
		virtual bool GetRequiresFinalization() const override { return true; }

		virtual void PrepareOperation(const int32 WriteIndex) const override { this->ResetToDefault(WriteIndex); }

		virtual void DoOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const PCGMetadataEntryKey WriteIndex, const double Alpha = 0) const override
		{
			const T A = this->PrimaryAccessor->Get(PrimaryReadIndex);
			const T B = this->SecondaryAccessor->Get(SecondaryReadIndex);
			this->PrimaryAccessor->SetValue(this->bInterpolationAllowed ? PCGExDataBlending::Add(A, B) : A, WriteIndex);
		}

		virtual void FinalizeOperation(const int32 WriteIndex, double Alpha) const override
		{
			const T A = this->PrimaryAccessor->Get(WriteIndex);
			this->PrimaryAccessor->SetValue(this->bInterpolationAllowed ? PCGExDataBlending::Div(A, Alpha) : A);
		}
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingCopy final : public FDataBlendingOperation<T>
	{
	public:
		virtual void DoOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const PCGMetadataEntryKey WriteIndex, const double Alpha = 0) const override
		{
			this->PrimaryAccessor->SetValue(WriteIndex, this->SecondaryAccessor->Get(SecondaryReadIndex));
		}
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingMax final : public FDataBlendingOperation<T>
	{
	public:
		virtual void DoOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const PCGMetadataEntryKey WriteIndex, const double Alpha = 0) const override
		{
			const T A = this->PrimaryAccessor->Get(PrimaryReadIndex);
			const T B = this->SecondaryAccessor->Get(SecondaryReadIndex);
			this->PrimaryAccessor->SetValue(this->bInterpolationAllowed ? PCGExDataBlending::Max(A, B) : A, WriteIndex);
		}
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingMin final : public FDataBlendingOperation<T>
	{
	public:
		virtual void DoOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const PCGMetadataEntryKey WriteIndex, const double Alpha = 0) const override
		{
			const T A = this->PrimaryAccessor->Get(PrimaryReadIndex);
			const T B = this->SecondaryAccessor->Get(SecondaryReadIndex);
			this->PrimaryAccessor->SetValue(this->bInterpolationAllowed ? PCGExDataBlending::Min(A, B) : A, WriteIndex);
		}
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingWeight final : public FDataBlendingOperation<T>
	{
	public:
		virtual void DoOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const PCGMetadataEntryKey WriteIndex, const double Alpha = 0) const override
		{
			const T A = this->PrimaryAccessor->Get(PrimaryReadIndex);
			const T B = this->SecondaryAccessor->Get(SecondaryReadIndex);
			this->PrimaryAccessor->SetValue(this->bInterpolationAllowed ? PCGExDataBlending::Lerp(A, B, Alpha) : A, WriteIndex);
		}
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API FDataBlendingNone final : public FDataBlendingOperation<T>
	{
	public:
		virtual void DoOperation(const int32 PrimaryReadIndex, const int32 SecondaryReadIndex, const PCGMetadataEntryKey WriteIndex, const double Alpha = 0) const override
		{
			const T B = this->SecondaryAccessor->Get(SecondaryReadIndex);
			this->PrimaryAccessor->SetValue(B, WriteIndex);
		}
	};
}

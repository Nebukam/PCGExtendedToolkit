// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"

#include "PCGExDataBlending.h"
#include "Data/PCGExProxyData.h"
#include "Data/PCGExUnionData.h"


namespace PCGExDataBlending
{
	class PCGEXTENDEDTOOLKIT_API IBlender : public TSharedFromThis<IBlender>
	{
	public:
		virtual ~IBlender() = default;
		// Target = Target|Target
		FORCEINLINE virtual void Blend(const int32 TargetIndex, const double Weight) const
		{
			Blend(TargetIndex, TargetIndex, TargetIndex, Weight);
		}

		// Target = Source|Target
		FORCEINLINE virtual void Blend(const int32 SourceIndex, const int32 TargetIndex, const double Weight) const
		{
			Blend(SourceIndex, TargetIndex, TargetIndex, Weight);
		}

		virtual void InitTrackers(TArray<PCGEx::FOpStats>& Trackers) const = 0;

		// Target = SourceA|SourceB
		virtual void Blend(const int32 SourceIndexA, const int32 SourceIndexB, const int32 TargetIndex, const double Weight) const = 0;

		virtual void BeginMultiBlend(const int32 TargetIndex, TArray<PCGEx::FOpStats>& Trackers) const = 0;
		virtual void MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double Weight, TArray<PCGEx::FOpStats>& Tracker) const = 0;
		virtual void EndMultiBlend(const int32 TargetIndex, TArray<PCGEx::FOpStats>& Tracker) const = 0;
	};

	class PCGEXTENDEDTOOLKIT_API FDummyBlender final : public IBlender
	{
	public:
		virtual ~FDummyBlender() override = default;
		// Target = Target|Target
		virtual void Blend(const int32 TargetIndex, const double Weight) const override
		{
		}

		// Target = Source|Target
		virtual void Blend(const int32 SourceIndex, const int32 TargetIndex, const double Weight) const override
		{
		}

		virtual void InitTrackers(TArray<PCGEx::FOpStats>& Trackers) const override
		{
		}

		// Target = SourceA|SourceB
		virtual void Blend(const int32 SourceIndexA, const int32 SourceIndexB, const int32 TargetIndex, const double Weight) const override
		{
		}

		virtual void BeginMultiBlend(const int32 TargetIndex, TArray<PCGEx::FOpStats>& Trackers) const override
		{
		}

		virtual void MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double Weight, TArray<PCGEx::FOpStats>& Tracker) const override
		{
		}

		FORCEINLINE virtual void EndMultiBlend(const int32 TargetIndex, TArray<PCGEx::FOpStats>& Tracker) const override
		{
		}
	};

	class PCGEXTENDEDTOOLKIT_API IUnionBlender : public TSharedFromThis<IUnionBlender>
	{
	public:
		virtual ~IUnionBlender() = default;

		virtual void InitTrackers(TArray<PCGEx::FOpStats>& Trackers) const = 0;
		virtual int32 ComputeWeights(const int32 WriteIndex, const TSharedPtr<PCGExData::IUnionData>& InUnionData, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints) const = 0;
		virtual void Blend(const int32 WriteIndex, const TArray<PCGExData::FWeightedPoint>& InWeightedPoints, TArray<PCGEx::FOpStats>& Trackers) const = 0;
		virtual void MergeSingle(const int32 WriteIndex, const TSharedPtr<PCGExData::IUnionData>& InUnionData, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints, TArray<PCGEx::FOpStats>& Trackers) const = 0;
		virtual void MergeSingle(const int32 UnionIndex, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints, TArray<PCGEx::FOpStats>& Trackers) const = 0;

		FORCEINLINE EPCGPointNativeProperties GetAllocatedProperties() const { return AllocatedProperties; }

	protected:
		EPCGPointNativeProperties AllocatedProperties = EPCGPointNativeProperties::None;
	};

	class PCGEXTENDEDTOOLKIT_API FDummyUnionBlender final : public IUnionBlender
	{
	public:
		virtual ~FDummyUnionBlender() override = default;

		void Init(const TSharedPtr<PCGExData::FFacade>& TargetData, const TArray<TSharedRef<PCGExData::FFacade>>& InSources);

		virtual void InitTrackers(TArray<PCGEx::FOpStats>& Trackers) const override
		{
		};
		virtual int32 ComputeWeights(const int32 WriteIndex, const TSharedPtr<PCGExData::IUnionData>& InUnionData, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints) const override;

		virtual void Blend(const int32 WriteIndex, const TArray<PCGExData::FWeightedPoint>& InWeightedPoints, TArray<PCGEx::FOpStats>& Trackers) const override
		{
		}

		virtual void MergeSingle(const int32 WriteIndex, const TSharedPtr<PCGExData::IUnionData>& InUnionData, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints, TArray<PCGEx::FOpStats>& Trackers) const override
		{
		};

		virtual void MergeSingle(const int32 UnionIndex, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints, TArray<PCGEx::FOpStats>& Trackers) const override
		{
		};

	protected:
		TSharedPtr<PCGExData::FFacade> CurrentTargetData;
		TSharedPtr<PCGEx::FIndexLookup> IOLookup;
		TArray<const UPCGBasePointData*> SourcesData;
		TSharedPtr<PCGExDetails::FDistances> Distances;
	};

	/**
	 * Simple C=AxB blend
	 */
	class PCGEXTENDEDTOOLKIT_API FProxyDataBlender : public TSharedFromThis<FProxyDataBlender>
	{
	public:
		virtual ~FProxyDataBlender()
		{
		}

		EPCGMetadataTypes UnderlyingType = EPCGMetadataTypes::Unknown;

		// Target = Source|Target
		FORCEINLINE virtual void Blend(const int32 SourceIndex, const int32 TargetIndex, const double Weight) { Blend(SourceIndex, TargetIndex, TargetIndex, Weight); }

		// Target = SourceA|SourceB
		virtual void Blend(const int32 SourceIndexA, const int32 SourceIndexB, const int32 TargetIndex, const double Weight) = 0;

		virtual PCGEx::FOpStats BeginMultiBlend(const int32 TargetIndex) = 0;
		virtual void MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double Weight, PCGEx::FOpStats& Tracker) = 0;
		virtual void EndMultiBlend(const int32 TargetIndex, PCGEx::FOpStats& Tracker) = 0;

		virtual void Div(const int32 TargetIndex, const double Divider) = 0;

		virtual TSharedPtr<PCGExData::IBuffer> GetOutputBuffer() const = 0;

		virtual bool InitFromParam(
			FPCGExContext* InContext, const FBlendingParam& InParam, const TSharedPtr<PCGExData::FFacade> InTargetFacade,
			const TSharedPtr<PCGExData::FFacade> InSourceFacade, PCGExData::EIOSide InSide, bool bWantsDirectAccess = false) = 0;

		template <typename T>
		void Set(const int32 TargetIndex, const T Value) const;

	protected:
#define PCGEX_DECL_BLEND_BIT(_TYPE, _NAME, ...) virtual void Set##_NAME(const int32 TargetIndex, const _TYPE Value) const = 0;
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_DECL_BLEND_BIT)
#undef PCGEX_DECL_BLEND_BIT
	};

#define PCGEX_TPL(_TYPE, _NAME, ...) \
extern template void FProxyDataBlender::Set<_TYPE>(const int32 TargetIndex, const _TYPE Value) const;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

	template <typename T_WORKING>
	class IProxyDataBlender : public FProxyDataBlender
	{
	public:
		TSharedPtr<PCGExData::TBufferProxy<T_WORKING>> A;
		TSharedPtr<PCGExData::TBufferProxy<T_WORKING>> B;
		TSharedPtr<PCGExData::TBufferProxy<T_WORKING>> C;

		IProxyDataBlender() { UnderlyingType = PCGEx::GetMetadataType<T_WORKING>(); }

		virtual ~IProxyDataBlender() override
		{
		}

		virtual void Blend(const int32 SourceIndexA, const int32 SourceIndexB, const int32 TargetIndex, const double Weight = 1) override
		PCGEX_NOT_IMPLEMENTED(Blend(const int32 SourceIndexA, const int32 SourceIndexB, const int32 TargetIndex, const double Weight = 1))


		virtual PCGEx::FOpStats BeginMultiBlend(const int32 TargetIndex) override
		PCGEX_NOT_IMPLEMENTED_RET(BeginMultiBlend(const int32 TargetIndex), PCGEx::FOpStats{})

		virtual void MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double Weight, PCGEx::FOpStats& Tracker) override
		PCGEX_NOT_IMPLEMENTED(MultiBlend(const int32 SourceIndex, const int32 TargetIndex, FBlendTracker& Tracker))

		virtual void EndMultiBlend(const int32 TargetIndex, PCGEx::FOpStats& Tracker) override
		PCGEX_NOT_IMPLEMENTED(EndMultiBlend(const int32 TargetIndex, FBlendTracker& Tracker))

		virtual void Div(const int32 TargetIndex, const double Divider) override
		PCGEX_NOT_IMPLEMENTED(Div(const int32 TargetIndex, const double Divider))

		virtual TSharedPtr<PCGExData::IBuffer> GetOutputBuffer() const override { return C ? C->GetBuffer() : nullptr; }

		virtual bool InitFromParam(
			FPCGExContext* InContext, const FBlendingParam& InParam, const TSharedPtr<PCGExData::FFacade> InTargetFacade,
			const TSharedPtr<PCGExData::FFacade> InSourceFacade, const PCGExData::EIOSide InSide, const bool bWantsDirectAccess = false) override
		{
			// Setup a single blender per A/B pair
			PCGExData::FProxyDescriptor Desc_A = PCGExData::FProxyDescriptor(InSourceFacade, PCGExData::EProxyRole::Read);
			PCGExData::FProxyDescriptor Desc_B = PCGExData::FProxyDescriptor(InTargetFacade, PCGExData::EProxyRole::Read);

			if (!Desc_A.Capture(InContext, InParam.Selector, InSide)) { return false; }

			if (InParam.bIsNewAttribute)
			{
				// Capturing B will fail as it does not exist yet.
				// Simply copy A
				Desc_B = Desc_A;

				// Swap B side for Out so the buffer will be initialized
				Desc_B.Side = PCGExData::EIOSide::Out;
				Desc_B.DataFacade = InTargetFacade;
			}
			else
			{
				// Strict capture may fail here, TBD
				if (!Desc_B.CaptureStrict(InContext, InParam.Selector, PCGExData::EIOSide::Out)) { return false; }
			}

			PCGExData::FProxyDescriptor Desc_C = Desc_B;
			Desc_C.Side = PCGExData::EIOSide::Out;
			Desc_C.Role = PCGExData::EProxyRole::Write;

			Desc_A.bWantsDirect = bWantsDirectAccess;
			Desc_B.bWantsDirect = bWantsDirectAccess;
			Desc_C.bWantsDirect = bWantsDirectAccess;

			// Create output first so we may read from it
			C = StaticCastSharedPtr<PCGExData::TBufferProxy<T_WORKING>>(GetProxyBuffer(InContext, Desc_C));
			A = StaticCastSharedPtr<PCGExData::TBufferProxy<T_WORKING>>(GetProxyBuffer(InContext, Desc_A));
			B = StaticCastSharedPtr<PCGExData::TBufferProxy<T_WORKING>>(GetProxyBuffer(InContext, Desc_B));

			// Ensure C is readable for MultiBlend, as those will use GetCurrent
			if (!C->EnsureReadable())
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Fail to ensure target buffer is readable."));
				return false;
			}

			return A && B && C;
		}

	protected:
#define PCGEX_DECL_BLEND_BIT(_TYPE, _NAME, ...) virtual void Set##_NAME(const int32 TargetIndex, const _TYPE Value) const override { C->Set(TargetIndex, PCGEx::Convert<_TYPE, T_WORKING>(Value)); };
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_DECL_BLEND_BIT)
#undef PCGEX_DECL_BLEND_BIT
	};

#pragma region externalization

#define PCGEX_TPL(_TYPE, _NAME, ...)\
extern template class IProxyDataBlender<_TYPE>;

	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)

#undef PCGEX_TPL

#pragma endregion

	template <typename T_WORKING, EPCGExABBlendingType BLEND_MODE, bool bResetValueForMultiBlend = true>
	class TProxyDataBlender : public IProxyDataBlender<T_WORKING>
	{
		using IProxyDataBlender<T_WORKING>::A;
		using IProxyDataBlender<T_WORKING>::B;
		using IProxyDataBlender<T_WORKING>::C;

	public:
		virtual ~TProxyDataBlender() override
		{
		}

		virtual void Blend(const int32 SourceIndexA, const int32 SourceIndexB, const int32 TargetIndex, const double Weight = 1) override
		{
			BOOKMARK_BLENDMODE

			check(A)
			if constexpr (BLEND_MODE != EPCGExABBlendingType::CopySource) { check(B) }
			check(C)

#define PCGEX_A A->Get(SourceIndexA)
#define PCGEX_B B->Get(SourceIndexB)

			if constexpr (BLEND_MODE == EPCGExABBlendingType::None)
			{
			}
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Average) { C->Set(TargetIndex, PCGExBlend::Div(PCGExBlend::Add(PCGEX_A,PCGEX_B), 2)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Weight) { C->Set(TargetIndex, PCGExBlend::WeightedAdd(PCGEX_A, PCGEX_B, Weight)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Min) { C->Set(TargetIndex, PCGExBlend::Min(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Max) { C->Set(TargetIndex, PCGExBlend::Max(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Add) { C->Set(TargetIndex, PCGExBlend::Add(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Subtract) { C->Set(TargetIndex, PCGExBlend::Sub(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Multiply) { C->Set(TargetIndex, PCGExBlend::Mult(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Divide) { C->Set(TargetIndex, PCGExBlend::Div(PCGEX_A, PCGEx::Convert<T_WORKING, double>(PCGEX_B))); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::WeightedAdd) { C->Set(TargetIndex, PCGExBlend::WeightedAdd(PCGEX_A,PCGEX_B, Weight)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::WeightedSubtract) { C->Set(TargetIndex, PCGExBlend::WeightedSub(PCGEX_A, PCGEX_B, Weight)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Lerp) { C->Set(TargetIndex, PCGExBlend::Lerp(PCGEX_A,PCGEX_B, Weight)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::UnsignedMin) { C->Set(TargetIndex, PCGExBlend::UnsignedMin(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::UnsignedMax) { C->Set(TargetIndex, PCGExBlend::UnsignedMax(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::AbsoluteMin) { C->Set(TargetIndex, PCGExBlend::AbsoluteMin(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::AbsoluteMax) { C->Set(TargetIndex, PCGExBlend::AbsoluteMax(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::CopyTarget) { C->Set(TargetIndex, PCGEX_B); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::CopySource) { C->Set(TargetIndex, PCGEX_A); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Hash) { C->Set(TargetIndex, PCGExBlend::NaiveHash(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::UnsignedHash) { C->Set(TargetIndex, PCGExBlend::NaiveUnsignedHash(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Mod) { C->Set(TargetIndex, PCGExBlend::ModSimple(PCGEX_A, PCGEx::Convert<T_WORKING, double>(PCGEX_B))); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::ModCW) { C->Set(TargetIndex, PCGExBlend::ModComplex(PCGEX_A,PCGEX_B)); }

#undef PCGEX_A
#undef PCGEX_B
		}

		virtual PCGEx::FOpStats BeginMultiBlend(const int32 TargetIndex) override
		{
			check(C)

			PCGEx::FOpStats Tracker{};

			if constexpr (
				BLEND_MODE == EPCGExABBlendingType::Min ||
				BLEND_MODE == EPCGExABBlendingType::Max ||
				BLEND_MODE == EPCGExABBlendingType::UnsignedMin ||
				BLEND_MODE == EPCGExABBlendingType::UnsignedMax ||
				BLEND_MODE == EPCGExABBlendingType::AbsoluteMin ||
				BLEND_MODE == EPCGExABBlendingType::AbsoluteMax ||
				BLEND_MODE == EPCGExABBlendingType::Hash ||
				BLEND_MODE == EPCGExABBlendingType::UnsignedHash)
			{
				// These modes require the first operation to be a copy of the value
				// before the can be properly blended
				Tracker.Count = -1;
			}
			else if constexpr (
				BLEND_MODE == EPCGExABBlendingType::Average ||
				BLEND_MODE == EPCGExABBlendingType::Add ||
				BLEND_MODE == EPCGExABBlendingType::Subtract ||
				BLEND_MODE == EPCGExABBlendingType::Weight ||
				BLEND_MODE == EPCGExABBlendingType::WeightedAdd ||
				BLEND_MODE == EPCGExABBlendingType::WeightedSubtract)
			{
				// Some BlendModes can leverage this
				if constexpr (bResetValueForMultiBlend)
				{
					C->Set(TargetIndex, T_WORKING{});
				}
				else
				{
					// Otherwise, bump up original count so EndBlend can account for pre-existing value as "one blend step"
					Tracker.Count = 1;
					Tracker.Weight = 1;
				}
			}

			return Tracker;
		}

		virtual void MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double Weight, PCGEx::FOpStats& Tracker) override
		{
			check(A)
			check(C)

			ON_SCOPE_EXIT
			{
				Tracker.Count++;
				Tracker.Weight += Weight;
			};

#define PCGEX_SRC A->Get(SourceIndex)
#define PCGEX_TGT C->GetCurrent(TargetIndex) // We read from current value during multiblend

			if (Tracker.Count < 0)
			{
				Tracker.Count = 0;
				C->Set(TargetIndex, PCGEX_SRC);
				return;
			}

			if constexpr (BLEND_MODE == EPCGExABBlendingType::None)
			{
			}
			if constexpr (BLEND_MODE == EPCGExABBlendingType::Average) { C->Set(TargetIndex, PCGExBlend::Add(PCGEX_SRC,PCGEX_TGT)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Weight) { C->Set(TargetIndex, PCGExBlend::WeightedAdd(PCGEX_TGT, PCGEX_SRC, Weight)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Min) { C->Set(TargetIndex, PCGExBlend::Min(PCGEX_TGT,PCGEX_SRC)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Max) { C->Set(TargetIndex, PCGExBlend::Max(PCGEX_TGT,PCGEX_SRC)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Add) { C->Set(TargetIndex, PCGExBlend::Add(PCGEX_TGT,PCGEX_SRC)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Subtract) { C->Set(TargetIndex, PCGExBlend::Sub(PCGEX_TGT,PCGEX_SRC)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Multiply) { C->Set(TargetIndex, PCGExBlend::Mult(PCGEX_TGT,PCGEX_SRC)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Divide) { C->Set(TargetIndex, PCGExBlend::Div(PCGEX_TGT, PCGEx::Convert<T_WORKING, double>(PCGEX_SRC))); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::WeightedAdd) { C->Set(TargetIndex, PCGExBlend::WeightedAdd(PCGEX_TGT,PCGEX_SRC, Weight)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::WeightedSubtract) { C->Set(TargetIndex, PCGExBlend::WeightedSub(PCGEX_TGT, PCGEX_SRC, Weight)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Lerp) { C->Set(TargetIndex, PCGExBlend::Lerp(PCGEX_TGT,PCGEX_SRC, Weight)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::UnsignedMin) { C->Set(TargetIndex, PCGExBlend::UnsignedMin(PCGEX_TGT,PCGEX_SRC)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::UnsignedMax) { C->Set(TargetIndex, PCGExBlend::UnsignedMax(PCGEX_TGT,PCGEX_SRC)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::AbsoluteMin) { C->Set(TargetIndex, PCGExBlend::AbsoluteMin(PCGEX_TGT,PCGEX_SRC)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::AbsoluteMax) { C->Set(TargetIndex, PCGExBlend::AbsoluteMax(PCGEX_TGT,PCGEX_SRC)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::CopyTarget) { C->Set(TargetIndex, PCGEX_TGT); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::CopySource) { C->Set(TargetIndex, PCGEX_SRC); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Hash) { C->Set(TargetIndex, PCGExBlend::NaiveHash(PCGEX_TGT,PCGEX_SRC)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::UnsignedHash) { C->Set(TargetIndex, PCGExBlend::NaiveUnsignedHash(PCGEX_TGT,PCGEX_SRC)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Mod) { C->Set(TargetIndex, PCGExBlend::ModSimple(PCGEX_TGT, PCGEx::Convert<T_WORKING, double>(PCGEX_SRC))); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::ModCW) { C->Set(TargetIndex, PCGExBlend::ModComplex(PCGEX_TGT,PCGEX_SRC)); }


#undef PCGEX_SRC
#undef PCGEX_TGT
		}

		virtual void EndMultiBlend(const int32 TargetIndex, PCGEx::FOpStats& Tracker) override
		{
			check(A)
			check(C)

#define PCGEX_C C->GetCurrent(TargetIndex)

			if (!Tracker.Count) { return; } // Skip division by zero

			// Some modes require a "finish" pass, like Average and Weight
			if constexpr (BLEND_MODE == EPCGExABBlendingType::Average) { C->Set(TargetIndex, PCGExBlend::Div(PCGEX_C, Tracker.Count)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Weight)
			{
				if (Tracker.Weight > 1) { C->Set(TargetIndex, PCGExBlend::Div(PCGEX_C, Tracker.Weight)); }
			}

#undef PCGEX_C
		}

		// Target = Target / Divider
		// Useful for finalizing multi-source ops
		virtual void Div(const int32 TargetIndex, const double Divider) override { C->Set(TargetIndex, PCGExBlend::Div(C->Get(TargetIndex), Divider)); }
	};

#pragma region externalization

#define PCGEX_TPL(_TYPE, _NAME, _BLENDMODE)\
extern template class TProxyDataBlender<_TYPE, EPCGExABBlendingType::_BLENDMODE, true>;\
extern template class TProxyDataBlender<_TYPE, EPCGExABBlendingType::_BLENDMODE, false>;

#define PCGEX_TPL_LOOP(_BLENDMODE)	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL, _BLENDMODE)

	PCGEX_FOREACH_PROXYBLENDMODE(PCGEX_TPL_LOOP)

#undef PCGEX_TPL_LOOP
#undef PCGEX_TPL

#pragma endregion

	template <typename T>
	TSharedPtr<IProxyDataBlender<T>> CreateProxyBlender(const EPCGExABBlendingType BlendMode, const bool bResetValueForMultiBlend = true);

#define PCGEX_TPL(_TYPE, _NAME, ...) \
extern template TSharedPtr<IProxyDataBlender<_TYPE>> CreateProxyBlender(const EPCGExABBlendingType BlendMode, const bool bResetValueForMultiBlend);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

	PCGEXTENDEDTOOLKIT_API
	TSharedPtr<FProxyDataBlender> CreateProxyBlender(
		FPCGExContext* InContext,
		const EPCGExABBlendingType BlendMode,
		const PCGExData::FProxyDescriptor& A,
		const PCGExData::FProxyDescriptor& B,
		const PCGExData::FProxyDescriptor& C,
		const bool bResetValueForMultiBlend = true);


	PCGEXTENDEDTOOLKIT_API
	TSharedPtr<FProxyDataBlender> CreateProxyBlender(
		FPCGExContext* InContext,
		const EPCGExABBlendingType BlendMode,
		const PCGExData::FProxyDescriptor& A,
		const PCGExData::FProxyDescriptor& C,
		const bool bResetValueForMultiBlend = true);
}

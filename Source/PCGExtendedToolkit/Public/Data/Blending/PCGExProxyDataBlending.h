// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"

#include "PCGExDataBlending.h"
#include "Data/PCGExProxyData.h"


namespace PCGExDataBlending
{
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

		// Target = Target|Target
		FORCEINLINE virtual void Blend(const int32 TargetIndex, const double Weight = 1) { Blend(TargetIndex, TargetIndex, TargetIndex); }

		// Target = Source|Target
		FORCEINLINE virtual void Blend(const int32 SourceIndex, const int32 TargetIndex, const double Weight = 1) { Blend(SourceIndex, TargetIndex, TargetIndex); }

		// Target = SourceA|SourceB
		virtual void Blend(const int32 SourceIndexA, const int32 SourceIndexB, const int32 TargetIndex, const double Weight = 1) = 0;

		virtual void Blend(TArrayView<const int32> SourceIndices, const int32 TargetIndex, TArrayView<const double> Weights);
		virtual void Blend(TArrayView<const int32> SourceIndices, const int32 TargetIndex, const double Weight = 1);

		virtual PCGEx::FOpStats BeginMultiBlend(const int32 TargetIndex) = 0;
		virtual void MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double Weight, PCGEx::FOpStats& Tracker) = 0;
		virtual void EndMultiBlend(const int32 TargetIndex, PCGEx::FOpStats& Tracker) = 0;

		virtual void Div(const int32 TargetIndex, const double Divider) = 0;

		virtual TSharedPtr<PCGExData::FBufferBase> GetOutputBuffer() const = 0;

		virtual bool InitFromHeader(
			FPCGExContext* InContext, const FBlendingHeader& InHeader, const TSharedPtr<PCGExData::FFacade> InTargetFacade,
			const TSharedPtr<PCGExData::FFacade> InSourceFacade, PCGExData::EIOSide InSide, bool bWantsDirectAccess = false) = 0;

		template <typename T>
		void Set(const int32 TargetIndex, const T Value)
		{
#define PCGEX_DECL_BLEND_BIT(_TYPE, _NAME, ...) else if constexpr (std::is_same_v<T, _TYPE>){ Set##_NAME(TargetIndex, Value); }
			if constexpr (false)
			{
			}
			PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_DECL_BLEND_BIT)
#undef PCGEX_DECL_BLEND_BIT
		}

	protected:
#define PCGEX_DECL_BLEND_BIT(_TYPE, _NAME, ...) virtual void SetValue(const int32 TargetIndex, const _TYPE Value) = 0;
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_DECL_BLEND_BIT)
#undef PCGEX_DECL_BLEND_BIT
	};

	template <typename T_WORKING>
	class PCGEXTENDEDTOOLKIT_API TProxyDataBlenderBase : public FProxyDataBlender
	{
	public:
		TSharedPtr<PCGExData::TBufferProxy<T_WORKING>> A;
		TSharedPtr<PCGExData::TBufferProxy<T_WORKING>> B;
		TSharedPtr<PCGExData::TBufferProxy<T_WORKING>> C;

		TProxyDataBlenderBase() { UnderlyingType = PCGEx::GetMetadataType<T_WORKING>(); }

		virtual ~TProxyDataBlenderBase() override
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

		virtual TSharedPtr<PCGExData::FBufferBase> GetOutputBuffer() const override { return C ? C->GetBuffer() : nullptr; }

		virtual bool InitFromHeader(
			FPCGExContext* InContext, const FBlendingHeader& InHeader, const TSharedPtr<PCGExData::FFacade> InTargetFacade,
			const TSharedPtr<PCGExData::FFacade> InSourceFacade, const PCGExData::EIOSide InSide, const bool bWantsDirectAccess = false) override
		{
			// Setup a single blender per A/B pair
			PCGExData::FProxyDescriptor Desc_A = PCGExData::FProxyDescriptor(InSourceFacade);
			PCGExData::FProxyDescriptor Desc_B = PCGExData::FProxyDescriptor(InTargetFacade);

			if (!Desc_A.Capture(InContext, InHeader.Selector, InSide)) { return false; }

			if (InHeader.bIsNewAttribute)
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
				if (!Desc_B.CaptureStrict(InContext, InHeader.Selector, PCGExData::EIOSide::Out)) { return false; }
			}

			PCGExData::FProxyDescriptor Desc_C = Desc_B;

			Desc_A.bWantsDirect = bWantsDirectAccess;
			Desc_B.bWantsDirect = bWantsDirectAccess;
			Desc_C.bWantsDirect = bWantsDirectAccess;

			A = StaticCastSharedPtr<PCGExData::TBufferProxy<T_WORKING>>(PCGExData::GetProxyBuffer(InContext, Desc_A));
			B = StaticCastSharedPtr<PCGExData::TBufferProxy<T_WORKING>>(PCGExData::GetProxyBuffer(InContext, Desc_B));
			C = StaticCastSharedPtr<PCGExData::TBufferProxy<T_WORKING>>(PCGExData::GetProxyBuffer(InContext, Desc_C));

			return A && B && C;
		}

	protected:
#define PCGEX_DECL_BLEND_BIT(_TYPE, _NAME, ...) virtual void SetValue(const int32 TargetIndex, const _TYPE Value) override { C->Set(TargetIndex, PCGEx::Convert<T_WORKING>(Value)); };
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_DECL_BLEND_BIT)
#undef PCGEX_DECL_BLEND_BIT
	};

	template <typename T_WORKING, EPCGExABBlendingType BLEND_MODE>
	class PCGEXTENDEDTOOLKIT_API TProxyDataBlender : public TProxyDataBlenderBase<T_WORKING>
	{
		using TProxyDataBlenderBase<T_WORKING>::A;
		using TProxyDataBlenderBase<T_WORKING>::B;
		using TProxyDataBlenderBase<T_WORKING>::C;

	public:
		virtual ~TProxyDataBlender() override
		{
		}

		virtual void Blend(const int32 SourceIndexA, const int32 SourceIndexB, const int32 TargetIndex, const double Weight = 1) override
		{
			BOOKMARK_BLENDMODE

			check(A)
			check(B)
			check(C)

#define PCGEX_A A->Get(SourceIndexA)
#define PCGEX_B B->Get(SourceIndexB)

			if constexpr (BLEND_MODE == EPCGExABBlendingType::None)
			{
			}
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Average) { C->Set(TargetIndex, PCGExBlend::Div(PCGExBlend::Add(PCGEX_A,PCGEX_B), 2)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Weight) { C->Set(TargetIndex, PCGExBlend::Div(PCGExBlend::Add(PCGEX_A,PCGEX_B), Weight)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Min) { C->Set(TargetIndex, PCGExBlend::Min(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Max) { C->Set(TargetIndex, PCGExBlend::Max(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Add) { C->Set(TargetIndex, PCGExBlend::Add(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Subtract) { C->Set(TargetIndex, PCGExBlend::Sub(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Multiply) { C->Set(TargetIndex, PCGExBlend::Mult(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Divide) { C->Set(TargetIndex, PCGExBlend::Div(PCGEX_A, PCGEx::Convert<double>(PCGEX_B))); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::WeightedAdd) { C->Set(TargetIndex, PCGExBlend::WeightedAdd(PCGEX_A,PCGEX_B, Weight)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::WeightedSubtract) { C->Set(TargetIndex, PCGExBlend::WeightedSub(PCGEX_A,PCGEX_B, Weight)); }
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
			PCGEx::FOpStats Tracker{};

			if constexpr (
				BLEND_MODE == EPCGExABBlendingType::Average ||
				BLEND_MODE == EPCGExABBlendingType::Add ||
				BLEND_MODE == EPCGExABBlendingType::Subtract ||
				BLEND_MODE == EPCGExABBlendingType::Weight ||
				BLEND_MODE == EPCGExABBlendingType::WeightedAdd)
			{
				// These modes require a zeroed-out value
				// before the can be properly blended
				C->Set(TargetIndex, T_WORKING{});
			}
			else if constexpr (
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

			return Tracker;
		}

		virtual void MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double Weight, PCGEx::FOpStats& Tracker) override
		{
			ON_SCOPE_EXIT
			{
				Tracker.Count++;
				Tracker.Weight += Weight;
			};

#define PCGEX_A A->Get(SourceIndex)
#define PCGEX_B B->Get(TargetIndex)

			if (Tracker.Count < 0)
			{
				Tracker.Count = 0;
				C->Set(TargetIndex, PCGEX_A);
				return;
			}

			if constexpr (BLEND_MODE == EPCGExABBlendingType::Average) { C->Set(TargetIndex, PCGExBlend::Add(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Weight) { C->Set(TargetIndex, PCGExBlend::Add(PCGEX_A,PCGEX_B)); }
			else { Blend(SourceIndex, TargetIndex, TargetIndex); }

#undef PCGEX_A
#undef PCGEX_B
		}

		virtual void EndMultiBlend(const int32 TargetIndex, PCGEx::FOpStats& Tracker) override
		{
#define PCGEX_C C->Get(TargetIndex)

			if (!Tracker.Count) { return; } // Skip division by zero

			// Some modes require a "finish" pass, like Average and Weight
			if constexpr (BLEND_MODE == EPCGExABBlendingType::Average) { C->Set(TargetIndex, PCGExBlend::Div(PCGEX_C, Tracker.Count)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Weight) { C->Set(TargetIndex, PCGExBlend::Div(PCGEX_C, Tracker.Weight)); }

#undef PCGEX_C
		}

		// Target = Target / Divider
		// Useful for finalizing multi-source ops
		virtual void Div(const int32 TargetIndex, const double Divider) override { C->Set(TargetIndex, PCGExBlend::Div(C->Get(TargetIndex), Divider)); }
	};

	template <typename T>
	static TSharedPtr<TProxyDataBlenderBase<T>> CreateProxyBlender(const EPCGExABBlendingType BlendMode)
	{
		TSharedPtr<TProxyDataBlenderBase<T>> OutBlender;

#define PCGEX_CREATE_BLENDER(_BLEND)case EPCGExABBlendingType::_BLEND : \
OutBlender = MakeShared<TProxyDataBlender<T, EPCGExABBlendingType::_BLEND>>(); \
break;
		switch (BlendMode) { PCGEX_FOREACH_PROXYBLENDMODE(PCGEX_CREATE_BLENDER) }
#undef PCGEX_CREATE_BLENDER

		return OutBlender;
	}

	static TSharedPtr<FProxyDataBlender> CreateProxyBlender(
		FPCGExContext* InContext,
		const EPCGExABBlendingType BlendMode,
		const PCGExData::FProxyDescriptor& A,
		const PCGExData::FProxyDescriptor& B,
		const PCGExData::FProxyDescriptor& C)
	{
		TSharedPtr<FProxyDataBlender> OutBlender;

		if (A.WorkingType != B.WorkingType || A.WorkingType != C.WorkingType)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("ProxyBlender : T_WORKING mismatch."));
			return nullptr;
		}

		PCGEx::ExecuteWithRightType(
			A.WorkingType, [&](auto DummyValue)
			{
				using T = decltype(DummyValue);

				TSharedPtr<TProxyDataBlenderBase<T>> TypedBlender = CreateProxyBlender<T>(BlendMode);

				if (!TypedBlender) { return; }

				TypedBlender->A = StaticCastSharedPtr<PCGExData::TBufferProxy<T>>(GetProxyBuffer(InContext, A));
				TypedBlender->B = StaticCastSharedPtr<PCGExData::TBufferProxy<T>>(GetProxyBuffer(InContext, B));
				TypedBlender->C = StaticCastSharedPtr<PCGExData::TBufferProxy<T>>(GetProxyBuffer(InContext, C));

				if (!TypedBlender) { return; }
				if (!TypedBlender->A || !TypedBlender->B || !TypedBlender->C)
				{
					PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("ProxyBlender : Missing at least one proxy."));
					return;
				}

				OutBlender = TypedBlender;
			});


		return OutBlender;
	}
}

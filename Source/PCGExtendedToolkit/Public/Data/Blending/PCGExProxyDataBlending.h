// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"

#include "PCGExDataBlending.h"
#include "Data/PCGExProxyData.h"

// This is a different blending list that makes more sense for AxB blending
// and also includes extra modes that don't make sense in regular multi-source data blending
UENUM(BlueprintType)
enum class EPCGExABBlendingType : uint8
{
	None             = 0 UMETA(Hidden, DisplayName = "None", ToolTip="No blending is applied, keep the original value."),
	Average          = 1 UMETA(DisplayName = "Average", ToolTip="(A + B) / 2"),
	Weight           = 2 UMETA(DisplayName = "Weight", ToolTip="(A + B) / Weight"),
	Multiply         = 3 UMETA(DisplayName = "Multiply", ToolTip="A * B"),
	Divide           = 4 UMETA(DisplayName = "Divide", ToolTip="A / B"),
	Min              = 5 UMETA(DisplayName = "Min", ToolTip="Min(A, B)"),
	Max              = 6 UMETA(DisplayName = "Max", ToolTip="Max(A, B)"),
	CopyTarget       = 7 UMETA(DisplayName = "Copy (Target)", ToolTip = "= B"),
	CopySource       = 8 UMETA(DisplayName = "Copy (Source)", ToolTip="= A"),
	Add              = 9 UMETA(DisplayName = "Add", ToolTip = "A + B"),
	Subtract         = 10 UMETA(DisplayName = "Subtract", ToolTip="A - B"),
	WeightedAdd      = 11 UMETA(DisplayName = "Weighted Add", ToolTip = "A + (B * Weight)"),
	WeightedSubtract = 12 UMETA(DisplayName = "Weighted Subtract", ToolTip="A - (B * Weight)"),
	Lerp             = 13 UMETA(DisplayName = "Lerp", ToolTip="Lerp(A, B, Weight)"),
	UnsignedMin      = 14 UMETA(DisplayName = "Unsigned Min", ToolTip="Min(A, B) * Sign"),
	UnsignedMax      = 15 UMETA(DisplayName = "Unsigned Max", ToolTip="Max(A, B) * Sign"),
	AbsoluteMin      = 16 UMETA(DisplayName = "Absolute Min", ToolTip="+Min(A, B)"),
	AbsoluteMax      = 17 UMETA(DisplayName = "Absolute Max", ToolTip="+Max(A, B)"),
	Hash             = 18 UMETA(DisplayName = "Hash", ToolTip="Hash(A, B)"),
	UnsignedHash     = 19 UMETA(DisplayName = "Hash (Unsigned)", ToolTip="Hash(Min(A, B), Max(A, B))"),
	Mod              = 20 UMETA(DisplayName = "Modulo (Simple)", ToolTip="FMod(A, cast(B))"),
	ModCW            = 21 UMETA(DisplayName = "Modulo (Component Wise)", ToolTip="FMod(A, B)")
};

#define PCGEX_FOREACH_PROXYBLENDMODE(MACRO)\
MACRO(None) \
MACRO(Average) \
MACRO(Weight) \
MACRO(Multiply) \
MACRO(Divide) \
MACRO(Min) \
MACRO(Max) \
MACRO(CopyTarget) \
MACRO(CopySource) \
MACRO(Add) \
MACRO(Subtract) \
MACRO(WeightedAdd) \
MACRO(WeightedSubtract) \
MACRO(Lerp) \
MACRO(UnsignedMin) \
MACRO(UnsignedMax) \
MACRO(AbsoluteMin) \
MACRO(AbsoluteMax) \
MACRO(Hash) \
MACRO(UnsignedHash) \
MACRO(Mod) \
MACRO(ModCW)

namespace PCGExDataBlending
{

	
	
	struct FBlendTracker
	{
		int32 Count = 0;
		double Weight = 0;
	};

	/**
	 * Simple C=AxB blend
	 */
	class PCGEXTENDEDTOOLKIT_API FProxyDataBlenderBase : public TSharedFromThis<FProxyDataBlenderBase>
	{
	public:
		virtual ~FProxyDataBlenderBase()
		{
		}

		// Target = Target|Target
		FORCEINLINE virtual void Blend(const int32 TargetIndex, const double Weight = 1) { Blend(TargetIndex, TargetIndex, TargetIndex); }

		// Target = Source|Target
		FORCEINLINE  virtual void Blend(const int32 SourceIndex, const int32 TargetIndex, const double Weight = 1) { Blend(SourceIndex, TargetIndex, TargetIndex); }

		// Target = SourceA|SourceB
		virtual void Blend(const int32 SourceIndexA, const int32 SourceIndexB, const int32 TargetIndex, const double Weight = 1) = 0;

		virtual void Blend(TArrayView<const int32> SourceIndices, const int32 TargetIndex, TArrayView<const double> Weights);
		virtual void Blend(TArrayView<const int32> SourceIndices, const int32 TargetIndex, const double Weight = 1);

		virtual FBlendTracker BeginMultiBlend(const int32 TargetIndex) = 0;
		virtual void MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double Weight, FBlendTracker& Tracker) = 0;
		virtual void EndMultiBlend(const int32 TargetIndex, FBlendTracker& Tracker) = 0;

		virtual void Div(const int32 TargetIndex, const double Divider) = 0;

		virtual TSharedPtr<PCGExData::FBufferBase> GetOutputBuffer() const = 0;

		template <typename T>
		void Set(const int32 TargetIndex, const T Value)
		{
#define PCGEX_DECL_BLEND_BIT(_TYPE, _NAME) else if constexpr (std::is_same_v<T, _TYPE>){ Set##_NAME(TargetIndex, Value); }
			if constexpr (false)
			{
			}
			PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_DECL_BLEND_BIT)
#undef PCGEX_DECL_BLEND_BIT
		}

	protected:
#define PCGEX_DECL_BLEND_BIT(_TYPE, _NAME) virtual void Set##_Name(const int32 TargetIndex, const _TYPE Value) = 0;
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_DECL_BLEND_BIT)
#undef PCGEX_DECL_BLEND_BIT
	};

	template <typename T_WORKING>
	class PCGEXTENDEDTOOLKIT_API TProxyDataBlenderBase : public FProxyDataBlenderBase
	{
	public:
		TSharedPtr<PCGExData::TBufferProxy<T_WORKING>> A;
		TSharedPtr<PCGExData::TBufferProxy<T_WORKING>> B;
		TSharedPtr<PCGExData::TBufferProxy<T_WORKING>> C;

		virtual ~TProxyDataBlenderBase() override
		{
		}

		virtual void Blend(const int32 SourceIndexA, const int32 SourceIndexB, const int32 TargetIndex, const double Weight = 1) override
		PCGEX_NOT_IMPLEMENTED(Blend(const int32 SourceIndexA, const int32 SourceIndexB, const int32 TargetIndex, const double Weight = 1))


		virtual FBlendTracker BeginMultiBlend(const int32 TargetIndex) override
		PCGEX_NOT_IMPLEMENTED_RET(BeginMultiBlend(const int32 TargetIndex), FBlendTracker{})

		virtual void MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double Weight, FBlendTracker& Tracker) override
		PCGEX_NOT_IMPLEMENTED(MultiBlend(const int32 SourceIndex, const int32 TargetIndex, FBlendTracker& Tracker))

		virtual void EndMultiBlend(const int32 TargetIndex, FBlendTracker& Tracker) override
		PCGEX_NOT_IMPLEMENTED(EndMultiBlend(const int32 TargetIndex, FBlendTracker& Tracker))


		virtual void Div(const int32 TargetIndex, const double Divider) override
		PCGEX_NOT_IMPLEMENTED(Div(const int32 TargetIndex, const double Divider))

		virtual TSharedPtr<PCGExData::FBufferBase> GetOutputBuffer() const override { return C ? C->GetBuffer() : nullptr; }

	protected:
#define PCGEX_DECL_BLEND_BIT(_TYPE, _NAME) virtual void Set##_Name(const int32 TargetIndex, const _TYPE Value) override { C->Set(TargetIndex, Value); };
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

		virtual FBlendTracker BeginMultiBlend(const int32 TargetIndex) override
		{
			// Some modes need to "start from scratch", or use a sensible default
			// This is true for Min/Max

			FBlendTracker Tracker{};

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
				// TODO : Might need to add a way to overwrite with "defaults"
				// on top of copying the first value. Or this might simply be enough.
				Tracker.Count = -1;
			}

			return Tracker;
		}

		virtual void MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double Weight, FBlendTracker& Tracker) override
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

		virtual void EndMultiBlend(const int32 TargetIndex, FBlendTracker& Tracker) override
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

	static TSharedPtr<FProxyDataBlenderBase> CreateProxyBlender(
		FPCGExContext* InContext,
		const EPCGExABBlendingType BlendMode,
		const PCGExData::FProxyDescriptor& A,
		const PCGExData::FProxyDescriptor& B,
		const PCGExData::FProxyDescriptor& C)
	{
		TSharedPtr<FProxyDataBlenderBase> OutBlender;

		if (A.WorkingType != B.WorkingType || A.WorkingType != C.WorkingType)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("ProxyBlender : T_WORKING mismatch."));
			return nullptr;
		}

		PCGEx::ExecuteWithRightType(
			A.WorkingType, [&](auto DummyValue)
			{
				using T = decltype(DummyValue);
				TSharedPtr<TProxyDataBlenderBase<T>> TypedBlender;

#define PCGEX_CREATE_BLENDER(_BLEND)case EPCGExABBlendingType::_BLEND : \
TypedBlender = MakeShared<TProxyDataBlender<T, EPCGExABBlendingType::_BLEND>>(); \
break;
				switch (BlendMode) { PCGEX_FOREACH_PROXYBLENDMODE(PCGEX_CREATE_BLENDER) }
#undef PCGEX_CREATE_BLENDER

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

#pragma region Metadatablenders

	// Helpers to supersede Metadata & Union blenders
	// Regular ProxyBlender expected fully fledged descriptor as each source
	// can be pointing to a variety of this. Here, we're in total control of the inputs/outputs
	// There's no user-facing selection so we have a little bit more leeway
	// Also we don't support sub-selectors so that helps.

	// Create a blender for a single attribute
	// Source A, Source B, Target
	// Source doesn't have to be the same as target, second Source will the same as target.
	// And the same source can technically be used in all three but won't be threadsafe

	// We still need the FMetadataBlender to:
	// Prepare (reset target values)
	// Trigger actual blend
	// Finalize blend (divide if multiple blend ops happened)
}

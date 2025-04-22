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
	CopyTarget       = 7 UMETA(DisplayName = "Copy (Target)", ToolTip = "= A"),
	CopySource       = 8 UMETA(DisplayName = "Copy (Source)", ToolTip="= B"),
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
	/**
	 * Simple C=AxB blend
	 */
	class PCGEXTENDEDTOOLKIT_API FProxyDataBlenderBase : public TSharedFromThis<FProxyDataBlenderBase>
	{
	public:
		virtual ~FProxyDataBlenderBase()
		{
		}

		virtual void Blend(const int32 Index, FPCGPoint& Point, const double Weight = 1) = 0;
		virtual void Blend(const int32 SourceIndex, const FPCGPoint& SourcePoint, const int32 TargetIndex, FPCGPoint& TargetPoint, const double Weight = 1) = 0;
		virtual TSharedPtr<PCGExData::FBufferBase> GetOutputBuffer() const = 0;
	};

	template <typename T>
	class PCGEXTENDEDTOOLKIT_API TProxyDataBlenderBase : public FProxyDataBlenderBase
	{
	public:
		TSharedPtr<PCGExData::TBufferProxy<T>> A;
		TSharedPtr<PCGExData::TBufferProxy<T>> B;
		TSharedPtr<PCGExData::TBufferProxy<T>> C;

		virtual ~TProxyDataBlenderBase() override
		{
		}

		virtual void Blend(const int32 Index, FPCGPoint& Point, const double Weight = 1) override
		PCGEX_NOT_IMPLEMENTED(Blend(const int32 Index, FPCGPoint& Point, const double Weight = 1))

		virtual void Blend(const int32 SourceIndex, const FPCGPoint& SourcePoint, const int32 TargetIndex, FPCGPoint& TargetPoint, const double Weight = 1) override
		PCGEX_NOT_IMPLEMENTED(Blend(const int32 Index, FPCGPoint& Point, const double Weight = 1))

		virtual TSharedPtr<PCGExData::FBufferBase> GetOutputBuffer() const override { return C ? C->GetBuffer() : nullptr; }
	};

	template <typename T, EPCGExABBlendingType BLEND_MODE>
	class PCGEXTENDEDTOOLKIT_API TProxyDataBlender : public TProxyDataBlenderBase<T>
	{
		using TProxyDataBlenderBase<T>::A;
		using TProxyDataBlenderBase<T>::B;
		using TProxyDataBlenderBase<T>::C;

	public:
		virtual ~TProxyDataBlender() override
		{
		}

		virtual void Blend(const int32 Index, FPCGPoint& Point, const double Weight = 1) override
		{
			BOOKMARK_BLENDMODE

#define PCGEX_A A->Get(Index, Point)
#define PCGEX_B B->Get(Index, Point)

			if constexpr (BLEND_MODE == EPCGExABBlendingType::None)
			{
			}
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Average) { C->Set(Index, Point, PCGExBlend::Div(PCGExBlend::Add(PCGEX_A,PCGEX_B), 2)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Weight) { C->Set(Index, Point, PCGExBlend::Div(PCGExBlend::Add(PCGEX_A,PCGEX_B), Weight)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Min) { C->Set(Index, Point, PCGExBlend::Min(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Max) { C->Set(Index, Point, PCGExBlend::Max(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Add) { C->Set(Index, Point, PCGExBlend::Add(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Subtract) { C->Set(Index, Point, PCGExBlend::Sub(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Multiply) { C->Set(Index, Point, PCGExBlend::Mult(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Divide) { C->Set(Index, Point, PCGExBlend::Div(PCGEX_A, PCGEx::Convert<double>(PCGEX_B))); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::WeightedAdd) { C->Set(Index, Point, PCGExBlend::WeightedAdd(PCGEX_A,PCGEX_B, Weight)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::WeightedSubtract) { C->Set(Index, Point, PCGExBlend::WeightedSub(PCGEX_A,PCGEX_B, Weight)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Lerp) { C->Set(Index, Point, PCGExBlend::Lerp(PCGEX_A,PCGEX_B, Weight)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::UnsignedMin) { C->Set(Index, Point, PCGExBlend::UnsignedMin(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::UnsignedMax) { C->Set(Index, Point, PCGExBlend::UnsignedMax(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::AbsoluteMin) { C->Set(Index, Point, PCGExBlend::AbsoluteMin(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::AbsoluteMax) { C->Set(Index, Point, PCGExBlend::AbsoluteMax(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::CopyTarget) { C->Set(Index, Point, PCGEX_A); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::CopySource) { C->Set(Index, Point, PCGEX_B); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Hash) { C->Set(Index, Point, PCGExBlend::NaiveHash(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::UnsignedHash) { C->Set(Index, Point, PCGExBlend::NaiveUnsignedHash(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Mod) { C->Set(Index, Point, PCGExBlend::ModSimple(PCGEX_A, PCGEx::Convert<T, double>(B->Get(Index, Point)))); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::ModCW) { C->Set(Index, Point, PCGExBlend::ModComplex(PCGEX_A,PCGEX_B)); }

#undef PCGEX_A
#undef PCGEX_B
		}

		virtual void Blend(const int32 SourceIndex, const FPCGPoint& SourcePoint, const int32 TargetIndex, FPCGPoint& TargetPoint, const double Weight = 1) override
		{
			BOOKMARK_BLENDMODE

#define PCGEX_A A->Get(SourceIndex, SourcePoint)
#define PCGEX_B B->Get(TargetIndex, TargetPoint)

			if constexpr (BLEND_MODE == EPCGExABBlendingType::None)
			{
			}
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Average) { C->Set(TargetIndex, TargetPoint, PCGExBlend::Div(PCGExBlend::Add(PCGEX_A,PCGEX_B), 2)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Weight) { C->Set(TargetIndex, TargetPoint, PCGExBlend::Div(PCGExBlend::Add(PCGEX_A,PCGEX_B), Weight)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Min) { C->Set(TargetIndex, TargetPoint, PCGExBlend::Min(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Max) { C->Set(TargetIndex, TargetPoint, PCGExBlend::Max(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Add) { C->Set(TargetIndex, TargetPoint, PCGExBlend::Add(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Subtract) { C->Set(TargetIndex, TargetPoint, PCGExBlend::Sub(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Multiply) { C->Set(TargetIndex, TargetPoint, PCGExBlend::Mult(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Divide) { C->Set(TargetIndex, TargetPoint, PCGExBlend::Div(PCGEX_A, PCGEx::Convert<double>(PCGEX_B))); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::WeightedAdd) { C->Set(TargetIndex, TargetPoint, PCGExBlend::WeightedAdd(PCGEX_A,PCGEX_B, Weight)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::WeightedSubtract) { C->Set(TargetIndex, TargetPoint, PCGExBlend::WeightedSub(PCGEX_A,PCGEX_B, Weight)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Lerp) { C->Set(TargetIndex, TargetPoint, PCGExBlend::Lerp(PCGEX_A,PCGEX_B, Weight)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::UnsignedMin) { C->Set(TargetIndex, TargetPoint, PCGExBlend::UnsignedMin(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::UnsignedMax) { C->Set(TargetIndex, TargetPoint, PCGExBlend::UnsignedMax(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::AbsoluteMin) { C->Set(TargetIndex, TargetPoint, PCGExBlend::AbsoluteMin(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::AbsoluteMax) { C->Set(TargetIndex, TargetPoint, PCGExBlend::AbsoluteMax(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::CopyTarget) { C->Set(TargetIndex, TargetPoint, PCGEX_A); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::CopySource) { C->Set(TargetIndex, TargetPoint, PCGEX_B); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Hash) { C->Set(TargetIndex, TargetPoint, PCGExBlend::NaiveHash(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::UnsignedHash) { C->Set(TargetIndex, TargetPoint, PCGExBlend::NaiveUnsignedHash(PCGEX_A,PCGEX_B)); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::Mod) { C->Set(TargetIndex, TargetPoint, PCGExBlend::ModSimple(PCGEX_A, PCGEx::Convert<T, double>(PCGEX_B))); }
			else if constexpr (BLEND_MODE == EPCGExABBlendingType::ModCW) { C->Set(TargetIndex, TargetPoint, PCGExBlend::ModComplex(PCGEX_A,PCGEX_B)); }

#undef PCGEX_A
#undef PCGEX_B
		}
	};

	static TSharedPtr<FProxyDataBlenderBase> CreateProxyBlender(
		FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade,
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

				TypedBlender->A = StaticCastSharedPtr<PCGExData::TBufferProxy<T>>(GetProxyBuffer(InContext, InDataFacade, A));
				TypedBlender->B = StaticCastSharedPtr<PCGExData::TBufferProxy<T>>(GetProxyBuffer(InContext, InDataFacade, B));
				TypedBlender->C = StaticCastSharedPtr<PCGExData::TBufferProxy<T>>(GetProxyBuffer(InContext, InDataFacade, C));

				if (!TypedBlender) { return; }
				if (!TypedBlender->A || !TypedBlender->B || !TypedBlender->C)
				{
					PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("ProxyBlender : Missing at least one proxy."));
				}

				OutBlender = TypedBlender;
			});


		return OutBlender;
	}
}

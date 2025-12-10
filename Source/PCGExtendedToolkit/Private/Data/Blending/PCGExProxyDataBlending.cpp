// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExProxyDataBlending.h"

#include "Data/PCGExPointIO.h"
#include "Data/PCGExProxyData.h"
#include "Data/PCGExProxyDataHelpers.h"
#include "Data/PCGExUnionData.h"
#include "Data/Blending/PCGExBlendModes.h"
#include "Details/PCGExDetailsDistances.h"

namespace PCGExDataBlending
{
	void FDummyUnionBlender::Init(const TSharedPtr<PCGExData::FFacade>& TargetData, const TArray<TSharedRef<PCGExData::FFacade>>& InSources)
	{
		CurrentTargetData = TargetData;

		int32 MaxIndex = 0;
		for (const TSharedRef<PCGExData::FFacade>& Src : InSources) { MaxIndex = FMath::Max(Src->Source->IOIndex, MaxIndex); }
		IOLookup = MakeShared<PCGEx::FIndexLookup>(MaxIndex + 1);
		for (const TSharedRef<PCGExData::FFacade>& Src : InSources) { IOLookup->Set(Src->Source->IOIndex, SourcesData.Add(Src->GetIn())); }

		Distances = PCGExDetails::MakeDistances();
	}

	int32 FDummyUnionBlender::ComputeWeights(const int32 WriteIndex, const TSharedPtr<PCGExData::IUnionData>& InUnionData, TArray<PCGExData::FWeightedPoint>& OutWeightedPoints) const
	{
		const PCGExData::FConstPoint Target = CurrentTargetData->Source->GetOutPoint(WriteIndex);
		return InUnionData->ComputeWeights(SourcesData, IOLookup, Target, Distances, OutWeightedPoints);
	}

	template <typename T>
	void FProxyDataBlender::Set(const int32 TargetIndex, const T Value) const
	{
#define PCGEX_DECL_BLEND_BIT(_TYPE, _NAME, ...) else if constexpr (std::is_same_v<T, _TYPE>){ Set##_NAME(TargetIndex, Value); }
		if constexpr (false)
		{
		} PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_DECL_BLEND_BIT)
#undef PCGEX_DECL_BLEND_BIT
	}

#define PCGEX_TPL(_TYPE, _NAME, ...) \
template PCGEXTENDEDTOOLKIT_API void FProxyDataBlender::Set<_TYPE>(const int32 TargetIndex, const _TYPE Value) const;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

	template <typename T_WORKING>
	IProxyDataBlender<T_WORKING>::IProxyDataBlender()
	{
		UnderlyingType = PCGEx::GetMetadataType<T_WORKING>();
	}

	template <typename T_WORKING>
	TSharedPtr<PCGExData::IBuffer> IProxyDataBlender<T_WORKING>::GetOutputBuffer() const
	{
		return C ? C->GetBuffer() : nullptr;
	}

	template <typename T_WORKING>
	bool IProxyDataBlender<T_WORKING>::InitFromParam(FPCGExContext* InContext, const FBlendingParam& InParam, const TSharedPtr<PCGExData::FFacade> InTargetFacade, const TSharedPtr<PCGExData::FFacade> InSourceFacade, const PCGExData::EIOSide InSide, const bool bWantsDirectAccess)
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

#define PCGEX_DECL_BLEND_BIT(_TYPE, _NAME, ...) \
	template <typename T_WORKING>\
	void IProxyDataBlender<T_WORKING>::Set##_NAME(const int32 TargetIndex, const _TYPE Value) const { C->Set(TargetIndex, PCGEx::Convert<_TYPE, T_WORKING>(Value)); };
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_DECL_BLEND_BIT)
#undef PCGEX_DECL_BLEND_BIT

	template <typename T_WORKING, EPCGExABBlendingType BLEND_MODE, bool bResetValueForMultiBlend>
	void TProxyDataBlender<T_WORKING, BLEND_MODE, bResetValueForMultiBlend>::Blend(const int32 SourceIndexA, const int32 SourceIndexB, const int32 TargetIndex, const double Weight)
	{
		BOOKMARK_BLENDMODE check(A)
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

	template <typename T_WORKING, EPCGExABBlendingType BLEND_MODE, bool bResetValueForMultiBlend>
	PCGEx::FOpStats TProxyDataBlender<T_WORKING, BLEND_MODE, bResetValueForMultiBlend>::BeginMultiBlend(const int32 TargetIndex)
	{
		check(C)

		PCGEx::FOpStats Tracker{};

		if constexpr (BLEND_MODE == EPCGExABBlendingType::Min || BLEND_MODE == EPCGExABBlendingType::Max || BLEND_MODE == EPCGExABBlendingType::UnsignedMin || BLEND_MODE == EPCGExABBlendingType::UnsignedMax || BLEND_MODE == EPCGExABBlendingType::AbsoluteMin || BLEND_MODE == EPCGExABBlendingType::AbsoluteMax || BLEND_MODE == EPCGExABBlendingType::Hash || BLEND_MODE == EPCGExABBlendingType::UnsignedHash)
		{
			// These modes require the first operation to be a copy of the value
			// before the can be properly blended
			Tracker.Count = -1;
		}
		else if constexpr (BLEND_MODE == EPCGExABBlendingType::Average || BLEND_MODE == EPCGExABBlendingType::Add || BLEND_MODE == EPCGExABBlendingType::Subtract || BLEND_MODE == EPCGExABBlendingType::Weight || BLEND_MODE == EPCGExABBlendingType::WeightedAdd || BLEND_MODE == EPCGExABBlendingType::WeightedSubtract)
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

	template <typename T_WORKING, EPCGExABBlendingType BLEND_MODE, bool bResetValueForMultiBlend>
	void TProxyDataBlender<T_WORKING, BLEND_MODE, bResetValueForMultiBlend>::MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double Weight, PCGEx::FOpStats& Tracker)
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

	template <typename T_WORKING, EPCGExABBlendingType BLEND_MODE, bool bResetValueForMultiBlend>
	void TProxyDataBlender<T_WORKING, BLEND_MODE, bResetValueForMultiBlend>::EndMultiBlend(const int32 TargetIndex, PCGEx::FOpStats& Tracker)
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

	template <typename T_WORKING, EPCGExABBlendingType BLEND_MODE, bool bResetValueForMultiBlend>
	void TProxyDataBlender<T_WORKING, BLEND_MODE, bResetValueForMultiBlend>::Div(const int32 TargetIndex, const double Divider)
	{
		C->Set(TargetIndex, PCGExBlend::Div(C->Get(TargetIndex), Divider));
	}

#pragma region externalization FProxyDataBlender

#define PCGEX_TPL(_TYPE, _NAME, ...)\
template class PCGEXTENDEDTOOLKIT_API IProxyDataBlender<_TYPE>;

	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)

#undef PCGEX_TPL

#pragma endregion

#pragma region externalization TProxyDataBlender

#define PCGEX_TPL(_TYPE, _NAME, _BLENDMODE)\
template class PCGEXTENDEDTOOLKIT_API TProxyDataBlender<_TYPE, EPCGExABBlendingType::_BLENDMODE, true>;\
template class PCGEXTENDEDTOOLKIT_API TProxyDataBlender<_TYPE, EPCGExABBlendingType::_BLENDMODE, false>;

#define PCGEX_TPL_LOOP(_BLENDMODE)	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL, _BLENDMODE)

	PCGEX_FOREACH_PROXYBLENDMODE(PCGEX_TPL_LOOP)

#undef PCGEX_TPL_LOOP
#undef PCGEX_TPL

#pragma endregion

	template <typename T>
	TSharedPtr<IProxyDataBlender<T>> CreateProxyBlender(const EPCGExABBlendingType BlendMode, const bool bResetValueForMultiBlend)
	{
		TSharedPtr<IProxyDataBlender<T>> OutBlender;

		if (bResetValueForMultiBlend)
		{
#define PCGEX_CREATE_BLENDER(_BLEND)case EPCGExABBlendingType::_BLEND : \
OutBlender = MakeShared<TProxyDataBlender<T, EPCGExABBlendingType::_BLEND, true>>(); \
break;
			switch (BlendMode) { PCGEX_FOREACH_PROXYBLENDMODE(PCGEX_CREATE_BLENDER) }
#undef PCGEX_CREATE_BLENDER
		}
		else
		{
#define PCGEX_CREATE_BLENDER(_BLEND)case EPCGExABBlendingType::_BLEND : \
OutBlender = MakeShared<TProxyDataBlender<T, EPCGExABBlendingType::_BLEND, false>>(); \
break;
			switch (BlendMode) { PCGEX_FOREACH_PROXYBLENDMODE(PCGEX_CREATE_BLENDER) }
#undef PCGEX_CREATE_BLENDER
		}

		return OutBlender;
	}

#define PCGEX_TPL(_TYPE, _NAME, ...) \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<IProxyDataBlender<_TYPE>> CreateProxyBlender<_TYPE>(const EPCGExABBlendingType BlendMode, const bool bResetValueForMultiBlend);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

	TSharedPtr<FProxyDataBlender> CreateProxyBlender(FPCGExContext* InContext, const EPCGExABBlendingType BlendMode, const PCGExData::FProxyDescriptor& A, const PCGExData::FProxyDescriptor& B, const PCGExData::FProxyDescriptor& C, const bool bResetValueForMultiBlend)
	{
		TSharedPtr<FProxyDataBlender> OutBlender;

		if (A.WorkingType != B.WorkingType || A.WorkingType != C.WorkingType)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("ProxyBlender : T_WORKING mismatch."));
			return nullptr;
		}

		PCGEx::ExecuteWithRightType(A.WorkingType, [&](auto DummyValue)
		{
			using T = decltype(DummyValue);

			TSharedPtr<IProxyDataBlender<T>> TypedBlender = CreateProxyBlender<T>(BlendMode, bResetValueForMultiBlend);

			if (!TypedBlender) { return; }

			// Create output first so we may read from it
			TypedBlender->C = StaticCastSharedPtr<PCGExData::TBufferProxy<T>>(GetProxyBuffer(InContext, C));
			TypedBlender->A = StaticCastSharedPtr<PCGExData::TBufferProxy<T>>(GetProxyBuffer(InContext, A));
			TypedBlender->B = StaticCastSharedPtr<PCGExData::TBufferProxy<T>>(GetProxyBuffer(InContext, B));

			if (!TypedBlender->A)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("ProxyBlender : Failed to generate buffer for Operand A."));
				return;
			}

			if (!TypedBlender->B)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("ProxyBlender : Failed to generate buffer for Operand B."));
				return;
			}

			if (!TypedBlender->C)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("ProxyBlender : Failed to generate buffer for Output."));
				return;
			}

			// Ensure C is readable for MultiBlend, as those will use GetCurrent
			if (!TypedBlender->C->EnsureReadable())
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Fail to ensure target write buffer is also readable."));
				return;
			}

			OutBlender = TypedBlender;
		});


		return OutBlender;
	}

	TSharedPtr<FProxyDataBlender> CreateProxyBlender(FPCGExContext* InContext, const EPCGExABBlendingType BlendMode, const PCGExData::FProxyDescriptor& A, const PCGExData::FProxyDescriptor& C, const bool bResetValueForMultiBlend)
	{
		TSharedPtr<FProxyDataBlender> OutBlender;

		if (A.WorkingType != C.WorkingType)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("ProxyBlender : T_WORKING mismatch."));
			return nullptr;
		}

		PCGEx::ExecuteWithRightType(A.WorkingType, [&](auto DummyValue)
		{
			using T = decltype(DummyValue);

			TSharedPtr<IProxyDataBlender<T>> TypedBlender = CreateProxyBlender<T>(BlendMode, bResetValueForMultiBlend);

			if (!TypedBlender) { return; }

			// Create output first so we may read from it
			TypedBlender->C = StaticCastSharedPtr<PCGExData::TBufferProxy<T>>(GetProxyBuffer(InContext, C));
			TypedBlender->A = StaticCastSharedPtr<PCGExData::TBufferProxy<T>>(GetProxyBuffer(InContext, A));
			TypedBlender->B = nullptr;

			if (!TypedBlender->A)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("ProxyBlender : Failed to generate buffer for Operand A."));
				return;
			}

			if (!TypedBlender->C)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("ProxyBlender : Failed to generate buffer for Output."));
				return;
			}

			// Ensure C is readable for MultiBlend, as those will use GetCurrent
			if (!TypedBlender->C->EnsureReadable())
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Fail to ensure target write buffer is also readable."));
				return;
			}

			OutBlender = TypedBlender;
		});


		return OutBlender;
	}
}

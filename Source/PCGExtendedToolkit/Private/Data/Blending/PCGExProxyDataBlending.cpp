// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/Blending/PCGExProxyDataBlending.h"

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
		}
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_DECL_BLEND_BIT)
#undef PCGEX_DECL_BLEND_BIT
	}

#define PCGEX_TPL(_TYPE, _NAME, ...) \
template PCGEXTENDEDTOOLKIT_API void FProxyDataBlender::Set<_TYPE>(const int32 TargetIndex, const _TYPE Value) const;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

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

		PCGEx::ExecuteWithRightType(
			A.WorkingType, [&](auto DummyValue)
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

		PCGEx::ExecuteWithRightType(
			A.WorkingType, [&](auto DummyValue)
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

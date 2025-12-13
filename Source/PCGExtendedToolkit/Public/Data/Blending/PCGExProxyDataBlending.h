// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Data/PCGExAttributeHelpers.h"
#include "Data/PCGExData.h"

#include "PCGExDataBlending.h"
#include "Types/PCGExTypeOps.h"
#include "Types/PCGExTypeOpsImpl.h"

namespace PCGExDetails
{
	class FDistances;
}

namespace PCGExData
{
	class IUnionData;

	template <typename T>
	class TBufferProxy;

	struct FProxyDescriptor;
}

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
		const PCGExDetails::FDistances* Distances = nullptr;
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

		virtual bool InitFromParam(FPCGExContext* InContext, const FBlendingParam& InParam, const TSharedPtr<PCGExData::FFacade> InTargetFacade, const TSharedPtr<PCGExData::FFacade> InSourceFacade, PCGExData::EIOSide InSide, bool bWantsDirectAccess = false) = 0;

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
	protected:
		const PCGExTypeOps::TTypeOpsImpl<T_WORKING>& TypeOpsImpl;
		
	public:
		TSharedPtr<PCGExData::TBufferProxy<T_WORKING>> A;
		TSharedPtr<PCGExData::TBufferProxy<T_WORKING>> B;
		TSharedPtr<PCGExData::TBufferProxy<T_WORKING>> C;

		IProxyDataBlender();

		virtual ~IProxyDataBlender() override = default;

		virtual void Blend(const int32 SourceIndexA, const int32 SourceIndexB, const int32 TargetIndex, const double Weight = 1) override PCGEX_NOT_IMPLEMENTED(Blend(const int32 SourceIndexA, const int32 SourceIndexB, const int32 TargetIndex, const double Weight = 1))

		virtual PCGEx::FOpStats BeginMultiBlend(const int32 TargetIndex) override PCGEX_NOT_IMPLEMENTED_RET(BeginMultiBlend(const int32 TargetIndex), PCGEx::FOpStats{})

		virtual void MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double Weight, PCGEx::FOpStats& Tracker) override PCGEX_NOT_IMPLEMENTED(MultiBlend(const int32 SourceIndex, const int32 TargetIndex, FBlendTracker& Tracker))

		virtual void EndMultiBlend(const int32 TargetIndex, PCGEx::FOpStats& Tracker) override PCGEX_NOT_IMPLEMENTED(EndMultiBlend(const int32 TargetIndex, FBlendTracker& Tracker))

		virtual void Div(const int32 TargetIndex, const double Divider) override PCGEX_NOT_IMPLEMENTED(Div(const int32 TargetIndex, const double Divider))

		virtual TSharedPtr<PCGExData::IBuffer> GetOutputBuffer() const override;

		virtual bool InitFromParam(FPCGExContext* InContext, const FBlendingParam& InParam, const TSharedPtr<PCGExData::FFacade> InTargetFacade, const TSharedPtr<PCGExData::FFacade> InSourceFacade, const PCGExData::EIOSide InSide, const bool bWantsDirectAccess = false) override;

	protected:
#define PCGEX_DECL_BLEND_BIT(_TYPE, _NAME, ...) virtual void Set##_NAME(const int32 TargetIndex, const _TYPE Value) const override;
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
		using IProxyDataBlender<T_WORKING>::TypeOpsImpl;
		using IProxyDataBlender<T_WORKING>::A;
		using IProxyDataBlender<T_WORKING>::B;
		using IProxyDataBlender<T_WORKING>::C;

	public:
		virtual ~TProxyDataBlender() override = default;

		virtual void Blend(const int32 SourceIndexA, const int32 SourceIndexB, const int32 TargetIndex, const double Weight = 1) override;

		virtual PCGEx::FOpStats BeginMultiBlend(const int32 TargetIndex) override;
		virtual void MultiBlend(const int32 SourceIndex, const int32 TargetIndex, const double Weight, PCGEx::FOpStats& Tracker) override;
		virtual void EndMultiBlend(const int32 TargetIndex, PCGEx::FOpStats& Tracker) override;

		// Target = Target / Divider
		// Useful for finalizing multi-source ops
		virtual void Div(const int32 TargetIndex, const double Divider) override;
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

	PCGEXTENDEDTOOLKIT_API TSharedPtr<FProxyDataBlender> CreateProxyBlender(FPCGExContext* InContext, const EPCGExABBlendingType BlendMode, const PCGExData::FProxyDescriptor& A, const PCGExData::FProxyDescriptor& B, const PCGExData::FProxyDescriptor& C, const bool bResetValueForMultiBlend = true);

	PCGEXTENDEDTOOLKIT_API TSharedPtr<FProxyDataBlender> CreateProxyBlender(FPCGExContext* InContext, const EPCGExABBlendingType BlendMode, const PCGExData::FProxyDescriptor& A, const PCGExData::FProxyDescriptor& C, const bool bResetValueForMultiBlend = true);
}

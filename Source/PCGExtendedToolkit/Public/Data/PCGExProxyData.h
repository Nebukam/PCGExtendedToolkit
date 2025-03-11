// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExAttributeHelpers.h"
#include "PCGExData.h"

namespace PCGExData
{
	class FBufferProxyBase : public TSharedFromThis<FBufferProxyBase>
	{
	public:
		virtual ~FBufferProxyBase() = default;
	};

	template <typename T>
	class TBufferProxy : public FBufferProxyBase
	{
	public:
		PCGEx::FSubSelection SubSelection;
		virtual T Get(const int32 Index, FPCGPoint& Point) const = 0;
		virtual void Set(const int32 Index, FPCGPoint& Point, const T& Value) const = 0;
	};

	template <typename T_REAL, typename T_WORKING, bool bConverting>
	class TAttributeBufferProxy : public TBufferProxy<T_WORKING>
	{
		using TBufferProxy<T_WORKING>::SubSelection;

	public:
		virtual T_WORKING Get(const int32 Index, FPCGPoint& Point) const override
		{
			// i.e get Rotation<FQuat>.Forward<FVector> as <double>
			//					^ T_REAL	  ^ Sub		      ^ T_WORKING
			if constexpr (bConverting) { return PCGEx::Convert<T_REAL, T_WORKING>(Buffer->Read(Index)); }
			else { return SubSelection.template Get<T_REAL, T_WORKING>(Buffer->Read(Index)); }
		}

		virtual void Set(const int32 Index, FPCGPoint& Point, const T_WORKING& Value) const override
		{
			// i.e set Rotation<FQuat>.Forward<FVector> from <FRotator>
			//					^ T_REAL	  ^ Sub		      ^ T_WORKING
			if constexpr (bConverting) { Buffer->GetMutable(Index) = PCGEx::Convert<T_WORKING, T_REAL>(Value); }
			else { Buffer->GetMutable(Index) = SubSelection.template Get<T_WORKING, T_REAL>(Value); }
		}

	protected:
		TSharedPtr<TBuffer<T_REAL>> Buffer;
	};

	template <typename T_REAL, typename T_WORKING, bool bConverting, EPCGPointProperties PROPERTY>
	class TPointPropertyProxy : public TBufferProxy<T_WORKING>
	{
		using TBufferProxy<T_WORKING>::SubSelection;

	public:
		virtual T_WORKING Get(const int32 Index, FPCGPoint& Point) const override
		{
#define PCGEX_GET_SUBPROPERTY(_ACCESSOR, _TYPE) \
			if constexpr (bConverting){ return PCGEx::Convert<T_REAL, T_WORKING>(Point._ACCESSOR); } \
			else{ return SubSelection.template Get<T_REAL, T_WORKING>(Point._ACCESSOR); }

			PCGEX_CONSTEXPR_IFELSE_GETPOINTPROPERTY(PROPERTY, PCGEX_GET_SUBPROPERTY)
#undef PCGEX_GET_SUBPROPERTY
			else { return T_WORKING{}; }
		}

		virtual void Set(const int32 Index, FPCGPoint& Point, const T_WORKING& Value) const override
		{
#define PCGEX_STATIC_POINTPROPERTY_SETTER(_TYPE) PCGEx::Convert<T_WORKING, _TYPE>(Value)
			PCGEX_CONSTEXPR_IFELSE_SETPOINTPROPERTY(PROPERTY, Point, PCGEX_STATIC_POINTPROPERTY_SETTER)
#undef PCGEX_STATIC_POINTPROPERTY_SETTER
		}
	};

	template <typename T_REAL, typename T_WORKING, bool bConverting, EPCGExtraProperties PROPERTY>
	class TPointExtraPropertyProxy : public TBufferProxy<T_WORKING>
	{
		using TBufferProxy<T_WORKING>::SubSelection;

	public:
		virtual T_WORKING Get(const int32 Index, FPCGPoint& Point) const override
		{
			if constexpr (PROPERTY == EPCGExtraProperties::Index)
			{
				return PCGEx::Convert<T_REAL, T_WORKING>(Index);
			}
			else { return T_WORKING{}; }
		}

		virtual void Set(const int32 Index, FPCGPoint& Point, const T_WORKING& Value) const override
		{
			// Well, no
		}
	};

	template <typename T>
	static TSharedPtr<TBufferProxy<T>> GetProxyBuffer(FPCGExContext* InContext, const TSharedPtr<FFacade>& InDataFacade, const FPCGAttributePropertyInputSelector& Selector)
	{
		const PCGEx::FAttributeProcessingInfos ProcessingInfos = PCGEx::FAttributeProcessingInfos(InDataFacade->Source->GetOutIn(), Selector);
		if (!ProcessingInfos.bIsValid) { return nullptr; }

		const PCGEx::FSubSelection& Sel = ProcessingInfos.SubSelection;

		TSharedPtr<TBufferProxy<T>> OutBuffer;
		if (ProcessingInfos.Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			EPCGMetadataTypes RealType = static_cast<EPCGMetadataTypes>(ProcessingInfos.Attribute->GetTypeId());

			if (Sel.bIsValid)
			{
				// Converting
			}
			else
			{
				// Non-converting
			}
		}
		else if (ProcessingInfos.Selector.GetSelection() == EPCGAttributePropertySelection::PointProperty)
		{
			EPCGMetadataTypes WorkingType = EPCGMetadataTypes::Unknown;

			if (Sel.bIsValid)
			{
				// Converting
			}
			else
			{
				// Non-converting
			}
		}
		else if (ProcessingInfos.Selector.GetSelection() == EPCGAttributePropertySelection::ExtraProperty)
		{
			EPCGMetadataTypes WorkingType = EPCGMetadataTypes::Unknown;

			if (Sel.bIsValid)
			{
				// Converting
			}
			else
			{
				// Non-converting
			}
		}

		return OutBuffer;
	}

	template <typename T>
	TSharedPtr<FBufferProxyBase> GetProxyBuffer(FPCGExContext* InContext, const TSharedPtr<FFacade>& InDataFacade, FString AttributePath)
	{
		FPCGAttributePropertyInputSelector Selector = FPCGAttributePropertyInputSelector();
		Selector.Update(AttributePath);
		return GetProxyBuffer<T>(InContext, InDataFacade, Selector);
	}
}

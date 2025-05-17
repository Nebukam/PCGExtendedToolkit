// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExAttributeHelpers.h"
#include "PCGExData.h"

namespace PCGExData
{
	struct PCGEXTENDEDTOOLKIT_API FProxyDescriptor
	{
		FPCGAttributePropertyInputSelector Selector;
		PCGEx::FSubSelection SubSelection;

		EIOSide Side = EIOSide::In;

		EPCGMetadataTypes RealType = EPCGMetadataTypes::Unknown;
		EPCGMetadataTypes WorkingType = EPCGMetadataTypes::Unknown;

		TWeakPtr<FFacade> DataFacade;

		// For roaming properties only, isn't widely supported.
		bool bReadOnly = false;
		const UPCGBasePointData* PointData = nullptr;
		
		bool bIsConstant = false;

		FProxyDescriptor()
		{
		}

		explicit FProxyDescriptor(const TSharedPtr<FFacade>& InDataFacade)
			: DataFacade(InDataFacade)
		{
		}

		~FProxyDescriptor() = default;
		void UpdateSubSelection();
		bool SetFieldIndex(const int32 InFieldIndex);

		bool Capture(FPCGExContext* InContext, const FString& Path, const EIOSide InSide = EIOSide::Out, const bool bThrowError = true);
		bool Capture(FPCGExContext* InContext, const FPCGAttributePropertyInputSelector& InSelector, const EIOSide InSide = EIOSide::Out, const bool bThrowError = true);

		bool CaptureStrict(FPCGExContext* InContext, const FString& Path, const EIOSide InSide = EIOSide::Out, const bool bThrowError = true);
		bool CaptureStrict(FPCGExContext* InContext, const FPCGAttributePropertyInputSelector& InSelector, const EIOSide InSide = EIOSide::Out, const bool bThrowError = true);

		//static FProxyDescriptor CreateForPointProperty(UPCGBasePointData* PointData);
	};

	class FBufferProxyBase : public TSharedFromThis<FBufferProxyBase>
	{
	public:
		UPCGBasePointData* Data = nullptr;
		PCGEx::FSubSelection SubSelection;
		EPCGMetadataTypes RealType = EPCGMetadataTypes::Unknown;
		EPCGMetadataTypes WorkingType = EPCGMetadataTypes::Unknown;

		FBufferProxyBase() = default;
		virtual ~FBufferProxyBase() = default;

		bool Validate(const FProxyDescriptor& Descriptor) const { return Descriptor.RealType == RealType && Descriptor.WorkingType == WorkingType; }
		virtual TSharedPtr<FBufferBase> GetBuffer() const { return nullptr; }
	};

	template <typename T>
	class TBufferProxy : public FBufferProxyBase
	{
	public:
		TBufferProxy()
			: FBufferProxyBase()
		{
			WorkingType = PCGEx::GetMetadataType<T>();
		}

		virtual T Get(const int32 Index) const = 0;
		virtual void Set(const int32 Index, const T& Value) const = 0;
		virtual TSharedPtr<FBufferBase> GetBuffer() const override { return nullptr; }
	};

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	class TAttributeBufferProxy : public TBufferProxy<T_WORKING>
	{
		using TBufferProxy<T_WORKING>::SubSelection;

	public:
		TSharedPtr<TBuffer<T_REAL>> Buffer;

		TAttributeBufferProxy()
			: TBufferProxy<T_WORKING>()
		{
			this->RealType = PCGEx::GetMetadataType<T_REAL>();
		}

		virtual T_WORKING Get(const int32 Index) const override
		{
			// i.e get Rotation<FQuat>.Forward<FVector> as <double>
			//					^ T_REAL	  ^ Sub		      ^ T_WORKING
			if constexpr (!bSubSelection)
			{
				if constexpr (std::is_same_v<T_REAL, T_WORKING>) { return Buffer->Read(Index); }
				else { return PCGEx::Convert<T_REAL, T_WORKING>(Buffer->Read(Index)); }
			}
			else { return SubSelection.template Get<T_REAL, T_WORKING>(Buffer->Read(Index)); }
		}

		virtual void Set(const int32 Index, const T_WORKING& Value) const override
		{
			// i.e set Rotation<FQuat>.Forward<FVector> from <FRotator>
			//					^ T_REAL	  ^ Sub		      ^ T_WORKING
			if constexpr (!bSubSelection)
			{
				if constexpr (std::is_same_v<T_REAL, T_WORKING>) { Buffer->GetMutable(Index) = Value; }
				else { Buffer->GetMutable(Index) = PCGEx::Convert<T_WORKING, T_REAL>(Value); }
			}
			else
			{
				SubSelection.template Set<T_REAL, T_WORKING>(Buffer->GetMutable(Index), Value);
			}
		}

		virtual TSharedPtr<FBufferBase> GetBuffer() const override { return Buffer; }
	};

	template <typename T_REAL, typename T_WORKING, bool bSubSelection, EPCGPointProperties PROPERTY, typename T_VALUERANGE>
	class TPointPropertyProxy : public TBufferProxy<T_WORKING>
	{
		using TBufferProxy<T_WORKING>::SubSelection;
		using TBufferProxy<T_WORKING>::Data;
		
	public:
		TPointPropertyProxy()
			: TBufferProxy<T_WORKING>()
		{
			this->RealType = PCGEx::GetMetadataType<T_REAL>();
		}

		virtual T_WORKING Get(const int32 Index) const override
		{
#define PCGEX_GET_SUBPROPERTY(_ACCESSOR, _TYPE) \
			if constexpr (!bSubSelection){ \
				if constexpr (std::is_same_v<T_REAL, T_WORKING>) { return Data->_ACCESSOR; }\
				else{ return PCGEx::Convert<T_REAL, T_WORKING>(Data->_ACCESSOR); }\
			}else{ return SubSelection.template Get<T_REAL, T_WORKING>(Data->_ACCESSOR); }

			PCGEX_CONSTEXPR_IFELSE_GETPOINTPROPERTY(PROPERTY, PCGEX_GET_SUBPROPERTY)
#undef PCGEX_GET_SUBPROPERTY
			else { return T_WORKING{}; }
		}

		virtual void Set(const int32 Index, const T_WORKING& Value) const override
		{
			if constexpr (!bSubSelection)
			{
#define PCGEX_PROPERTY_VALUE(_TYPE) PCGEx::Convert<T_WORKING, _TYPE>(Value)
				PCGEX_CONSTEXPR_IFELSE_SETPOINTPROPERTY(PROPERTY, Data, PCGEX_MACRO_NONE, PCGEX_PROPERTY_VALUE)
#undef PCGEX_PROPERTY_VALUE
			}
			else
			{
				T_REAL V = T_REAL{};
#define PCGEX_GET_REAL(_ACCESSOR, _TYPE) V = Data->_ACCESSOR;
				PCGEX_CONSTEXPR_IFELSE_GETPOINTPROPERTY(PROPERTY, PCGEX_GET_REAL)
#undef PCGEX_GET_REAL

#define PCGEX_PROPERTY_SET(_TYPE) SubSelection.template Set<T_REAL, T_WORKING>(V, Value);
#define PCGEX_PROPERTY_VALUE(_TYPE) V
				PCGEX_CONSTEXPR_IFELSE_SETPOINTPROPERTY(PROPERTY, Data, PCGEX_PROPERTY_SET, PCGEX_PROPERTY_VALUE)
#undef PCGEX_PROPERTY_VALUE
#undef PCGEX_PROPERTY_SET
			}
		}
	};

	template <typename T_REAL, typename T_WORKING, bool bSubSelection, EPCGExtraProperties PROPERTY>
	class TPointExtraPropertyProxy : public TBufferProxy<T_WORKING>
	{
		using TBufferProxy<T_WORKING>::SubSelection;

	public:
		TSharedPtr<TBuffer<T_REAL>> Buffer;

		TPointExtraPropertyProxy()
			: TBufferProxy<T_WORKING>()
		{
			this->RealType = PCGEx::GetMetadataType<T_REAL>();
		}

		virtual T_WORKING Get(const int32 Index) const override
		{
			if constexpr (PROPERTY == EPCGExtraProperties::Index)
			{
				return PCGEx::Convert<T_REAL, T_WORKING>(Index);
			}
			else { return T_WORKING{}; }
		}

		virtual void Set(const int32 Index, const T_WORKING& Value) const override
		{
			// Well, no
		}
	};

	template <typename T_WORKING>
	class TConstantProxy : public TBufferProxy<T_WORKING>
	{
		T_WORKING Constant = T_WORKING{};

	public:
		TConstantProxy()
			: TBufferProxy<T_WORKING>()
		{
			this->RealType = PCGEx::GetMetadataType<T_WORKING>();
		}

		template <typename T>
		void SetConstant(const T& InValue)
		{
			Constant = PCGEx::Convert<T, T_WORKING>(InValue);
		}

		virtual T_WORKING Get(const int32 Index) const override
		{
			return Constant;
		}

		virtual void Set(const int32 Index, const T_WORKING& Value) const override
		{
			// This should never happen, check the callstack
			check(false)
		}
	};

	TSharedPtr<FBufferProxyBase> GetProxyBuffer(
		FPCGExContext* InContext,
		const FProxyDescriptor& InDescriptor);

	template <typename T>
	TSharedPtr<FBufferProxyBase> GetConstantProxyBuffer(const T& Constant)
	{
		TSharedPtr<TConstantProxy<T>> TypedProxy = MakeShared<TConstantProxy<T>>();
		TypedProxy->SetConstant(Constant);
		return TypedProxy;
	}

	bool GetPerFieldProxyBuffers(
		FPCGExContext* InContext,
		const FProxyDescriptor& InBaseDescriptor,
		const int32 NumDesiredFields,
		TArray<TSharedPtr<FBufferProxyBase>>& OutProxies);
}

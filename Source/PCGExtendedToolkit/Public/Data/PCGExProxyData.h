// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "PCGExAttributeHelpers.h"
#include "PCGExData.h"

namespace PCGExData
{
	enum class EProxyRole : uint8
	{
		Read,
		Write
	};

	struct PCGEXTENDEDTOOLKIT_API FProxyDescriptor
	{
		FPCGAttributePropertyInputSelector Selector;
		PCGEx::FSubSelection SubSelection;

		EIOSide Side = EIOSide::In;
		EProxyRole Role = EProxyRole::Read;

		EPCGMetadataTypes RealType = EPCGMetadataTypes::Unknown;
		EPCGMetadataTypes WorkingType = EPCGMetadataTypes::Unknown;

		TWeakPtr<FFacade> DataFacade;

		const UPCGBasePointData* PointData = nullptr;

		bool bIsConstant = false;
		bool bWantsDirect = false;

		FProxyDescriptor()
		{
		}

		explicit FProxyDescriptor(const TSharedPtr<FFacade>& InDataFacade, const EProxyRole InRole = EProxyRole::Read)
			: Role(InRole), DataFacade(InDataFacade)
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

	class IBufferProxy : public TSharedFromThis<IBufferProxy>
	{
	public:
		UPCGBasePointData* Data = nullptr;
		PCGEx::FSubSelection SubSelection;
		EPCGMetadataTypes RealType = EPCGMetadataTypes::Unknown;
		EPCGMetadataTypes WorkingType = EPCGMetadataTypes::Unknown;

#if WITH_EDITOR
		// This is purely for debugging purposes
		FProxyDescriptor Descriptor;
#endif

		IBufferProxy() = default;
		virtual ~IBufferProxy() = default;

		virtual bool Validate(const FProxyDescriptor& InDescriptor) const { return InDescriptor.RealType == RealType && InDescriptor.WorkingType == WorkingType; }
		virtual TSharedPtr<IBuffer> GetBuffer() const { return nullptr; }
		virtual bool EnsureReadable() const { return true; }

#define PCGEX_CONVERTING_READ(_TYPE, _NAME, ...) FORCEINLINE virtual _TYPE ReadAs##_NAME(const int32 Index) const PCGEX_NOT_IMPLEMENTED_RET(ReadAs##_NAME, _TYPE{})
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_CONVERTING_READ)
#undef PCGEX_CONVERTING_READ
	};


	template <typename T_WORKING>
	class TBufferProxy : public IBufferProxy
	{
	public:
		TBufferProxy()
			: IBufferProxy()
		{
			WorkingType = PCGEx::GetMetadataType<T_WORKING>();
		}

		virtual T_WORKING Get(const int32 Index) const = 0;
		virtual void Set(const int32 Index, const T_WORKING& Value) const = 0;
		virtual T_WORKING GetCurrent(const int32 Index) const { return Get(Index); };
		virtual TSharedPtr<IBuffer> GetBuffer() const override { return nullptr; }

#define PCGEX_CONVERTING_READ(_TYPE, _NAME, ...) FORCEINLINE virtual _TYPE ReadAs##_NAME(const int32 Index) const override { \
		if constexpr (std::is_same_v<_TYPE, T_WORKING>) { return Get(Index); } \
		else { return PCGEx::Convert<T_WORKING, _TYPE>(Get(Index)); } \
	}
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_CONVERTING_READ)
#undef PCGEX_CONVERTING_READ
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
				if constexpr (std::is_same_v<T_REAL, T_WORKING>) { Buffer->SetValue(Index, Value); }
				else { Buffer->SetValue(Index, PCGEx::Convert<T_WORKING, T_REAL>(Value)); }
			}
			else
			{
				T_REAL V = Buffer->GetValue(Index);
				SubSelection.template Set<T_REAL, T_WORKING>(V, Value);
				Buffer->SetValue(Index, V);
			}
		}

		virtual T_WORKING GetCurrent(const int32 Index) const override
		{
			// i.e get Rotation<FQuat>.Forward<FVector> as <double>
			//					^ T_REAL	  ^ Sub		      ^ T_WORKING
			if constexpr (!bSubSelection)
			{
				if constexpr (std::is_same_v<T_REAL, T_WORKING>) { return Buffer->GetValue(Index); }
				else { return PCGEx::Convert<T_REAL, T_WORKING>(Buffer->GetValue(Index)); }
			}
			else { return SubSelection.template Get<T_REAL, T_WORKING>(Buffer->GetValue(Index)); }
		}

		virtual TSharedPtr<IBuffer> GetBuffer() const override { return Buffer; }
		virtual bool EnsureReadable() const override { return Buffer->EnsureReadable(); }
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
				if constexpr (std::is_same_v<T_REAL, T_WORKING>)
				{
#define PCGEX_PROPERTY_VALUE(_TYPE) Value
					PCGEX_CONSTEXPR_IFELSE_SETPOINTPROPERTY(PROPERTY, Data, PCGEX_MACRO_NONE, PCGEX_PROPERTY_VALUE)
#undef PCGEX_PROPERTY_VALUE
				}
				else
				{
#define PCGEX_PROPERTY_VALUE(_TYPE) PCGEx::Convert<T_WORKING, _TYPE>(Value)
					PCGEX_CONSTEXPR_IFELSE_SETPOINTPROPERTY(PROPERTY, Data, PCGEX_MACRO_NONE, PCGEX_PROPERTY_VALUE)
#undef PCGEX_PROPERTY_VALUE
				}
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
			else
			{
				return T_WORKING{};
			}
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

		virtual bool Validate(const FProxyDescriptor& InDescriptor) const override { return InDescriptor.WorkingType == this->WorkingType; }
	};

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	class TDirectAttributeProxy : public TBufferProxy<T_WORKING>
	{
		// A memory-friendly but super slow proxy version that works with Setter/Getter on the attribute
		// TODO : Implement support for this to replace old "soft" metadata ops

		using TBufferProxy<T_WORKING>::SubSelection;
		using TBufferProxy<T_WORKING>::Data;

	public:
		FPCGMetadataAttribute<T_REAL>* InAttribute = nullptr;
		FPCGMetadataAttribute<T_REAL>* OutAttribute = nullptr;

		TDirectAttributeProxy()
			: TBufferProxy<T_WORKING>()
		{
			this->RealType = PCGEx::GetMetadataType<T_REAL>();
		}

		virtual T_WORKING Get(const int32 Index) const override
		{
			const int64 MetadataEntry = Data->GetMetadataEntry(Index);

			// i.e get Rotation<FQuat>.Forward<FVector> as <double>
			//					^ T_REAL	  ^ Sub		      ^ T_WORKING
			if constexpr (!bSubSelection)
			{
				if constexpr (std::is_same_v<T_REAL, T_WORKING>) { return InAttribute->GetValueFromItemKey(MetadataEntry); }
				else { return PCGEx::Convert<T_REAL, T_WORKING>(InAttribute->GetValueFromItemKey(MetadataEntry)); }
			}
			else
			{
				return SubSelection.template Get<T_REAL, T_WORKING>(InAttribute->GetValueFromItemKey(MetadataEntry));
			}
		}

		virtual T_WORKING GetCurrent(const int32 Index) const override
		{
			const int64 MetadataEntry = Data->GetMetadataEntry(Index);

			// i.e get Rotation<FQuat>.Forward<FVector> as <double>
			//					^ T_REAL	  ^ Sub		      ^ T_WORKING
			if constexpr (!bSubSelection)
			{
				if constexpr (std::is_same_v<T_REAL, T_WORKING>) { return OutAttribute->GetValueFromItemKey(MetadataEntry); }
				else { return PCGEx::Convert<T_REAL, T_WORKING>(OutAttribute->GetValueFromItemKey(MetadataEntry)); }
			}
			else
			{
				return SubSelection.template Get<T_REAL, T_WORKING>(OutAttribute->GetValueFromItemKey(MetadataEntry));
			}
		}

		virtual void Set(const int32 Index, const T_WORKING& Value) const override
		{
			const int64 MetadataEntry = Data->GetMetadataEntry(Index);

			// i.e set Rotation<FQuat>.Forward<FVector> from <FRotator>
			//					^ T_REAL	  ^ Sub		      ^ T_WORKING
			if constexpr (!bSubSelection)
			{
				if constexpr (std::is_same_v<T_REAL, T_WORKING>) { OutAttribute->SetValue(MetadataEntry, Value); }
				else { OutAttribute->SetValue(MetadataEntry, PCGEx::Convert<T_WORKING, T_REAL>(Value)); }
			}
			else
			{
				T_REAL V = OutAttribute->GetValueFromItemKey(MetadataEntry);
				SubSelection.template Set<T_REAL, T_WORKING>(V, Value);
				OutAttribute->SetValue(MetadataEntry, V);
			}
		}
	};

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	class TDirectDataAttributeProxy : public TBufferProxy<T_WORKING>
	{
		// A memory-friendly but super slow proxy version that works with Setter/Getter on the attribute
		// TODO : Implement support for this to replace old "soft" metadata ops

		using TBufferProxy<T_WORKING>::SubSelection;
		using TBufferProxy<T_WORKING>::Data;

	public:
		FPCGMetadataAttribute<T_REAL>* InAttribute = nullptr;
		FPCGMetadataAttribute<T_REAL>* OutAttribute = nullptr;

		TDirectDataAttributeProxy()
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
				if constexpr (std::is_same_v<T_REAL, T_WORKING>) { return InAttribute->GetValueFromItemKey(PCGDefaultValueKey); }
				else { return PCGEx::Convert<T_REAL, T_WORKING>(InAttribute->GetValueFromItemKey(PCGDefaultValueKey)); }
			}
			else
			{
				return SubSelection.template Get<T_REAL, T_WORKING>(InAttribute->GetValueFromItemKey(PCGDefaultValueKey));
			}
		}

		virtual T_WORKING GetCurrent(const int32 Index) const override
		{
			// i.e get Rotation<FQuat>.Forward<FVector> as <double>
			//					^ T_REAL	  ^ Sub		      ^ T_WORKING
			if constexpr (!bSubSelection)
			{
				if constexpr (std::is_same_v<T_REAL, T_WORKING>) { return OutAttribute->GetValueFromItemKey(PCGDefaultValueKey); }
				else { return PCGEx::Convert<T_REAL, T_WORKING>(OutAttribute->GetValueFromItemKey(PCGDefaultValueKey)); }
			}
			else
			{
				return SubSelection.template Get<T_REAL, T_WORKING>(OutAttribute->GetValueFromItemKey(PCGDefaultValueKey));
			}
		}

		virtual void Set(const int32 Index, const T_WORKING& Value) const override
		{
			// i.e set Rotation<FQuat>.Forward<FVector> from <FRotator>
			//					^ T_REAL	  ^ Sub		      ^ T_WORKING
			if constexpr (!bSubSelection)
			{
				if constexpr (std::is_same_v<T_REAL, T_WORKING>) { OutAttribute->SetDefaultValue(Value); }
				else { OutAttribute->SetDefaultValue(PCGEx::Convert<T_WORKING, T_REAL>(Value)); }
			}
			else
			{
				T_REAL V = OutAttribute->GetValueFromItemKey(PCGDefaultValueKey);
				SubSelection.template Set<T_REAL, T_WORKING>(V, Value);
				OutAttribute->SetDefaultValue(V);
			}
		}
	};

	TSharedPtr<IBufferProxy> GetProxyBuffer(
		FPCGExContext* InContext,
		const FProxyDescriptor& InDescriptor);

	template <typename T>
	TSharedPtr<IBufferProxy> GetConstantProxyBuffer(const T& Constant)
	{
		TSharedPtr<TConstantProxy<T>> TypedProxy = MakeShared<TConstantProxy<T>>();
		TypedProxy->SetConstant(Constant);
		return TypedProxy;
	}

	bool GetPerFieldProxyBuffers(
		FPCGExContext* InContext,
		const FProxyDescriptor& InBaseDescriptor,
		const int32 NumDesiredFields,
		TArray<TSharedPtr<IBufferProxy>>& OutProxies);
}

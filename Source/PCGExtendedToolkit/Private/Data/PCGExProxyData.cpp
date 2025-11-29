// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExProxyData.h"

#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExValueHash.h"

namespace PCGExData
{
#pragma region FProxyDescriptor

	void FProxyDescriptor::UpdateSubSelection()
	{
		SubSelection = PCGEx::FSubSelection(Selector);
	}

	bool FProxyDescriptor::SetFieldIndex(const int32 InFieldIndex)
	{
		if (SubSelection.SetFieldIndex(InFieldIndex))
		{
			WorkingType = EPCGMetadataTypes::Double;
			return true;
		}

		return false;
	}

	bool FProxyDescriptor::Capture(FPCGExContext* InContext, const FString& Path, const EIOSide InSide, const bool bRequired)
	{
		const TSharedPtr<FFacade> InFacade = DataFacade.Pin();
		check(InFacade);

		bool bValid = true;

		Selector = FPCGAttributePropertyInputSelector();
		Selector.Update(Path);

		Side = InSide;

		if (!PCGEx::TryGetTypeAndSource(Selector, InFacade, RealType, Side))
		{
			if (bRequired) { PCGEX_LOG_INVALID_SELECTOR_C(InContext, , Selector) }
			bValid = false;
		}

		Selector = Selector.CopyAndFixLast(InFacade->Source->GetData(Side));

		UpdateSubSelection();
		WorkingType = SubSelection.GetSubType(RealType);

		return bValid;
	}

	bool FProxyDescriptor::Capture(FPCGExContext* InContext, const FPCGAttributePropertyInputSelector& InSelector, const EIOSide InSide, const bool bRequired)
	{
		const TSharedPtr<FFacade> InFacade = DataFacade.Pin();
		check(InFacade);

		bool bValid = true;
		Side = bIsConstant ? EIOSide::In : InSide;

		if (!PCGEx::TryGetTypeAndSource(InSelector, InFacade, RealType, Side))
		{
			if (bRequired) { PCGEX_LOG_INVALID_SELECTOR_C(InContext, , InSelector) }
			bValid = false;
		}

		PointData = InFacade->Source->GetData(Side);
		Selector = InSelector.CopyAndFixLast(InFacade->Source->GetData(Side));

		UpdateSubSelection();
		WorkingType = SubSelection.GetSubType(RealType);

		return bValid;
	}

	bool FProxyDescriptor::CaptureStrict(FPCGExContext* InContext, const FString& Path, const EIOSide InSide, const bool bRequired)
	{
		if (!Capture(InContext, Path, InSide, bRequired)) { return false; }

		if (Side != InSide)
		{
			if (bRequired && !InContext->bQuietMissingAttributeError)
			{
				if (InSide == EIOSide::In)
				{
					PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("\"{0}\" does not exist on input."), FText::FromString(Path)));
				}
				else
				{
					PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("\"{0}\" does not exist on output."), FText::FromString(Path)));
				}
			}

			return false;
		}

		return true;
	}

	bool FProxyDescriptor::CaptureStrict(FPCGExContext* InContext, const FPCGAttributePropertyInputSelector& InSelector, const EIOSide InSide, const bool bRequired)
	{
		if (!Capture(InContext, InSelector, InSide, bRequired)) { return false; }

		if (Side != InSide)
		{
			if (bRequired && !InContext->bQuietMissingAttributeError)
			{
				if (InSide == EIOSide::In)
				{
					PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("\"{0}\" does not exist on input."), FText::FromString(PCGEx::GetSelectorDisplayName(InSelector))));
				}
				else
				{
					PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("\"{0}\" does not exist on output."), FText::FromString(PCGEx::GetSelectorDisplayName(InSelector))));
				}
			}

			return false;
		}

		return true;
	}

#pragma endregion

#define PCGEX_CONVERTING_READ(_TYPE, _NAME, ...) _TYPE IBufferProxy::ReadAs##_NAME(const int32 Index) const PCGEX_NOT_IMPLEMENTED_RET(ReadAs##_NAME, _TYPE{})

	void IBufferProxy::SetSubSelection(const PCGEx::FSubSelection& InSubSelection)
	{
		SubSelection = InSubSelection;
		bWantsSubSelection = SubSelection.bIsValid;
	}

	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_CONVERTING_READ)
#undef PCGEX_CONVERTING_READ

	template <typename T_WORKING>
	TBufferProxy<T_WORKING>::TBufferProxy() : IBufferProxy() { WorkingType = PCGEx::GetMetadataType<T_WORKING>(); }

#define PCGEX_CONVERTING_READ(_TYPE, _NAME, ...) template <typename T_WORKING> _TYPE TBufferProxy<T_WORKING>::ReadAs##_NAME(const int32 Index) const { \
if constexpr (std::is_same_v<_TYPE, T_WORKING>) { return Get(Index); } \
else { return PCGEx::Convert<T_WORKING, _TYPE>(Get(Index)); }}
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_CONVERTING_READ)

	template <typename T_WORKING>
	PCGExValueHash TBufferProxy<T_WORKING>::ReadValueHash(const int32 Index)
	{
		return PCGExBlend::ValueHash(Get(Index));
	}
#undef PCGEX_CONVERTING_READ

#pragma region externalization TBufferProxy

#define PCGEX_TPL(_TYPE, _NAME, ...) template class PCGEXTENDEDTOOLKIT_API TBufferProxy<_TYPE>;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	template <typename T_REAL, typename T_WORKING>
	TAttributeBufferProxy<T_REAL, T_WORKING>::TAttributeBufferProxy()
		: TBufferProxy<T_WORKING>()
	{
		this->RealType = PCGEx::GetMetadataType<T_REAL>();
	}

	template <typename T_REAL, typename T_WORKING>
	T_WORKING TAttributeBufferProxy<T_REAL, T_WORKING>::Get(const int32 Index) const
	{
		// i.e get Rotation<FQuat>.Forward<FVector> as <double>
		//					^ T_REAL	  ^ Sub		      ^ T_WORKING
		if (!bWantsSubSelection)
		{
			if constexpr (std::is_same_v<T_REAL, T_WORKING>) { return Buffer->Read(Index); }
			else { return PCGEx::Convert<T_REAL, T_WORKING>(Buffer->Read(Index)); }
		}
		return SubSelection.template Get<T_REAL, T_WORKING>(Buffer->Read(Index));
	}

	template <typename T_REAL, typename T_WORKING>
	void TAttributeBufferProxy<T_REAL, T_WORKING>::Set(const int32 Index, const T_WORKING& Value) const
	{
		// i.e set Rotation<FQuat>.Forward<FVector> from <FRotator>
		//					^ T_REAL	  ^ Sub		      ^ T_WORKING
		if (!bWantsSubSelection)
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

	template <typename T_REAL, typename T_WORKING>
	T_WORKING TAttributeBufferProxy<T_REAL, T_WORKING>::GetCurrent(const int32 Index) const
	{
		// i.e get Rotation<FQuat>.Forward<FVector> as <double>
		//					^ T_REAL	  ^ Sub		      ^ T_WORKING
		if (!bWantsSubSelection)
		{
			if constexpr (std::is_same_v<T_REAL, T_WORKING>) { return Buffer->GetValue(Index); }
			else { return PCGEx::Convert<T_REAL, T_WORKING>(Buffer->GetValue(Index)); }
		}
		return SubSelection.template Get<T_REAL, T_WORKING>(Buffer->GetValue(Index));
	}

	template <typename T_REAL, typename T_WORKING>
	TSharedPtr<IBuffer> TAttributeBufferProxy<T_REAL, T_WORKING>::GetBuffer() const { return Buffer; }

	template <typename T_REAL, typename T_WORKING>
	bool TAttributeBufferProxy<T_REAL, T_WORKING>::EnsureReadable() const { return Buffer->EnsureReadable(); }

#pragma region externalization TAttributeBufferProxy

#define PCGEX_TPL(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) template class PCGEXTENDEDTOOLKIT_API TAttributeBufferProxy<_TYPE_A, _TYPE_B>;
	PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	template <typename T_REAL, typename T_WORKING, EPCGPointProperties PROPERTY>
	TPointPropertyProxy<T_REAL, T_WORKING, PROPERTY>::TPointPropertyProxy()
		: TBufferProxy<T_WORKING>()
	{
		this->RealType = PCGEx::GetMetadataType<T_REAL>();
	}

	template <typename T_REAL, typename T_WORKING, EPCGPointProperties PROPERTY>
	T_WORKING TPointPropertyProxy<T_REAL, T_WORKING, PROPERTY>::Get(const int32 Index) const
	{
#define PCGEX_GET_SUBPROPERTY(_ACCESSOR, _TYPE) \
if (!bWantsSubSelection){ \
if constexpr (std::is_same_v<T_REAL, T_WORKING>) { return Data->_ACCESSOR; }\
else{ return PCGEx::Convert<T_REAL, T_WORKING>(Data->_ACCESSOR); }\
}else{ return SubSelection.template Get<T_REAL, T_WORKING>(Data->_ACCESSOR); }

		PCGEX_CONSTEXPR_IFELSE_GETPOINTPROPERTY(PROPERTY, PCGEX_GET_SUBPROPERTY)
#undef PCGEX_GET_SUBPROPERTY
		else { return T_WORKING{}; }
	}

	template <typename T_REAL, typename T_WORKING, EPCGPointProperties PROPERTY>
	void TPointPropertyProxy<T_REAL, T_WORKING, PROPERTY>::Set(const int32 Index, const T_WORKING& Value) const
	{
		if (!bWantsSubSelection)
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

#pragma region externalization TPointPropertyProxy

#define PCGEX_TPL(_TYPE, _NAME, _REALTYPE, _PROPERTY) template class PCGEXTENDEDTOOLKIT_API TPointPropertyProxy<_REALTYPE, _TYPE, _PROPERTY>;
#define PCGEX_TPL_LOOP(_PROPERTY, _NAME, _TYPE, _NATIVETYPE)	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL, _TYPE, _PROPERTY)

	PCGEX_FOREACH_POINTPROPERTY(PCGEX_TPL_LOOP)

#undef PCGEX_TPL_LOOP
#undef PCGEX_TPL

#pragma endregion

	template <typename T_REAL, typename T_WORKING, EPCGExtraProperties PROPERTY>
	TPointExtraPropertyProxy<T_REAL, T_WORKING, PROPERTY>::TPointExtraPropertyProxy()
		: TBufferProxy<T_WORKING>()
	{
		this->RealType = PCGEx::GetMetadataType<T_REAL>();
	}

	template <typename T_REAL, typename T_WORKING, EPCGExtraProperties PROPERTY>
	T_WORKING TPointExtraPropertyProxy<T_REAL, T_WORKING, PROPERTY>::Get(const int32 Index) const
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

#pragma region externalization TPointExtraPropertyProxy

#define PCGEX_TPL(_TYPE, _NAME, _REALTYPE, _PROPERTY) template class PCGEXTENDEDTOOLKIT_API TPointExtraPropertyProxy<_REALTYPE, _TYPE, _PROPERTY>;
#define PCGEX_TPL_LOOP(_PROPERTY, _NAME, _TYPE)	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL, _TYPE, _PROPERTY)

	PCGEX_FOREACH_EXTRAPROPERTY(PCGEX_TPL_LOOP)

#undef PCGEX_TPL_LOOP
#undef PCGEX_TPL

#pragma endregion

	template <typename T_WORKING>
	TConstantProxy<T_WORKING>::TConstantProxy()
		: TBufferProxy<T_WORKING>()
	{
		this->RealType = PCGEx::GetMetadataType<T_WORKING>();
	}

	template <typename T_WORKING>
	template <typename T>
	void TConstantProxy<T_WORKING>::SetConstant(const T& InValue) { Constant = PCGEx::Convert<T, T_WORKING>(InValue); }

#pragma region externalization TConstantProxy

#define PCGEX_TPL(_TYPE, _NAME, ...) template class PCGEXTENDEDTOOLKIT_API TConstantProxy<_TYPE>;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#define PCGEX_TPL(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) template PCGEXTENDEDTOOLKIT_API void TConstantProxy<_TYPE_A>::SetConstant<_TYPE_B>(const _TYPE_B&);
	PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	template <typename T_REAL, typename T_WORKING>
	TDirectAttributeProxy<T_REAL, T_WORKING>::TDirectAttributeProxy()
		: TBufferProxy<T_WORKING>()
	{
		this->RealType = PCGEx::GetMetadataType<T_REAL>();
	}

	template <typename T_REAL, typename T_WORKING>
	T_WORKING TDirectAttributeProxy<T_REAL, T_WORKING>::Get(const int32 Index) const
	{
		const int64 MetadataEntry = Data->GetMetadataEntry(Index);

		// i.e get Rotation<FQuat>.Forward<FVector> as <double>
		//					^ T_REAL	  ^ Sub		      ^ T_WORKING
		if (!bWantsSubSelection)
		{
			if constexpr (std::is_same_v<T_REAL, T_WORKING>) { return InAttribute->GetValueFromItemKey(MetadataEntry); }
			else { return PCGEx::Convert<T_REAL, T_WORKING>(InAttribute->GetValueFromItemKey(MetadataEntry)); }
		}
		return SubSelection.template Get<T_REAL, T_WORKING>(InAttribute->GetValueFromItemKey(MetadataEntry));
	}

	template <typename T_REAL, typename T_WORKING>
	T_WORKING TDirectAttributeProxy<T_REAL, T_WORKING>::GetCurrent(const int32 Index) const
	{
		const int64 MetadataEntry = Data->GetMetadataEntry(Index);

		// i.e get Rotation<FQuat>.Forward<FVector> as <double>
		//					^ T_REAL	  ^ Sub		      ^ T_WORKING
		if (!bWantsSubSelection)
		{
			if constexpr (std::is_same_v<T_REAL, T_WORKING>) { return OutAttribute->GetValueFromItemKey(MetadataEntry); }
			else { return PCGEx::Convert<T_REAL, T_WORKING>(OutAttribute->GetValueFromItemKey(MetadataEntry)); }
		}
		return SubSelection.template Get<T_REAL, T_WORKING>(OutAttribute->GetValueFromItemKey(MetadataEntry));
	}

	template <typename T_REAL, typename T_WORKING>
	void TDirectAttributeProxy<T_REAL, T_WORKING>::Set(const int32 Index, const T_WORKING& Value) const
	{
		const int64 MetadataEntry = Data->GetMetadataEntry(Index);

		// i.e set Rotation<FQuat>.Forward<FVector> from <FRotator>
		//					^ T_REAL	  ^ Sub		      ^ T_WORKING
		if (!bWantsSubSelection)
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

#pragma region externalization TDirectAttributeProxy

#define PCGEX_TPL(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) template class PCGEXTENDEDTOOLKIT_API TDirectAttributeProxy<_TYPE_A, _TYPE_B>;
	PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	template <typename T_REAL, typename T_WORKING>
	TDirectDataAttributeProxy<T_REAL, T_WORKING>::TDirectDataAttributeProxy()
		: TBufferProxy<T_WORKING>()
	{
		this->RealType = PCGEx::GetMetadataType<T_REAL>();
	}

	template <typename T_REAL, typename T_WORKING>
	T_WORKING TDirectDataAttributeProxy<T_REAL, T_WORKING>::Get(const int32 Index) const
	{
		// i.e get Rotation<FQuat>.Forward<FVector> as <double>
		//					^ T_REAL	  ^ Sub		      ^ T_WORKING
		if (!bWantsSubSelection)
		{
			if constexpr (std::is_same_v<T_REAL, T_WORKING>) { return InAttribute->GetValueFromItemKey(PCGDefaultValueKey); }
			else { return PCGEx::Convert<T_REAL, T_WORKING>(InAttribute->GetValueFromItemKey(PCGDefaultValueKey)); }
		}
		return SubSelection.template Get<T_REAL, T_WORKING>(InAttribute->GetValueFromItemKey(PCGDefaultValueKey));
	}

	template <typename T_REAL, typename T_WORKING>
	T_WORKING TDirectDataAttributeProxy<T_REAL, T_WORKING>::GetCurrent(const int32 Index) const
	{
		// i.e get Rotation<FQuat>.Forward<FVector> as <double>
		//					^ T_REAL	  ^ Sub		      ^ T_WORKING
		if (!bWantsSubSelection)
		{
			if constexpr (std::is_same_v<T_REAL, T_WORKING>) { return OutAttribute->GetValueFromItemKey(PCGDefaultValueKey); }
			else { return PCGEx::Convert<T_REAL, T_WORKING>(OutAttribute->GetValueFromItemKey(PCGDefaultValueKey)); }
		}
		return SubSelection.template Get<T_REAL, T_WORKING>(OutAttribute->GetValueFromItemKey(PCGDefaultValueKey));
	}

	template <typename T_REAL, typename T_WORKING>
	void TDirectDataAttributeProxy<T_REAL, T_WORKING>::Set(const int32 Index, const T_WORKING& Value) const
	{
		// i.e set Rotation<FQuat>.Forward<FVector> from <FRotator>
		//					^ T_REAL	  ^ Sub		      ^ T_WORKING
		if (!bWantsSubSelection)
		{
			if constexpr (std::is_same_v<T_REAL, T_WORKING>) { PCGExDataHelpers::SetDataValue(OutAttribute, Value); }
			else { PCGExDataHelpers::SetDataValue(OutAttribute, PCGEx::Convert<T_WORKING, T_REAL>(Value)); }
		}
		else
		{
			T_REAL V = OutAttribute->GetValueFromItemKey(PCGDefaultValueKey);
			SubSelection.template Set<T_REAL, T_WORKING>(V, Value);
			PCGExDataHelpers::SetDataValue(OutAttribute, V);
		}
	}

#pragma region externalization TDirectDataAttributeProxy

#define PCGEX_TPL(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) template class PCGEXTENDEDTOOLKIT_API TDirectDataAttributeProxy<_TYPE_A, _TYPE_B>;
	PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion
}

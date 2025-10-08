// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExProxyData.h"

#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExPointIO.h"

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

	bool FProxyDescriptor::Capture(FPCGExContext* InContext, const FString& Path, const EIOSide InSide, const bool bThrowError)
	{
		const TSharedPtr<FFacade> InFacade = DataFacade.Pin();
		check(InFacade);

		bool bValid = true;

		Selector = FPCGAttributePropertyInputSelector();
		Selector.Update(Path);

		Side = InSide;

		if (!PCGEx::TryGetTypeAndSource(Selector, InFacade, RealType, Side))
		{
			if (bThrowError) { PCGEX_LOG_INVALID_SELECTOR_C(InContext, , Selector) }
			bValid = false;
		}

		Selector = Selector.CopyAndFixLast(InFacade->Source->GetData(Side));

		UpdateSubSelection();
		WorkingType = SubSelection.GetSubType(RealType);

		return bValid;
	}

	bool FProxyDescriptor::Capture(FPCGExContext* InContext, const FPCGAttributePropertyInputSelector& InSelector, const EIOSide InSide, const bool bThrowError)
	{
		const TSharedPtr<FFacade> InFacade = DataFacade.Pin();
		check(InFacade);

		bool bValid = true;
		Side = bIsConstant ? EIOSide::In : InSide;

		if (!PCGEx::TryGetTypeAndSource(InSelector, InFacade, RealType, Side))
		{
			if (bThrowError) { PCGEX_LOG_INVALID_SELECTOR_C(InContext, , InSelector) }
			bValid = false;
		}

		PointData = InFacade->Source->GetData(Side);
		Selector = InSelector.CopyAndFixLast(InFacade->Source->GetData(Side));

		UpdateSubSelection();
		WorkingType = SubSelection.GetSubType(RealType);

		return bValid;
	}

	bool FProxyDescriptor::CaptureStrict(FPCGExContext* InContext, const FString& Path, const EIOSide InSide, const bool bThrowError)
	{
		if (!Capture(InContext, Path, InSide, bThrowError)) { return false; }

		if (Side != InSide)
		{
			if (bThrowError)
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

	bool FProxyDescriptor::CaptureStrict(FPCGExContext* InContext, const FPCGAttributePropertyInputSelector& InSelector, const EIOSide InSide, const bool bThrowError)
	{
		if (!Capture(InContext, InSelector, InSide, bThrowError)) { return false; }

		if (Side != InSide)
		{
			if (bThrowError)
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

	template <typename T_WORKING>
	TBufferProxy<T_WORKING>::TBufferProxy() : IBufferProxy() { WorkingType = PCGEx::GetMetadataType<T_WORKING>(); }

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	TAttributeBufferProxy<T_REAL, T_WORKING, bSubSelection>::TAttributeBufferProxy()
		: TBufferProxy<T_WORKING>()
	{
		this->RealType = PCGEx::GetMetadataType<T_REAL>();
	}

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	T_WORKING TAttributeBufferProxy<T_REAL, T_WORKING, bSubSelection>::Get(const int32 Index) const
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

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	void TAttributeBufferProxy<T_REAL, T_WORKING, bSubSelection>::Set(const int32 Index, const T_WORKING& Value) const
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

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	T_WORKING TAttributeBufferProxy<T_REAL, T_WORKING, bSubSelection>::GetCurrent(const int32 Index) const
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

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	TSharedPtr<IBuffer> TAttributeBufferProxy<T_REAL, T_WORKING, bSubSelection>::GetBuffer() const { return Buffer; }

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	bool TAttributeBufferProxy<T_REAL, T_WORKING, bSubSelection>::EnsureReadable() const { return Buffer->EnsureReadable(); }

	template <typename T_REAL, typename T_WORKING, bool bSubSelection, EPCGPointProperties PROPERTY>
	TPointPropertyProxy<T_REAL, T_WORKING, bSubSelection, PROPERTY>::TPointPropertyProxy()
		: TBufferProxy<T_WORKING>()
	{
		this->RealType = PCGEx::GetMetadataType<T_REAL>();
	}

	template <typename T_REAL, typename T_WORKING, bool bSubSelection, EPCGPointProperties PROPERTY>
	T_WORKING TPointPropertyProxy<T_REAL, T_WORKING, bSubSelection, PROPERTY>::Get(const int32 Index) const
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

	template <typename T_REAL, typename T_WORKING, bool bSubSelection, EPCGPointProperties PROPERTY>
	void TPointPropertyProxy<T_REAL, T_WORKING, bSubSelection, PROPERTY>::Set(const int32 Index, const T_WORKING& Value) const
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

	template <typename T_REAL, typename T_WORKING, bool bSubSelection, EPCGExtraProperties PROPERTY>
	TPointExtraPropertyProxy<T_REAL, T_WORKING, bSubSelection, PROPERTY>::TPointExtraPropertyProxy()
		: TBufferProxy<T_WORKING>()
	{
		this->RealType = PCGEx::GetMetadataType<T_REAL>();
	}

	template <typename T_REAL, typename T_WORKING, bool bSubSelection, EPCGExtraProperties PROPERTY>
	T_WORKING TPointExtraPropertyProxy<T_REAL, T_WORKING, bSubSelection, PROPERTY>::Get(const int32 Index) const
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

	template <typename T_WORKING>
	TConstantProxy<T_WORKING>::TConstantProxy()
		: TBufferProxy<T_WORKING>()
	{
		this->RealType = PCGEx::GetMetadataType<T_WORKING>();
	}

	template <typename T_WORKING>
	template <typename T>
	void TConstantProxy<T_WORKING>::SetConstant(const T& InValue) { Constant = PCGEx::Convert<T, T_WORKING>(InValue); }

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	TDirectAttributeProxy<T_REAL, T_WORKING, bSubSelection>::TDirectAttributeProxy()
		: TBufferProxy<T_WORKING>()
	{
		this->RealType = PCGEx::GetMetadataType<T_REAL>();
	}

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	T_WORKING TDirectAttributeProxy<T_REAL, T_WORKING, bSubSelection>::Get(const int32 Index) const
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

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	T_WORKING TDirectAttributeProxy<T_REAL, T_WORKING, bSubSelection>::GetCurrent(const int32 Index) const
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

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	void TDirectAttributeProxy<T_REAL, T_WORKING, bSubSelection>::Set(const int32 Index, const T_WORKING& Value) const
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

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	TDirectDataAttributeProxy<T_REAL, T_WORKING, bSubSelection>::TDirectDataAttributeProxy()
		: TBufferProxy<T_WORKING>()
	{
		this->RealType = PCGEx::GetMetadataType<T_REAL>();
	}

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	T_WORKING TDirectDataAttributeProxy<T_REAL, T_WORKING, bSubSelection>::Get(const int32 Index) const
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

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	T_WORKING TDirectDataAttributeProxy<T_REAL, T_WORKING, bSubSelection>::GetCurrent(const int32 Index) const
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

	template <typename T_REAL, typename T_WORKING, bool bSubSelection>
	void TDirectDataAttributeProxy<T_REAL, T_WORKING, bSubSelection>::Set(const int32 Index, const T_WORKING& Value) const
	{
		// i.e set Rotation<FQuat>.Forward<FVector> from <FRotator>
		//					^ T_REAL	  ^ Sub		      ^ T_WORKING
		if constexpr (!bSubSelection)
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

#pragma region externalization TPointPropertyProxy

#define PCGEX_TPL(_TYPE, _NAME, _REALTYPE, _PROPERTY)\
template class PCGEXTENDEDTOOLKIT_API TPointPropertyProxy<_REALTYPE, _TYPE, true, _PROPERTY>;\
template class PCGEXTENDEDTOOLKIT_API TPointPropertyProxy<_REALTYPE, _TYPE, false, _PROPERTY>;

#define PCGEX_TPL_LOOP(_PROPERTY, _NAME, _TYPE, _NATIVETYPE)	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL, _TYPE, _PROPERTY)

	PCGEX_FOREACH_POINTPROPERTY(PCGEX_TPL_LOOP)

#undef PCGEX_TPL_LOOP
#undef PCGEX_TPL

#pragma endregion

#pragma region externalization TPointExtraPropertyProxy

#define PCGEX_TPL(_TYPE, _NAME, _REALTYPE, _PROPERTY)\
template class PCGEXTENDEDTOOLKIT_API TPointExtraPropertyProxy<_REALTYPE, _TYPE, true, _PROPERTY>;\
template class PCGEXTENDEDTOOLKIT_API TPointExtraPropertyProxy<_REALTYPE, _TYPE, false, _PROPERTY>;

#define PCGEX_TPL_LOOP(_PROPERTY, _NAME, _TYPE)	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL, _TYPE, _PROPERTY)

	PCGEX_FOREACH_EXTRAPROPERTY(PCGEX_TPL_LOOP)

#undef PCGEX_TPL_LOOP
#undef PCGEX_TPL

#pragma endregion

#pragma region externalization

#define PCGEX_TPL(_TYPE, _NAME, ...) \
template class PCGEXTENDEDTOOLKIT_API TBufferProxy<_TYPE>;\
PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#define PCGEX_TPL(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) \
template class PCGEXTENDEDTOOLKIT_API TAttributeBufferProxy<_TYPE_A, _TYPE_B, true>;\
template class PCGEXTENDEDTOOLKIT_API TAttributeBufferProxy<_TYPE_A, _TYPE_B, false>;\
template class PCGEXTENDEDTOOLKIT_API TDirectAttributeProxy<_TYPE_A, _TYPE_B, true>;\
template class PCGEXTENDEDTOOLKIT_API TDirectAttributeProxy<_TYPE_A, _TYPE_B, false>;\
template class PCGEXTENDEDTOOLKIT_API TDirectDataAttributeProxy<_TYPE_A, _TYPE_B, true>;\
template class PCGEXTENDEDTOOLKIT_API TDirectDataAttributeProxy<_TYPE_A, _TYPE_B, false>;
	PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	TSharedPtr<IBufferProxy> GetProxyBuffer(
		FPCGExContext* InContext,
		const FProxyDescriptor& InDescriptor)
	{
		TSharedPtr<IBufferProxy> OutProxy = nullptr;
		const bool bSubSelection = InDescriptor.SubSelection.bIsValid;

		const TSharedPtr<FFacade> InDataFacade = InDescriptor.DataFacade.Pin();
		UPCGBasePointData* PointData = nullptr;

		if (!InDataFacade)
		{
			PointData = const_cast<UPCGBasePointData*>(InDescriptor.PointData);

			if (PointData && InDescriptor.Selector.GetSelection() == EPCGAttributePropertySelection::Property)
			{
				// We don't have a facade in the descriptor, but we only need the raw data.
				// This is to support cases where we're only interested in point properties that are not associated to a FFacade
			}
			else
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Proxy descriptor has no valid source."));
				return nullptr;
			}
		}
		else
		{
			if (InDescriptor.bIsConstant || InDescriptor.Side == EIOSide::In) { PointData = const_cast<UPCGBasePointData*>(InDataFacade->GetIn()); }
			else { PointData = InDataFacade->GetOut(); }

			if (!PointData)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Proxy descriptor attempted to work with a null PointData."));
				return nullptr;
			}
		}

		PCGEx::ExecuteWithRightType(
			InDescriptor.WorkingType, [&](auto W)
			{
				using T_WORKING = decltype(W);
				PCGEx::ExecuteWithRightType(
					InDescriptor.RealType, [&](auto R)
					{
						using T_REAL = decltype(R);

						if (InDescriptor.bIsConstant)
						{
							// TODO : Support subselector

							TSharedPtr<TConstantProxy<T_WORKING>> TypedProxy = MakeShared<TConstantProxy<T_WORKING>>();
							OutProxy = TypedProxy;

							if (InDescriptor.Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
							{
								const FPCGMetadataAttribute<T_REAL>* Attribute = PCGEx::TryGetConstAttribute<T_REAL>(InDataFacade->GetIn(), PCGEx::GetAttributeIdentifier(InDescriptor.Selector, InDataFacade->GetIn()));
								if (!Attribute)
								{
									TypedProxy->SetConstant(0);
									return;
								}

								const PCGMetadataEntryKey Key = InDataFacade->GetIn()->IsEmpty() ?
									                                PCGInvalidEntryKey :
									                                InDataFacade->GetIn()->GetMetadataEntry(0);

								TypedProxy->SetConstant(Attribute->GetValueFromItemKey(Key));
							}
							else if (InDescriptor.Selector.GetSelection() == EPCGAttributePropertySelection::Property)
							{
								TypedProxy->SetConstant(0);

								if (!PointData->IsEmpty())
								{
									constexpr int32 Index = 0;
									const EPCGPointProperties SelectedProperty = InDescriptor.Selector.GetPointProperty();

#define PCGEX_SET_CONST(_ACCESSOR, _TYPE) \
									if (bSubSelection) { TypedProxy->SetConstant(PointData->_ACCESSOR); } \
									else { TypedProxy->SetConstant(PointData->_ACCESSOR); }

									PCGEX_IFELSE_GETPOINTPROPERTY(SelectedProperty, PCGEX_SET_CONST)
									else { TypedProxy->SetConstant(0); }
#undef PCGEX_SET_CONST
								}
							}
							else
							{
								TypedProxy->SetConstant(0);
							}

							return;
						}

						if (InDescriptor.Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
						{
							if (InDescriptor.bWantsDirect)
							{
								const FPCGMetadataAttribute<T_REAL>* InAttribute = nullptr;
								FPCGMetadataAttribute<T_REAL>* OutAttribute = nullptr;

								if (InDescriptor.Role == EProxyRole::Read)
								{
									if (InDescriptor.Side == EIOSide::In)
									{
										InAttribute = PCGEx::TryGetConstAttribute<T_REAL>(InDataFacade->GetIn(), PCGEx::GetAttributeIdentifier(InDescriptor.Selector, InDataFacade->GetIn()));
									}
									else
									{
										InAttribute = PCGEx::TryGetConstAttribute<T_REAL>(InDataFacade->GetOut(), PCGEx::GetAttributeIdentifier(InDescriptor.Selector, InDataFacade->GetIn()));
									}

									if (InAttribute) { OutAttribute = const_cast<FPCGMetadataAttribute<T_REAL>*>(InAttribute); }

									check(InAttribute);
								}
								else if (InDescriptor.Role == EProxyRole::Write)
								{
									OutAttribute = InDataFacade->Source->FindOrCreateAttribute(PCGEx::GetAttributeIdentifier(InDescriptor.Selector, InDataFacade->GetOut()), T_REAL{});
									if (OutAttribute) { InAttribute = OutAttribute; }

									check(OutAttribute);
								}

#define PCGEX_DIRECT_PROXY(_SUBSELECTION, _DATA)\
TSharedPtr<TDirect##_DATA##AttributeProxy<T_REAL, T_WORKING, _SUBSELECTION>> TypedProxy = MakeShared<TDirect##_DATA##AttributeProxy<T_REAL, T_WORKING, _SUBSELECTION>>();\
TypedProxy->InAttribute = const_cast<FPCGMetadataAttribute<T_REAL>*>(InAttribute);\
TypedProxy->OutAttribute = OutAttribute;\
OutProxy = TypedProxy;

// @SPLASH_DAMAGE_CHANGE [IMPROVEMENT] #SDTechArt - BEGIN: Fixing Static Analysis warning with dereferenced ptr to InAttribute "warning C6011: Dereferencing NULL pointer 'InAttribute'".
								if (InAttribute && InAttribute->GetMetadataDomain()->GetDomainID().Flag == EPCGMetadataDomainFlag::Data)
// @SPLASH_DAMAGE_CHANGE [IMPROVEMENT] #SDTechArt - END: Fixing Static Analysis warning with dereferenced ptr to InAttribute "warning C6011: Dereferencing NULL pointer 'InAttribute'".
								{
									if (bSubSelection) { PCGEX_DIRECT_PROXY(true, Data) }
									else { PCGEX_DIRECT_PROXY(false, Data) }
								}
								else
								{
									if (bSubSelection) { PCGEX_DIRECT_PROXY(true,) }
									else { PCGEX_DIRECT_PROXY(false,) }
								}

								return;
							}

							const FPCGAttributeIdentifier Identifier = PCGEx::GetAttributeIdentifier(InDescriptor.Selector, InDescriptor.Side == EIOSide::In ? InDataFacade->GetIn() : InDataFacade->GetOut());

							// Check if there is an existing buffer with for our attribute
							TSharedPtr<TBuffer<T_REAL>> ExistingBuffer = InDataFacade->FindBuffer<T_REAL>(Identifier);
							TSharedPtr<TBuffer<T_REAL>> Buffer;

							// Proceed based on side & role
							// We want to read, but where from?
							if (InDescriptor.Role == EProxyRole::Read)
							{
								if (InDescriptor.Side == EIOSide::In)
								{
									// Use existing buffer if possible
									if (ExistingBuffer && ExistingBuffer->IsReadable()) { Buffer = ExistingBuffer; }

									// Otherwise create read-only buffer
									if (!Buffer) { Buffer = InDataFacade->GetReadable<T_REAL>(Identifier, EIOSide::In, true); }
								}
								else if (InDescriptor.Side == EIOSide::Out)
								{
									// This is the tricky bit.
									// We want to read from output directly, and we can only do so by converting an existing writable buffer to a readable one.
									// This is a risky operation because internally it will replace the read value buffer with the write values one.
									// Value-wise it's not an issue as the write buffer will generally be pre-filled with input values.
									// However this is a problem if the number of item differs between input & output
									if (ExistingBuffer)
									{
										// This buffer is already set-up to be read from its output data
										if (ExistingBuffer->ReadsFromOutput()) { Buffer = ExistingBuffer; }

										if (!Buffer)
										{
											// Change buffer state to read from output
											if (ExistingBuffer->IsWritable())
											{
												Buffer = InDataFacade->GetReadable<T_REAL>(Identifier, EIOSide::Out, true);
											}

											if (!Buffer)
											{
												PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Trying to read from an output buffer that doesn't exist yet."));
												return;
											}
										}
									}
									else
									{
										// Create a writable... Not ideal, will likely create a whole bunch of problems
										Buffer = InDataFacade->GetWritable<T_REAL>(Identifier, T_REAL{}, true, EBufferInit::Inherit);
										if (Buffer) { Buffer->EnsureReadable(); }
										else
										{
											// No existing buffer yet
											PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Could not create read/write buffer."));
											return;
										}
									}
								}
							}
							// We want to write, so we can only write to out!
							else if (InDescriptor.Role == EProxyRole::Write)
							{
								Buffer = InDataFacade->GetWritable<T_REAL>(Identifier, T_REAL{}, true, EBufferInit::Inherit);
							}

							if (!Buffer)
							{
								PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Failed to initialize proxy buffer."));
								return;
							}

							if (bSubSelection)
							{
								TSharedPtr<TAttributeBufferProxy<T_REAL, T_WORKING, true>> TypedProxy =
									MakeShared<TAttributeBufferProxy<T_REAL, T_WORKING, true>>();

								TypedProxy->Buffer = Buffer;
								OutProxy = TypedProxy;
							}
							else
							{
								TSharedPtr<TAttributeBufferProxy<T_REAL, T_WORKING, false>> TypedProxy =
									MakeShared<TAttributeBufferProxy<T_REAL, T_WORKING, false>>();

								TypedProxy->Buffer = Buffer;
								OutProxy = TypedProxy;
							}
						}

						else if (InDescriptor.Selector.GetSelection() == EPCGAttributePropertySelection::Property)
						{
							if (InDescriptor.Role == EProxyRole::Write)
							{
								// Ensure we allocate native properties we'll be writing to
								EPCGPointNativeProperties NativeType = PCGEx::GetPropertyNativeType(InDescriptor.Selector.GetPointProperty());
								if (NativeType == EPCGPointNativeProperties::None)
								{
									PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Attempting to write to an unsupported property type."));
									return;
								}

								PointData->AllocateProperties(NativeType);
							}

#define PCGEX_DECL_PROXY(_PROPERTY, _ACCESSOR, _TYPE, _RANGE_TYPE) \
						case _PROPERTY : \
						if (bSubSelection) { OutProxy = MakeShared<TPointPropertyProxy<_TYPE, T_WORKING, true, _PROPERTY>>(); } \
						else { OutProxy = MakeShared<TPointPropertyProxy<_TYPE, T_WORKING, false, _PROPERTY>>(); } \
						break;
							switch (InDescriptor.Selector.GetPointProperty())
							{
							PCGEX_FOREACH_POINTPROPERTY(PCGEX_DECL_PROXY)
							default: break;
							}
#undef PCGEX_DECL_PROXY
						}
						else
						{
							// TODO : Support new extra properties here!

							if (bSubSelection)
							{
								TSharedPtr<TPointExtraPropertyProxy<int32, T_WORKING, true, EPCGExtraProperties::Index>> TypedProxy =
									MakeShared<TPointExtraPropertyProxy<int32, T_WORKING, true, EPCGExtraProperties::Index>>();

								//TypedProxy->Buffer = InDataFacade->GetBroadcaster<int32>(InDescriptor.Selector, true);
								OutProxy = TypedProxy;
							}
							else
							{
								TSharedPtr<TPointExtraPropertyProxy<int32, T_WORKING, false, EPCGExtraProperties::Index>> TypedProxy =
									MakeShared<TPointExtraPropertyProxy<int32, T_WORKING, false, EPCGExtraProperties::Index>>();

								//TypedProxy->Buffer = InDataFacade->GetBroadcaster<int32>(InDescriptor.Selector, true);
								OutProxy = TypedProxy;
							}
						}
					});
			});

		if (OutProxy)
		{
#if WITH_EDITOR
			OutProxy->Descriptor = InDescriptor;
#endif

			OutProxy->Data = PointData;

			if (!OutProxy->Validate(InDescriptor))
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Proxy buffer doesn't match desired T_REAL and T_WORKING : \"{0}\""), FText::FromString(PCGEx::GetSelectorDisplayName(InDescriptor.Selector))));
				return nullptr;
			}

			OutProxy->SubSelection = InDescriptor.SubSelection;
		}


		return OutProxy;
	}

	template <typename T>
	TSharedPtr<IBufferProxy> GetConstantProxyBuffer(const T& Constant)
	{
		TSharedPtr<TConstantProxy<T>> TypedProxy = MakeShared<TConstantProxy<T>>();
		TypedProxy->SetConstant(Constant);
		return TypedProxy;
	}

#pragma region externalization

#define PCGEX_TPL(_TYPE, _NAME, ...) \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<IBufferProxy> GetConstantProxyBuffer<_TYPE>(const _TYPE& Constant);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	bool GetPerFieldProxyBuffers(
		FPCGExContext* InContext,
		const FProxyDescriptor& InBaseDescriptor,
		const int32 NumDesiredFields,
		TArray<TSharedPtr<IBufferProxy>>& OutProxies)
	{
		OutProxies.Reset(NumDesiredFields);

		const int32 Dimensions = PCGEx::GetMetadataSize(InBaseDescriptor.RealType);

		if (Dimensions == -1 &&
			(!InBaseDescriptor.SubSelection.bIsValid || !InBaseDescriptor.SubSelection.bIsComponentSet))
		{
			// There's no sub-selection yet we have a complex type
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Can't automatically break complex type into sub-components. Use a narrower selector or a supported type."));
			return false;
		}

		const int32 MaxIndex = Dimensions == -1 ? 2 : Dimensions - 1;

		// We have a subselection
		if (InBaseDescriptor.SubSelection.bIsValid)
		{
			// We have a single specific field set within that selection
			if (InBaseDescriptor.SubSelection.bIsFieldSet)
			{
				const TSharedPtr<IBufferProxy> Proxy = GetProxyBuffer(InContext, InBaseDescriptor);
				if (!Proxy) { return false; }

				// Just use the same pointer on each desired field
				for (int i = 0; i < NumDesiredFields; i++) { OutProxies.Add(Proxy); }

				return true;
			}
			// We don't have a single specific field so we need to "fake" individual ones
			for (int i = 0; i < NumDesiredFields; i++)
			{
				FProxyDescriptor SingleFieldCopy = InBaseDescriptor;
				SingleFieldCopy.SetFieldIndex(FMath::Clamp(i, 0, MaxIndex)); // Clamp field index to a safe max

				const TSharedPtr<IBufferProxy> Proxy = GetProxyBuffer(InContext, SingleFieldCopy);
				if (!Proxy) { return false; }
				OutProxies.Add(Proxy);
			}
		}
		// Source has no SubSelection
		else
		{
			for (int i = 0; i < NumDesiredFields; i++)
			{
				FProxyDescriptor SingleFieldCopy = InBaseDescriptor;
				SingleFieldCopy.SetFieldIndex(FMath::Clamp(i, 0, MaxIndex)); // Clamp field index to a safe max

				const TSharedPtr<IBufferProxy> Proxy = GetProxyBuffer(InContext, SingleFieldCopy);
				if (!Proxy) { return false; }
				OutProxies.Add(Proxy);
			}
		}

		return true;
	}
}

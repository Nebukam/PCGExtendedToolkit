// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExProxyDataHelpers.h"

#include "Data/PCGExData.h"
#include "Data/PCGExProxyData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExPointIO.h"

namespace PCGExData
{
	template <typename T_REAL>
	void TryGetInOutAttr(const FProxyDescriptor& InDescriptor, const TSharedPtr<FFacade>& InDataFacade, const FPCGMetadataAttribute<T_REAL>*& OutInAttribute, FPCGMetadataAttribute<T_REAL>*& OutOutAttribute)
	{
		OutInAttribute = nullptr;
		OutOutAttribute = nullptr;

		if (InDescriptor.Role == EProxyRole::Read)
		{
			if (InDescriptor.Side == EIOSide::In)
			{
				OutInAttribute = PCGEx::TryGetConstAttribute<T_REAL>(InDataFacade->GetIn(), PCGEx::GetAttributeIdentifier(InDescriptor.Selector, InDataFacade->GetIn()));
			}
			else
			{
				OutInAttribute = PCGEx::TryGetConstAttribute<T_REAL>(InDataFacade->GetOut(), PCGEx::GetAttributeIdentifier(InDescriptor.Selector, InDataFacade->GetIn()));
			}

			if (OutInAttribute) { OutOutAttribute = const_cast<FPCGMetadataAttribute<T_REAL>*>(OutInAttribute); }

			check(OutInAttribute);
		}
		else if (InDescriptor.Role == EProxyRole::Write)
		{
			OutOutAttribute = InDataFacade->Source->FindOrCreateAttribute(PCGEx::GetAttributeIdentifier(InDescriptor.Selector, InDataFacade->GetOut()), T_REAL{});
			if (OutOutAttribute) { OutInAttribute = OutOutAttribute; }

			check(OutOutAttribute);
		}
	}

	template <typename T_REAL>
	TSharedPtr<TBuffer<T_REAL>> TryGetBuffer(FPCGExContext* InContext, const FProxyDescriptor& InDescriptor, const TSharedPtr<FFacade>& InDataFacade)
	{
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
							return nullptr;
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
						return nullptr;
					}
				}
			}
		}
		// We want to write, so we can only write to out!
		else if (InDescriptor.Role == EProxyRole::Write)
		{
			Buffer = InDataFacade->GetWritable<T_REAL>(Identifier, T_REAL{}, true, EBufferInit::Inherit);
		}

		return Buffer;
	}

#pragma region externalization TryGetInOutAttr / TryGetBuffer

#define PCGEX_TPL(_TYPE, _NAME, ...) \
template PCGEXTENDEDTOOLKIT_API void TryGetInOutAttr<_TYPE>(const FProxyDescriptor& InDescriptor, const TSharedPtr<FFacade>& InDataFacade, const FPCGMetadataAttribute<_TYPE>*& OutInAttribute, FPCGMetadataAttribute<_TYPE>*& OutOutAttribute); \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<TBuffer<_TYPE>> TryGetBuffer<_TYPE>(FPCGExContext* InContext, const FProxyDescriptor& InDescriptor, const TSharedPtr<FFacade>& InDataFacade);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	template <typename T_REAL, typename T_WORKING>
	TSharedPtr<IBufferProxy> GetProxyBuffer(FPCGExContext* InContext, const FProxyDescriptor& InDescriptor, const TSharedPtr<FFacade>& InDataFacade, UPCGBasePointData* PointData)
	{
		TSharedPtr<IBufferProxy> OutProxy = nullptr;
#define PCGEX_RETURN_OUT_PROXY if (OutProxy) { OutProxy->SetSubSelection(InDescriptor.SubSelection); } return OutProxy;

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
					PCGEX_RETURN_OUT_PROXY
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

#define PCGEX_SET_CONST(_ACCESSOR, _TYPE) TypedProxy->SetConstant(PointData->_ACCESSOR);
					PCGEX_IFELSE_GETPOINTPROPERTY(SelectedProperty, PCGEX_SET_CONST)
					else { TypedProxy->SetConstant(0); }
#undef PCGEX_SET_CONST
				}
			}
			else
			{
				TypedProxy->SetConstant(0);
			}

			PCGEX_RETURN_OUT_PROXY
		}

		if (InDescriptor.Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			if (InDescriptor.bWantsDirect)
			{
				const FPCGMetadataAttribute<T_REAL>* InAttribute = nullptr;
				FPCGMetadataAttribute<T_REAL>* OutAttribute = nullptr;

				TryGetInOutAttr(InDescriptor, InDataFacade, InAttribute, OutAttribute);

#define PCGEX_DIRECT_PROXY(_DATA)\
TSharedPtr<TDirect##_DATA##AttributeProxy<T_REAL, T_WORKING>> TypedProxy = MakeShared<TDirect##_DATA##AttributeProxy<T_REAL, T_WORKING>>();\
TypedProxy->InAttribute = const_cast<FPCGMetadataAttribute<T_REAL>*>(InAttribute);\
TypedProxy->OutAttribute = OutAttribute;\
OutProxy = TypedProxy;

				if (InAttribute->GetMetadataDomain()->GetDomainID().Flag == EPCGMetadataDomainFlag::Data) { PCGEX_DIRECT_PROXY(Data) }
				else { PCGEX_DIRECT_PROXY() }

				PCGEX_RETURN_OUT_PROXY
			}

			// Check if there is an existing buffer with for our attribute
			TSharedPtr<TBuffer<T_REAL>> Buffer = TryGetBuffer<T_REAL>(InContext, InDescriptor, InDataFacade);

			if (!Buffer)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Failed to initialize proxy buffer."));
				return nullptr;
			}

			TSharedPtr<TAttributeBufferProxy<T_REAL, T_WORKING>> TypedProxy = MakeShared<TAttributeBufferProxy<T_REAL, T_WORKING>>();
			TypedProxy->Buffer = Buffer;
			OutProxy = TypedProxy;
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
					return nullptr;
				}

				PointData->AllocateProperties(NativeType);
			}

#define PCGEX_DECL_PROXY(_PROPERTY, _ACCESSOR, _TYPE, _RANGE_TYPE) \
						case _PROPERTY : \
						OutProxy = MakeShared<TPointPropertyProxy<_TYPE, T_WORKING, _PROPERTY>>(); \
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

			TSharedPtr<TPointExtraPropertyProxy<int32, T_WORKING, EPCGExtraProperties::Index>> TypedProxy =
				MakeShared<TPointExtraPropertyProxy<int32, T_WORKING, EPCGExtraProperties::Index>>();

			//TypedProxy->Buffer = InDataFacade->GetBroadcaster<int32>(InDescriptor.Selector, true);
			OutProxy = TypedProxy;
		}

		PCGEX_RETURN_OUT_PROXY


	}

#pragma region externalization GetProxyBuffer

#define PCGEX_TPL(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) \
template PCGEXTENDEDTOOLKIT_API TSharedPtr<IBufferProxy> GetProxyBuffer<_TYPE_A, _TYPE_B>(FPCGExContext* InContext, const FProxyDescriptor& InDescriptor, const TSharedPtr<FFacade>& InDataFacade, UPCGBasePointData* PointData);
	PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	TSharedPtr<IBufferProxy> GetProxyBuffer(
		FPCGExContext* InContext,
		const FProxyDescriptor& InDescriptor)
	{
		TSharedPtr<IBufferProxy> OutProxy = nullptr;
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

#define PCGEX_SWITCHON_WORKING(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...)	case EPCGMetadataTypes::_NAME_B : OutProxy = GetProxyBuffer<_TYPE_A, _TYPE_B>(InContext, InDescriptor, InDataFacade, PointData); break;
#define PCGEX_SWITCHON_REAL(_TYPE, _NAME, ...)	case EPCGMetadataTypes::_NAME :	switch (InDescriptor.WorkingType){	PCGEX_INNER_FOREACH_TYPE2(_TYPE, _NAME, PCGEX_SWITCHON_WORKING) } break;

		switch (InDescriptor.RealType)
		{
		PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_SWITCHON_REAL)
		default: break;
		}

#undef PCGEX_SWITCHON_WORKING
#undef PCGEX_SWITCHON_REAL

		if (OutProxy)
		{
			OutProxy->Data = PointData;

			if (!OutProxy->Validate(InDescriptor))
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Proxy buffer doesn't match desired T_REAL and T_WORKING : \"{0}\""), FText::FromString(PCGEx::GetSelectorDisplayName(InDescriptor.Selector))));
				OutProxy = nullptr;
			}
		}

		PCGEX_RETURN_OUT_PROXY
	}

#undef PCGEX_RETURN_OUT_PROXY
	
	template <typename T>
	TSharedPtr<IBufferProxy> GetConstantProxyBuffer(const T& Constant)
	{
		TSharedPtr<TConstantProxy<T>> TypedProxy = MakeShared<TConstantProxy<T>>();
		TypedProxy->SetConstant(Constant);
		return TypedProxy;
	}

#pragma region externalization GetConstantProxyBuffer

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


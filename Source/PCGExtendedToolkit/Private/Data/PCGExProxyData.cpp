// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExProxyData.h"

namespace PCGExData
{
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

	bool FProxyDescriptor::Capture(FPCGExContext* InContext, const FString& Path, const ESource InSource, const bool bThrowError)
	{
		const TSharedPtr<FFacade> InFacade = DataFacade.Pin();
		check(InFacade);

		bool bValid = true;

		Selector = FPCGAttributePropertyInputSelector();
		Selector.Update(Path);

		Source = InSource;

		if (!PCGEx::TryGetTypeAndSource(Selector, InFacade, RealType, Source))
		{
			if (bThrowError) { PCGEX_LOG_INVALID_SELECTOR_C(InContext, Attribute, Selector) }
			bValid = false;
		}

		Selector = Selector.CopyAndFixLast(InFacade->Source->GetData(Source));

		UpdateSubSelection();
		WorkingType = SubSelection.GetSubType(RealType);

		return bValid;
	}

	bool FProxyDescriptor::Capture(FPCGExContext* InContext, const FPCGAttributePropertyInputSelector& InSelector, const ESource InSource, const bool bThrowError)
	{
		const TSharedPtr<FFacade> InFacade = DataFacade.Pin();
		check(InFacade);

		bool bValid = true;
		Source = InSource;

		if (!PCGEx::TryGetTypeAndSource(InSelector, InFacade, RealType, Source))
		{
			if (bThrowError) { PCGEX_LOG_INVALID_SELECTOR_C(InContext, Attribute, InSelector) }
			bValid = false;
		}

		Selector = InSelector.CopyAndFixLast(InFacade->Source->GetData(Source));

		UpdateSubSelection();
		WorkingType = SubSelection.GetSubType(RealType);

		return bValid;
	}

	bool FProxyDescriptor::CaptureStrict(FPCGExContext* InContext, const FString& Path, const ESource InSource, const bool bThrowError)
	{
		if (!Capture(InContext, Path, InSource, bThrowError)) { return false; }

		if (Source != InSource)
		{
			if (bThrowError)
			{
				if (InSource == ESource::In)
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

	bool FProxyDescriptor::CaptureStrict(FPCGExContext* InContext, const FPCGAttributePropertyInputSelector& InSelector, const ESource InSource, const bool bThrowError)
	{
		if (!Capture(InContext, InSelector, InSource, bThrowError)) { return false; }

		if (Source != InSource)
		{
			if (bThrowError)
			{
				if (InSource == ESource::In)
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

	TSharedPtr<FBufferProxyBase> GetProxyBuffer(
		FPCGExContext* InContext,
		const FProxyDescriptor& InDescriptor)
	{
		const TSharedPtr<FFacade> InDataFacade = InDescriptor.DataFacade.Pin();

		if (!InDataFacade)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Proxy descriptor has no valid source."));
			return nullptr;
		}

		TSharedPtr<FBufferProxyBase> OutProxy = nullptr;
		const bool bSubSelection = InDescriptor.SubSelection.bIsValid;

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
								const FPCGMetadataAttribute<T_REAL>* Attribute = InDataFacade->GetIn()->Metadata->GetConstTypedAttribute<T_REAL>(InDescriptor.Selector.GetAttributeName());
								if (!Attribute)
								{
									TypedProxy->SetConstant(0);
									return;
								}

								const TArray<FPCGPoint>& Points = InDataFacade->GetIn()->GetPoints();
								if (Points.IsEmpty())
								{
									// No points, get default attribute value
									TypedProxy->SetConstant(Attribute->GetValueFromItemKey(PCGInvalidEntryKey));
								}
								else
								{
									TypedProxy->SetConstant(Attribute->GetValueFromItemKey(Points[0].MetadataEntry));
								}
							}
							else if (InDescriptor.Selector.GetSelection() == EPCGAttributePropertySelection::PointProperty)
							{
								const TArray<FPCGPoint>& Points = InDataFacade->GetIn()->GetPoints();
								if (Points.IsEmpty())
								{
									TypedProxy->SetConstant(0);
								}
								else
								{
									const FPCGPoint& Pt = Points[0];

#define PCGEX_SET_CONST(_PROPERTY, _ACCESSOR, _TYPE) \
									case _PROPERTY : \
									if (bSubSelection) { TypedProxy->SetConstant(Pt._ACCESSOR); } \
									else { TypedProxy->SetConstant(Pt._ACCESSOR); } \
									break;

									switch (InDescriptor.Selector.GetPointProperty()) { PCGEX_FOREACH_POINTPROPERTY(PCGEX_SET_CONST) }
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
							if (bSubSelection)
							{
								TSharedPtr<TAttributeBufferProxy<T_REAL, T_WORKING, true>> TypedProxy =
									MakeShared<TAttributeBufferProxy<T_REAL, T_WORKING, true>>();

								TSharedPtr<TBuffer<T_REAL>> ExistingBuffer = InDataFacade->FindBuffer<T_REAL>(InDescriptor.Selector.GetAttributeName());
								TSharedPtr<TBuffer<T_REAL>> Buffer;

								if (InDescriptor.Source == ESource::In && ExistingBuffer && ExistingBuffer->IsWritable())
								{
									// Read from writer
									Buffer = InDataFacade->GetScopedReadable<T_REAL>(InDescriptor.Selector.GetAttributeName(), ESource::Out);
								}

								if (!Buffer)
								{
									// Fallback, make a writer that inherit data instead
									// This will create issues if the same name is used with different types
									Buffer = InDataFacade->GetWritable<T_REAL>(InDescriptor.Selector.GetAttributeName(), T_REAL{}, true, EBufferInit::Inherit);
								}

								if (!Buffer)
								{
									// TODO : Identify and log error
									PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("An attribute is using the same name with different types."));
									return;
								}

								if (!Buffer->EnsureReadable())
								{
									PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Fail to make buffer readable."));
									return;
								}

								TypedProxy->Buffer = Buffer;
								OutProxy = TypedProxy;
							}
							else
							{
								TSharedPtr<TAttributeBufferProxy<T_REAL, T_WORKING, false>> TypedProxy =
									MakeShared<TAttributeBufferProxy<T_REAL, T_WORKING, false>>();

								TSharedPtr<TBuffer<T_REAL>> ExistingBuffer = InDataFacade->FindBuffer<T_REAL>(InDescriptor.Selector.GetAttributeName());
								TSharedPtr<TBuffer<T_REAL>> Buffer;

								if (InDescriptor.Source == ESource::In && ExistingBuffer && ExistingBuffer->IsWritable())
								{
									// Read from writer
									Buffer = InDataFacade->GetScopedReadable<T_REAL>(InDescriptor.Selector.GetAttributeName(), ESource::Out);
								}

								if (!Buffer)
								{
									// Fallback, make a writer that inherit data instead
									// This will create issues if the same name is used with different types
									Buffer = InDataFacade->GetWritable<T_REAL>(InDescriptor.Selector.GetAttributeName(), T_REAL{}, true, EBufferInit::Inherit);
								}

								if (!Buffer)
								{
									// TODO : Identify and log error
									PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("An attribute is using the same name with different types."));
									return;
								}

								if (!Buffer->EnsureReadable())
								{
									PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Fail to make buffer readable."));
									return;
								}

								TypedProxy->Buffer = Buffer;
								OutProxy = TypedProxy;
							}
						}
						else if (InDescriptor.Selector.GetSelection() == EPCGAttributePropertySelection::PointProperty)
						{
#define PCGEX_DECL_PROXY(_PROPERTY, _ACCESSOR, _TYPE) \
						case _PROPERTY : \
						if (bSubSelection) { OutProxy = MakeShared<TPointPropertyProxy<_TYPE, T_WORKING, true, _PROPERTY>>(); } \
						else { OutProxy = MakeShared<TPointPropertyProxy<_TYPE, T_WORKING, false, _PROPERTY>>(); } \
						break;

							switch (InDescriptor.Selector.GetPointProperty()) { PCGEX_FOREACH_POINTPROPERTY(PCGEX_DECL_PROXY) }
#undef PCGEX_DECL_PROXY
						}
						else
						{
							if (bSubSelection)
							{
								TSharedPtr<TPointExtraPropertyProxy<int32, T_WORKING, true, EPCGExtraProperties::Index>> TypedProxy =
									MakeShared<TPointExtraPropertyProxy<int32, T_WORKING, true, EPCGExtraProperties::Index>>();

								TypedProxy->Buffer = InDataFacade->GetScopedBroadcaster<int32>(InDescriptor.Selector);
								OutProxy = TypedProxy;
							}
							else
							{
								TSharedPtr<TPointExtraPropertyProxy<int32, T_WORKING, false, EPCGExtraProperties::Index>> TypedProxy =
									MakeShared<TPointExtraPropertyProxy<int32, T_WORKING, false, EPCGExtraProperties::Index>>();

								TypedProxy->Buffer = InDataFacade->GetScopedBroadcaster<int32>(InDescriptor.Selector);
								OutProxy = TypedProxy;
							}
						}
					});
			});

		if (OutProxy && !OutProxy->Validate(InDescriptor))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Proxy buffer doesn't match desired T_REAL and T_WORKING"));
			return nullptr;
		}

		OutProxy->SubSelection = InDescriptor.SubSelection;

		return OutProxy;
	}

	bool GetPerFieldProxyBuffers(
		FPCGExContext* InContext,
		const FProxyDescriptor& InBaseDescriptor,
		const int32 NumDesiredFields,
		TArray<TSharedPtr<FBufferProxyBase>>& OutProxies)
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
				const TSharedPtr<FBufferProxyBase> Proxy = GetProxyBuffer(InContext, InBaseDescriptor);
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

				const TSharedPtr<FBufferProxyBase> Proxy = GetProxyBuffer(InContext, SingleFieldCopy);
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

				const TSharedPtr<FBufferProxyBase> Proxy = GetProxyBuffer(InContext, SingleFieldCopy);
				if (!Proxy) { return false; }
				OutProxies.Add(Proxy);
			}
		}

		return true;
	}
}

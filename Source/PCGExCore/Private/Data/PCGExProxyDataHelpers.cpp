// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExProxyDataHelpers.h"

#include "Data/PCGExData.h"
#include "Data/PCGExProxyData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExSubSelectionOps.h"

namespace PCGExData
{
	template <typename T_REAL>
	void TryGetInOutAttr(
		const FProxyDescriptor& InDescriptor,
		const TSharedPtr<FFacade>& InDataFacade,
		const FPCGMetadataAttribute<T_REAL>*& OutInAttribute,
		FPCGMetadataAttribute<T_REAL>*& OutOutAttribute)
	{
		OutInAttribute = nullptr;
		OutOutAttribute = nullptr;

		if (InDescriptor.Role == EProxyRole::Read)
		{
			if (InDescriptor.Side == EIOSide::In)
			{
				OutInAttribute = PCGExMetaHelpers::TryGetConstAttribute<T_REAL>(
					InDataFacade->GetIn(),
					PCGExMetaHelpers::GetAttributeIdentifier(InDescriptor.Selector, InDataFacade->GetIn()));
			}
			else
			{
				OutInAttribute = PCGExMetaHelpers::TryGetConstAttribute<T_REAL>(
					InDataFacade->GetOut(),
					PCGExMetaHelpers::GetAttributeIdentifier(InDescriptor.Selector, InDataFacade->GetIn()));
			}

			if (OutInAttribute)
			{
				OutOutAttribute = const_cast<FPCGMetadataAttribute<T_REAL>*>(OutInAttribute);
			}

			check(OutInAttribute);
		}
		else if (InDescriptor.Role == EProxyRole::Write)
		{
			OutOutAttribute = InDataFacade->Source->FindOrCreateAttribute(
				PCGExMetaHelpers::GetAttributeIdentifier(InDescriptor.Selector, InDataFacade->GetOut()),
				T_REAL{});
			if (OutOutAttribute)
			{
				OutInAttribute = OutOutAttribute;
			}

			check(OutOutAttribute);
		}
	}

	template <typename T_REAL>
	TSharedPtr<TBuffer<T_REAL>> TryGetBuffer(
		FPCGExContext* InContext,
		const FProxyDescriptor& InDescriptor,
		const TSharedPtr<FFacade>& InDataFacade)
	{
		const FPCGAttributeIdentifier Identifier = PCGExMetaHelpers::GetAttributeIdentifier(
			InDescriptor.Selector,
			InDescriptor.Side == EIOSide::In ? InDataFacade->GetIn() : InDataFacade->GetOut());

		// Check for existing buffer
		TSharedPtr<TBuffer<T_REAL>> ExistingBuffer = InDataFacade->FindBuffer<T_REAL>(Identifier);
		TSharedPtr<TBuffer<T_REAL>> Buffer;

		if (InDescriptor.Role == EProxyRole::Read)
		{
			if (InDescriptor.Side == EIOSide::In)
			{
				if (ExistingBuffer && ExistingBuffer->IsReadable())
				{
					Buffer = ExistingBuffer;
				}

				if (!Buffer)
				{
					Buffer = InDataFacade->GetReadable<T_REAL>(Identifier, EIOSide::In, true);
				}
			}
			else if (InDescriptor.Side == EIOSide::Out)
			{
				if (ExistingBuffer)
				{
					if (ExistingBuffer->ReadsFromOutput())
					{
						Buffer = ExistingBuffer;
					}

					if (!Buffer)
					{
						if (ExistingBuffer->IsWritable())
						{
							Buffer = InDataFacade->GetReadable<T_REAL>(Identifier, EIOSide::Out, true);
						}

						if (!Buffer)
						{
							PCGE_LOG_C(Error, GraphAndLog, InContext,
							           FTEXT("Trying to read from an output buffer that doesn't exist yet."));
							return nullptr;
						}
					}
				}
				else
				{
					Buffer = InDataFacade->GetWritable<T_REAL>(Identifier, T_REAL{}, true, EBufferInit::Inherit);
					if (Buffer)
					{
						Buffer->EnsureReadable();
					}
					else
					{
						PCGE_LOG_C(Error, GraphAndLog, InContext,
						           FTEXT("Could not create read/write buffer."));
						return nullptr;
					}
				}
			}
		}
		else if (InDescriptor.Role == EProxyRole::Write)
		{
			Buffer = InDataFacade->GetWritable<T_REAL>(Identifier, T_REAL{}, true, EBufferInit::Inherit);
		}

		return Buffer;
	}

#pragma region externalization TryGetInOutAttr / TryGetBuffer

#define PCGEX_TPL(_TYPE, _NAME, ...) \
	template PCGEXCORE_API void TryGetInOutAttr<_TYPE>( \
		const FProxyDescriptor& InDescriptor, \
		const TSharedPtr<FFacade>& InDataFacade, \
		const FPCGMetadataAttribute<_TYPE>*& OutInAttribute, \
		FPCGMetadataAttribute<_TYPE>*& OutOutAttribute); \
	template PCGEXCORE_API TSharedPtr<TBuffer<_TYPE>> TryGetBuffer<_TYPE>( \
		FPCGExContext* InContext, \
		const FProxyDescriptor& InDescriptor, \
		const TSharedPtr<FFacade>& InDataFacade);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

	//
	// Internal proxy creation helpers
	//
	namespace Internal
	{
		template <typename T_REAL>
		TSharedPtr<IBufferProxy> CreateAttributeProxy(
			FPCGExContext* InContext,
			const FProxyDescriptor& InDescriptor,
			const TSharedPtr<FFacade>& InDataFacade)
		{
			TSharedPtr<TBuffer<T_REAL>> Buffer = TryGetBuffer<T_REAL>(InContext, InDescriptor, InDataFacade);

			if (!Buffer)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Failed to initialize proxy buffer."));
				return nullptr;
			}

			auto Proxy = MakeShared<TAttributeBufferProxy<T_REAL>>(InDescriptor.WorkingType);
			Proxy->Buffer = Buffer;
			return Proxy;
		}

		template <typename T_REAL>
		TSharedPtr<IBufferProxy> CreateDirectProxy(
			const FProxyDescriptor& InDescriptor,
			const TSharedPtr<FFacade>& InDataFacade,
			bool bIsDataDomain)
		{
			const FPCGMetadataAttribute<T_REAL>* InAttribute = nullptr;
			FPCGMetadataAttribute<T_REAL>* OutAttribute = nullptr;

			TryGetInOutAttr<T_REAL>(InDescriptor, InDataFacade, InAttribute, OutAttribute);

			if (bIsDataDomain)
			{
				auto Proxy = MakeShared<TDirectDataAttributeProxy<T_REAL>>(InDescriptor.WorkingType);
				Proxy->InAttribute = InAttribute;
				Proxy->OutAttribute = OutAttribute;
				return Proxy;
			}
			auto Proxy = MakeShared<TDirectAttributeProxy<T_REAL>>(InDescriptor.WorkingType);
			Proxy->InAttribute = InAttribute;
			Proxy->OutAttribute = OutAttribute;
			return Proxy;
		}

		template <typename T_CONST>
		TSharedPtr<IBufferProxy> CreateConstantProxyFromProperty(
			const FProxyDescriptor& InDescriptor,
			UPCGBasePointData* PointData)
		{
			auto Proxy = MakeShared<TConstantProxy<T_CONST>>(InDescriptor.WorkingType);

			if (!PointData->IsEmpty())
			{
				const FConstPoint Point(PointData, 0);

				switch (InDescriptor.Selector.GetPointProperty())
				{
				case EPCGPointProperties::Density: Proxy->SetConstant(Point.GetDensity());
					break;
				case EPCGPointProperties::BoundsMin: Proxy->SetConstant(Point.GetBoundsMin());
					break;
				case EPCGPointProperties::BoundsMax: Proxy->SetConstant(Point.GetBoundsMax());
					break;
				case EPCGPointProperties::Extents: Proxy->SetConstant(Point.GetExtents());
					break;
				case EPCGPointProperties::Color: Proxy->SetConstant(Point.GetColor());
					break;
				case EPCGPointProperties::Position: Proxy->SetConstant(Point.GetLocation());
					break;
				case EPCGPointProperties::Rotation: Proxy->SetConstant(Point.GetRotation());
					break;
				case EPCGPointProperties::Scale: Proxy->SetConstant(Point.GetScale3D());
					break;
				case EPCGPointProperties::Transform: Proxy->SetConstant(Point.GetTransform());
					break;
				case EPCGPointProperties::Steepness: Proxy->SetConstant(Point.GetSteepness());
					break;
				case EPCGPointProperties::LocalCenter: Proxy->SetConstant(Point.GetLocalCenter());
					break;
				case EPCGPointProperties::Seed: Proxy->SetConstant(Point.GetSeed());
					break;
				default: Proxy->SetConstant(T_CONST{});
					break;
				}
			}
			else
			{
				Proxy->SetConstant(T_CONST{});
			}

			return Proxy;
		}
	}

	template <typename T>
	TSharedPtr<IBufferProxy> GetConstantProxyBuffer(const T& Constant, EPCGMetadataTypes InWorkingType)
	{
		auto Proxy = MakeShared<TConstantProxy<T>>(InWorkingType);
		Proxy->SetConstant(Constant);
		return Proxy;
	}

#pragma region externalization GetConstantProxyBuffer

#define PCGEX_TPL(_TYPE, _NAME, ...) \
template PCGEXCORE_API TSharedPtr<IBufferProxy> GetConstantProxyBuffer<_TYPE>(const _TYPE& Constant, EPCGMetadataTypes InWorkingType);
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

	TSharedPtr<IBufferProxy> GetProxyBuffer(FPCGExContext* InContext, const FProxyDescriptor& InDescriptor)
	{
		TSharedPtr<IBufferProxy> OutProxy = nullptr;
		const TSharedPtr<FFacade> InDataFacade = InDescriptor.DataFacade.Pin();
		UPCGBasePointData* PointData = nullptr;

		// Determine point data source
		if (!InDataFacade)
		{
			PointData = const_cast<UPCGBasePointData*>(InDescriptor.PointData);

			if (PointData && InDescriptor.Selector.GetSelection() == EPCGAttributePropertySelection::Property)
			{
				// Property-only access without facade - OK
			}
			else
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Proxy descriptor has no valid source."));
				return nullptr;
			}
		}
		else
		{
			if (InDescriptor.bIsConstant || InDescriptor.Side == EIOSide::In)
			{
				PointData = const_cast<UPCGBasePointData*>(InDataFacade->GetIn());
			}
			else
			{
				PointData = InDataFacade->GetOut();
			}

			if (!PointData)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Proxy descriptor attempted to work with a null PointData."));
				return nullptr;
			}
		}

		// Handle constant proxy
		if (InDescriptor.bIsConstant)
		{
			const PCGMetadataEntryKey Key =
				InDataFacade->GetIn()->IsEmpty()
					? PCGInvalidEntryKey
					: InDataFacade->GetIn()->GetMetadataEntry(0);

			PCGExMetaHelpers::ExecuteWithRightType(InDescriptor.RealType, [&](auto DummyValue)
			{
				using T = decltype(DummyValue);

				if (InDescriptor.Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
				{
					if (const FPCGMetadataAttribute<T>* Attr = PCGExMetaHelpers::TryGetConstAttribute<T>(InDataFacade->GetIn(), PCGExMetaHelpers::GetAttributeIdentifier(InDescriptor.Selector, InDataFacade->GetIn())))
					{
						OutProxy = GetConstantProxyBuffer<T>(Attr->GetValueFromItemKey(Key), InDescriptor.WorkingType);
					}
				}
				else if (InDescriptor.Selector.GetSelection() == EPCGAttributePropertySelection::Property)
				{
					OutProxy = Internal::CreateConstantProxyFromProperty<T>(InDescriptor, PointData);
				}
			});

			if (OutProxy) { OutProxy->SetSubSelection(InDescriptor.SubSelection); }
			return OutProxy;
		}

		// Handle attribute proxy
		if (InDescriptor.Selector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			if (InDescriptor.bWantsDirect)
			{
				// Direct attribute access
				const FPCGAttributeIdentifier Identifier = PCGExMetaHelpers::GetAttributeIdentifier(InDescriptor.Selector, InDataFacade->GetIn());
				const FPCGMetadataAttributeBase* BaseAttr = InDataFacade->FindConstAttribute(Identifier, InDescriptor.Side);
				const bool bIsDataDomain = BaseAttr && BaseAttr->GetMetadataDomain()->GetDomainID().Flag == EPCGMetadataDomainFlag::Data;

				PCGExMetaHelpers::ExecuteWithRightType(InDescriptor.RealType, [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					OutProxy = Internal::CreateDirectProxy<T>(InDescriptor, InDataFacade, bIsDataDomain);
				});
			}
			else
			{
				// Buffered attribute access
				PCGExMetaHelpers::ExecuteWithRightType(InDescriptor.RealType, [&](auto DummyValue)
				{
					using T = decltype(DummyValue);
					OutProxy = Internal::CreateAttributeProxy<T>(InContext, InDescriptor, InDataFacade);
				});
			}
		}
		// Handle point property proxy
		else if (InDescriptor.Selector.GetSelection() == EPCGAttributePropertySelection::Property)
		{
			OutProxy = MakeShared<FPointPropertyProxy>(InDescriptor.Selector.GetPointProperty(), InDescriptor.WorkingType);
		}
		// Handle extra property proxy
		else
		{
			OutProxy = MakeShared<FPointExtraPropertyProxy>(EPCGExtraProperties::Index, InDescriptor.WorkingType);
		}

		// Finalize proxy setup
		if (OutProxy)
		{
			OutProxy->Data = PointData;
			OutProxy->SetSubSelection(InDescriptor.SubSelection);
			OutProxy->InitForRole(InDescriptor.Role);

			if (!OutProxy->Validate(InDescriptor))
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format( FTEXT("Proxy buffer doesn't match desired types: \"{0}\""), FText::FromString(PCGExMetaHelpers::GetSelectorDisplayName(InDescriptor.Selector))));
				OutProxy = nullptr;
			}
		}

		return OutProxy;
	}

#pragma endregion

	bool GetPerFieldProxyBuffers(
		FPCGExContext* InContext,
		const FProxyDescriptor& InBaseDescriptor,
		const int32 NumDesiredFields,
		TArray<TSharedPtr<IBufferProxy>>& OutProxies)
	{
		OutProxies.Reset(NumDesiredFields);
		const int32 Dimensions = FMath::Min(4, FSubSelectorRegistry::Get(InBaseDescriptor.RealType)->GetNumFields());

		if (Dimensions == -1 &&
			(!InBaseDescriptor.SubSelection.bIsValid || !InBaseDescriptor.SubSelection.bIsComponentSet))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext,
			           FTEXT("Can't automatically break complex type into sub-components. "
				           "Use a narrower selector or a supported type."));
			return false;
		}

		const int32 MaxIndex = Dimensions == -1 ? 2 : Dimensions - 1;

		if (InBaseDescriptor.SubSelection.bIsValid)
		{
			if (InBaseDescriptor.SubSelection.bIsFieldSet)
			{
				// Single specific field - use same proxy for all
				const TSharedPtr<IBufferProxy> Proxy = GetProxyBuffer(InContext, InBaseDescriptor);
				if (!Proxy) { return false; }

				for (int i = 0; i < NumDesiredFields; i++)
				{
					OutProxies.Add(Proxy);
				}
				return true;
			}

			// Create individual field proxies
			for (int i = 0; i < NumDesiredFields; i++)
			{
				FProxyDescriptor SingleFieldCopy = InBaseDescriptor;
				SingleFieldCopy.SetFieldIndex(FMath::Clamp(i, 0, MaxIndex));

				const TSharedPtr<IBufferProxy> Proxy = GetProxyBuffer(InContext, SingleFieldCopy);
				if (!Proxy) { return false; }
				OutProxies.Add(Proxy);
			}
		}
		else
		{
			// No subselection - create field proxies
			for (int i = 0; i < NumDesiredFields; i++)
			{
				FProxyDescriptor SingleFieldCopy = InBaseDescriptor;
				SingleFieldCopy.SetFieldIndex(FMath::Clamp(i, 0, MaxIndex));

				const TSharedPtr<IBufferProxy> Proxy = GetProxyBuffer(InContext, SingleFieldCopy);
				if (!Proxy) { return false; }
				OutProxies.Add(Proxy);
			}
		}

		return true;
	}
}

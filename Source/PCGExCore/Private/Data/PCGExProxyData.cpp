// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExProxyData.h"

#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExPointElements.h"
#include "Data/PCGPointArrayData.h"

namespace PCGExData
{
#pragma region FProxyDescriptor

	void FProxyDescriptor::UpdateSubSelection()
	{
		SubSelection = FSubSelection(Selector);
	}

	bool FProxyDescriptor::SetFieldIndex(const int32 InFieldIndex)
	{
		if (!SubSelection.SetFieldIndex(InFieldIndex)) { return false; }
		UpdateSubSelection();
		return true;
	}

	bool FProxyDescriptor::Capture(FPCGExContext* InContext, const FString& Path, const EIOSide InSide, const bool bRequired)
	{
		const TSharedPtr<FFacade> InFacade = DataFacade.Pin();
		check(InFacade);

		bool bValid = true;

		Selector = FPCGAttributePropertyInputSelector();
		Selector.Update(Path);

		Side = InSide;

		if (!TryGetTypeAndSource(Selector, InFacade, RealType, Side))
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
		Side = HasFlag(EProxyFlags::Constant) ? EIOSide::In : InSide;

		if (!TryGetTypeAndSource(InSelector, InFacade, RealType, Side))
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
					PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("\"{0}\" does not exist on input."), FText::FromString(PCGExMetaHelpers::GetSelectorDisplayName(InSelector))));
				}
				else
				{
					PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("\"{0}\" does not exist on output."), FText::FromString(PCGExMetaHelpers::GetSelectorDisplayName(InSelector))));
				}
			}

			return false;
		}

		return true;
	}

#pragma endregion

#pragma region IBufferProxy

	IBufferProxy::IBufferProxy(EPCGMetadataTypes InRealType, EPCGMetadataTypes InWorkingType)
		: RealType(InRealType)
		  , WorkingType(InWorkingType == EPCGMetadataTypes::Unknown ? InRealType : InWorkingType)
		  , WorkingToReal(PCGExTypeOps::FConversionTable::GetConversionFn(InWorkingType == EPCGMetadataTypes::Unknown ? InRealType : InWorkingType, InRealType))
		  , RealToWorking(PCGExTypeOps::FConversionTable::GetConversionFn(InRealType, InWorkingType == EPCGMetadataTypes::Unknown ? InRealType : InWorkingType))
	{
		// Get type ops from registry
		RealOps = PCGExTypeOps::FTypeOpsRegistry::Get(RealType);
		WorkingOps = PCGExTypeOps::FTypeOpsRegistry::Get(WorkingType);

		// Cache whether working type needs lifecycle management
		bWorkingTypeNeedsLifecycle = TypeTraits::NeedsLifecycleManagement(WorkingType);
	}

	bool IBufferProxy::Validate(const FProxyDescriptor& InDescriptor) const
	{
		return RealType == InDescriptor.RealType && WorkingType == InDescriptor.WorkingType;
	}

	void IBufferProxy::SetSubSelection(const FSubSelection& InSubSelection)
	{
		bWantsSubSelection = InSubSelection.bIsValid;
		if (bWantsSubSelection) { CachedSubSelection.Initialize(InSubSelection, RealType, WorkingType); }
	}

	void IBufferProxy::InitForRole(EProxyRole InRole)
	{
		// Default: no-op. Override in property proxies.
	}

	// Converting read implementations - Now using FScopedTypedValue for safety
#define PCGEX_CONVERTING_READ_IMPL(_TYPE, _NAME, ...) \
	_TYPE IBufferProxy::ReadAs##_NAME(const int32 Index) const \
	{ \
		PCGExTypes::FScopedTypedValue WorkingValue(WorkingType); \
		GetVoid(Index, WorkingValue.GetRaw()); \
		constexpr EPCGMetadataTypes TargetType = PCGExTypes::TTraits<_TYPE>::Type; \
		if (TargetType == WorkingType) \
		{ \
			if constexpr (TypeTraits::TIsComplexType<_TYPE>) { return WorkingValue.As<_TYPE>(); }\
			else { return *reinterpret_cast<const _TYPE*>(WorkingValue.GetRaw()); }\
		} \
		_TYPE Result{}; \
		PCGExTypeOps::FConversionTable::Convert(WorkingType, WorkingValue.GetRaw(), TargetType, &Result);\
		return Result; \
	}
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_CONVERTING_READ_IMPL)
#undef PCGEX_CONVERTING_READ_IMPL

#pragma endregion
}

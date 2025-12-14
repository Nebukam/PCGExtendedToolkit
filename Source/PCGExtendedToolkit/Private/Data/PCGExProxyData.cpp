// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExProxyData.h"

#include "PCGExHelpers.h"
#include "PCGExTypes.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExPointIO.h"
#include "Data/PCGExPointElements.h"
#include "Data/PCGPointArrayData.h"
#include "Types/PCGExTypeOpsImpl.h"

namespace PCGExData
{
#pragma region FProxyDescriptor

	void FProxyDescriptor::UpdateSubSelection()
	{
		SubSelection.Update();
	}

	bool FProxyDescriptor::SetFieldIndex(const int32 InFieldIndex)
	{
		if (!SubSelection.SetFieldIndex(InFieldIndex)) { return false; }
		UpdateSubSelection();
		return true;
	}

	bool FProxyDescriptor::Capture(FPCGExContext* InContext, const FString& Path, const EIOSide InSide, const bool bRequired)
	{
		FPCGAttributePropertyInputSelector TmpSelector = FPCGAttributePropertyInputSelector();
		TmpSelector.Update(Path);
		return Capture(InContext, TmpSelector, InSide, bRequired);
	}

	bool FProxyDescriptor::Capture(FPCGExContext* InContext, const FPCGAttributePropertyInputSelector& InSelector, const EIOSide InSide, const bool bRequired)
	{
		const TSharedPtr<FFacade> InDataFacade = DataFacade.Pin();
		if (!InDataFacade)
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Proxy descriptor has no source."));
			return false;
		}

		Selector = InSelector;
		Side = InSide;

		if (!PCGEx::TryGetTypeAndSource(Selector, InDataFacade, RealType, Side))
		{
			if (bRequired)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext,
				           FText::Format(FTEXT("Could not resolve selector: \"{0}\""),
					           FText::FromString(PCGEx::GetSelectorDisplayName(Selector))));
			}
			return false;
		}

		SubSelection = PCGEx::FSubSelection(Selector);
		WorkingType = SubSelection.GetSubType(RealType);
		return true;
	}

	bool FProxyDescriptor::CaptureStrict(FPCGExContext* InContext, const FString& Path, const EIOSide InSide, const bool bRequired)
	{
		FPCGAttributePropertyInputSelector TmpSelector = FPCGAttributePropertyInputSelector();
		TmpSelector.Update(Path);
		return CaptureStrict(InContext, TmpSelector, InSide, bRequired);
	}

	bool FProxyDescriptor::CaptureStrict(FPCGExContext* InContext, const FPCGAttributePropertyInputSelector& InSelector, const EIOSide InSide, const bool bRequired)
	{
		if (!Capture(InContext, InSelector, InSide, bRequired)) { return false; }
		WorkingType = RealType;
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
	}

	bool IBufferProxy::Validate(const FProxyDescriptor& InDescriptor) const
	{
		return RealType == InDescriptor.RealType && WorkingType == InDescriptor.WorkingType;
	}

	void IBufferProxy::SetSubSelection(const PCGEx::FSubSelection& InSubSelection)
	{
		SubSelection = InSubSelection;
		bWantsSubSelection = SubSelection.bIsValid;
	}

	void IBufferProxy::InitForRole(EProxyRole InRole)
	{
		// Default: no-op. Override in property proxies.
	}

	// Converting read implementations
#define PCGEX_CONVERTING_READ_IMPL(_TYPE, _NAME, ...) \
	_TYPE IBufferProxy::ReadAs##_NAME(const int32 Index) const \
	{ \
		alignas(16) uint8 WorkingBuffer[64]; \
		GetVoid(Index, WorkingBuffer); \
		constexpr EPCGMetadataTypes TargetType = PCGExTypeOps::TTypeToMetadata<_TYPE>::Type; \
		if (TargetType == WorkingType) { return *reinterpret_cast<const _TYPE*>(WorkingBuffer); } \
		_TYPE Result{}; \
		if (WorkingOps) { WorkingOps->ConvertTo(WorkingBuffer, TargetType, &Result); } \
		return Result; \
	}
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_CONVERTING_READ_IMPL)
#undef PCGEX_CONVERTING_READ_IMPL

#pragma endregion

#pragma region TAttributeBufferProxy

	template <typename T_REAL>
	TAttributeBufferProxy<T_REAL>::TAttributeBufferProxy(EPCGMetadataTypes InWorkingType)
		: IBufferProxy(PCGExTypeOps::TTypeToMetadata<T_REAL>::Type, InWorkingType)
	{
	}

	template <typename T_REAL>
	void TAttributeBufferProxy<T_REAL>::GetVoid(const int32 Index, void* OutValue) const
	{
		check(Buffer);
		const T_REAL& RealValue = Buffer->Read(Index);

		if (bWantsSubSelection)
		{
			// Use type-erased sub-selection extraction
			SubSelection.GetVoid(RealType, &RealValue, WorkingType, OutValue);
		}
		else if (RealType != WorkingType)
		{
			// Use direct conversion function
			if (RealToWorking)
			{
				RealToWorking(&RealValue, OutValue);
			}
		}
		else
		{
			*static_cast<T_REAL*>(OutValue) = RealValue;
		}
	}

	template <typename T_REAL>
	void TAttributeBufferProxy<T_REAL>::SetVoid(const int32 Index, const void* Value) const
	{
		check(Buffer);

		if (bWantsSubSelection)
		{
			// Read current value, apply sub-selection, write back
			T_REAL RealValue = Buffer->GetValue(Index);
			SubSelection.SetVoid(RealType, &RealValue, WorkingType, Value);
			Buffer->SetValue(Index, RealValue);
		}
		else if (RealType != WorkingType)
		{
			// Use direct conversion function
			T_REAL RealValue{};
			if (WorkingToReal)
			{
				WorkingToReal(Value, &RealValue);
			}
			Buffer->SetValue(Index, RealValue);
		}
		else
		{
			if constexpr (std::is_trivially_copyable_v<T_REAL>)
			{
				Buffer->SetValue(Index, *static_cast<const T_REAL*>(Value));
			}
			else
			{
				// Value is raw bytes, not a live object
				T_REAL RealValue;
				FMemory::Memcpy(&RealValue, Value, sizeof(T_REAL));
				Buffer->SetValue(Index, MoveTemp(RealValue));
			}
		}
	}

	template <typename T_REAL>
	void TAttributeBufferProxy<T_REAL>::GetCurrentVoid(const int32 Index, void* OutValue) const
	{
		check(Buffer);
		const T_REAL& RealValue = Buffer->GetValue(Index);

		if (RealType != WorkingType)
		{
			if (RealToWorking)
			{
				RealToWorking(&RealValue, OutValue);
			}
		}
		else
		{
			*static_cast<T_REAL*>(OutValue) = RealValue;
		}
	}

	template <typename T_REAL>
	TSharedPtr<IBuffer> TAttributeBufferProxy<T_REAL>::GetBuffer() const
	{
		return Buffer;
	}

	template <typename T_REAL>
	bool TAttributeBufferProxy<T_REAL>::EnsureReadable() const
	{
		return Buffer && Buffer->EnsureReadable();
	}

	template <typename T_REAL>
	PCGExValueHash TAttributeBufferProxy<T_REAL>::ReadValueHash(const int32 Index) const
	{
		check(Buffer);
		return Buffer->ReadValueHash(Index);
	}

	// Explicit instantiations
#define PCGEX_TPL(_TYPE, _NAME, ...) template class TAttributeBufferProxy<_TYPE>;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

#pragma region FPointPropertyProxy

	FPointPropertyProxy::FPointPropertyProxy(EPCGPointProperties InProperty, EPCGMetadataTypes InWorkingType)
		: IBufferProxy(PCGEx::GetPropertyType(InProperty), InWorkingType)
		  , Property(InProperty)
		  , PropertyRealType(PCGEx::GetPropertyType(InProperty))
	{
	}

	void FPointPropertyProxy::GetPropertyValue(const int32 Index, void* OutValue) const
	{
		check(Data);
		const FConstPoint Point(Data, Index);

		switch (Property)
		{
		case EPCGPointProperties::Density: *static_cast<float*>(OutValue) = Point.GetDensity();
			break;
		case EPCGPointProperties::BoundsMin: *static_cast<FVector*>(OutValue) = Point.GetBoundsMin();
			break;
		case EPCGPointProperties::BoundsMax: *static_cast<FVector*>(OutValue) = Point.GetBoundsMax();
			break;
		case EPCGPointProperties::Extents: *static_cast<FVector*>(OutValue) = Point.GetExtents();
			break;
		case EPCGPointProperties::Color: *static_cast<FVector4*>(OutValue) = Point.GetColor();
			break;
		case EPCGPointProperties::Position: *static_cast<FVector*>(OutValue) = Point.GetLocation();
			break;
		case EPCGPointProperties::Rotation: *static_cast<FQuat*>(OutValue) = Point.GetRotation();
			break;
		case EPCGPointProperties::Scale: *static_cast<FVector*>(OutValue) = Point.GetScale3D();
			break;
		case EPCGPointProperties::Transform: *static_cast<FTransform*>(OutValue) = Point.GetTransform();
			break;
		case EPCGPointProperties::Steepness: *static_cast<float*>(OutValue) = Point.GetSteepness();
			break;
		case EPCGPointProperties::LocalCenter: *static_cast<FVector*>(OutValue) = Point.GetLocalCenter();
			break;
		case EPCGPointProperties::LocalSize: *static_cast<FVector*>(OutValue) = Point.GetLocalSize();
			break;
		case EPCGPointProperties::ScaledLocalSize: *static_cast<FVector*>(OutValue) = Point.GetScaledLocalSize();
			break;
		case EPCGPointProperties::Seed: *static_cast<int32*>(OutValue) = Point.GetSeed();
			break;
		default: if (RealOps) { RealOps->SetDefault(OutValue); }
			break;
		}
	}

	void FPointPropertyProxy::SetPropertyValue(const int32 Index, const void* Value) const
	{
		check(Data);
		FMutablePoint Point(Data, Index);

		switch (Property)
		{
		case EPCGPointProperties::Density: Point.SetDensity(*static_cast<const float*>(Value));
			break;
		case EPCGPointProperties::BoundsMin: Point.SetBoundsMin(*static_cast<const FVector*>(Value));
			break;
		case EPCGPointProperties::BoundsMax: Point.SetBoundsMax(*static_cast<const FVector*>(Value));
			break;
		case EPCGPointProperties::Extents: Point.SetExtents(*static_cast<const FVector*>(Value));
			break;
		case EPCGPointProperties::Color: Point.SetColor(*static_cast<const FVector4*>(Value));
			break;
		case EPCGPointProperties::Position: Point.SetLocation(*static_cast<const FVector*>(Value));
			break;
		case EPCGPointProperties::Rotation: Point.SetRotation(*static_cast<const FQuat*>(Value));
			break;
		case EPCGPointProperties::Scale: Point.SetScale3D(*static_cast<const FVector*>(Value));
			break;
		case EPCGPointProperties::Transform: Point.SetTransform(*static_cast<const FTransform*>(Value));
			break;
		case EPCGPointProperties::Steepness: Point.SetSteepness(*static_cast<const float*>(Value));
			break;
		case EPCGPointProperties::LocalCenter: Point.SetLocalCenter(*static_cast<const FVector*>(Value));
			break;
		case EPCGPointProperties::Seed: Point.SetSeed(*static_cast<const int32*>(Value));
			break;
		default: break;
		}
	}

	void FPointPropertyProxy::GetVoid(const int32 Index, void* OutValue) const
	{
		alignas(16) uint8 PropertyBuffer[64];
		GetPropertyValue(Index, PropertyBuffer);

		if (bWantsSubSelection)
		{
			// Use type-erased sub-selection extraction
			SubSelection.GetVoid(PropertyRealType, PropertyBuffer, WorkingType, OutValue);
		}
		else if (PropertyRealType != WorkingType)
		{
			// Use direct conversion function
			if (RealToWorking)
			{
				RealToWorking(PropertyBuffer, OutValue);
			}
		}
		else
		{
			// Direct copy - sizes must match
			if (RealOps)
			{
				FMemory::Memcpy(OutValue, PropertyBuffer, RealOps->GetTypeSize());
			}
		}
	}

	void FPointPropertyProxy::SetVoid(const int32 Index, const void* Value) const
	{
		if (bWantsSubSelection)
		{
			// Read current property value, apply sub-selection, write back
			alignas(16) uint8 PropertyBuffer[64];
			GetPropertyValue(Index, PropertyBuffer);
			SubSelection.SetVoid(PropertyRealType, PropertyBuffer, WorkingType, Value);
			SetPropertyValue(Index, PropertyBuffer);
		}
		else if (PropertyRealType != WorkingType)
		{
			// Use direct conversion function
			alignas(16) uint8 PropertyBuffer[64];
			if (WorkingToReal)
			{
				WorkingToReal(Value, PropertyBuffer);
				SetPropertyValue(Index, PropertyBuffer);
			}
		}
		else
		{
			SetPropertyValue(Index, Value);
		}
	}

	void FPointPropertyProxy::InitForRole(EProxyRole InRole)
	{
		if (InRole == EProxyRole::Write && Data)
		{
			// Allocate property for writing
			if (UPCGPointArrayData* PointData = Cast<UPCGPointArrayData>(Data))
			{
				PointData->AllocateProperties(PCGEx::GetPropertyNativeTypes(Property));
			}
		}
	}

	PCGExValueHash FPointPropertyProxy::ReadValueHash(const int32 Index) const
	{
		alignas(16) uint8 PropertyBuffer[64];
		GetPropertyValue(Index, PropertyBuffer);

		const PCGExTypeOps::ITypeOpsBase* PropertyOps = PCGExTypeOps::FTypeOpsRegistry::Get(PropertyRealType);
		return PropertyOps ? PropertyOps->ComputeHash(PropertyBuffer) : 0;
	}

#pragma endregion

#pragma region FPointExtraPropertyProxy

	FPointExtraPropertyProxy::FPointExtraPropertyProxy(EPCGExtraProperties InProperty, EPCGMetadataTypes InWorkingType)
		: IBufferProxy(GetPropertyType(InProperty), InWorkingType)
		  , Property(InProperty)
	{
	}

	EPCGMetadataTypes FPointExtraPropertyProxy::GetPropertyType(EPCGExtraProperties InProperty)
	{
		switch (InProperty)
		{
		case EPCGExtraProperties::Index: return EPCGMetadataTypes::Integer32;
		default: return EPCGMetadataTypes::Unknown;
		}
	}

	void FPointExtraPropertyProxy::GetVoid(const int32 Index, void* OutValue) const
	{
		int32 Value = 0;

		switch (Property)
		{
		case EPCGExtraProperties::Index: Value = Index;
			break;
		default: break;
		}

		if (WorkingType == EPCGMetadataTypes::Integer32)
		{
			*static_cast<int32*>(OutValue) = Value;
		}
		else if (WorkingOps)
		{
			const PCGExTypeOps::ITypeOpsBase* Int32Ops = PCGExTypeOps::FTypeOpsRegistry::Get(EPCGMetadataTypes::Integer32);
			if (Int32Ops)
			{
				Int32Ops->ConvertTo(&Value, WorkingType, OutValue);
			}
		}
	}

	PCGExValueHash FPointExtraPropertyProxy::ReadValueHash(const int32 Index) const
	{
		int32 Value = 0;
		switch (Property)
		{
		case EPCGExtraProperties::Index: Value = Index;
			break;
		default: break;
		}
		return PCGExTypes::ComputeHash(Value);
	}

#pragma endregion

#pragma region TConstantProxy

	template <typename T_CONST>
	TConstantProxy<T_CONST>::TConstantProxy()
		: IBufferProxy(PCGExTypeOps::TTypeToMetadata<T_CONST>::Type, PCGExTypeOps::TTypeToMetadata<T_CONST>::Type)
	{
	}

	template <typename T_CONST>
	template <typename T>
	void TConstantProxy<T_CONST>::SetConstant(const T& InValue)
	{
		constexpr EPCGMetadataTypes SourceType = PCGExTypeOps::TTypeToMetadata<T>::Type;
		constexpr EPCGMetadataTypes ConstType = PCGExTypeOps::TTypeToMetadata<T_CONST>::Type;

		if constexpr (std::is_same_v<T, T_CONST>)
		{
			Constant = InValue;
		}
		else
		{
			PCGExTypeOps::FConversionTable::Convert(SourceType, &InValue, ConstType, &Constant);
		}
	}

	template <typename T_CONST>
	void TConstantProxy<T_CONST>::GetVoid(const int32 Index, void* OutValue) const
	{
		if (bWantsSubSelection)
		{
			// Use type-erased sub-selection extraction
			SubSelection.GetVoid(RealType, &Constant, WorkingType, OutValue);
		}
		else if (RealType != WorkingType)
		{
			if (RealOps)
			{
				RealOps->ConvertTo(&Constant, WorkingType, OutValue);
			}
		}
		else
		{
			*static_cast<T_CONST*>(OutValue) = Constant;
		}
	}

	template <typename T_CONST>
	bool TConstantProxy<T_CONST>::Validate(const FProxyDescriptor& InDescriptor) const
	{
		// Constants are more flexible with type matching
		return true;
	}

	template <typename T_CONST>
	PCGExValueHash TConstantProxy<T_CONST>::ReadValueHash(const int32 Index) const
	{
		return PCGExTypes::ComputeHash(Constant);
	}

	// Explicit instantiations
#define PCGEX_TPL(_TYPE, _NAME, ...) template class TConstantProxy<_TYPE>;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

	// SetConstant cross-type instantiations
#define PCGEX_TPL(_TYPE_A, _NAME_A, _TYPE_B, _NAME_B, ...) \
	template void TConstantProxy<_TYPE_A>::SetConstant<_TYPE_B>(const _TYPE_B&);
	PCGEX_FOREACH_SUPPORTEDTYPES_PAIRS(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

#pragma region TDirectAttributeProxy

	template <typename T_REAL>
	TDirectAttributeProxy<T_REAL>::TDirectAttributeProxy(EPCGMetadataTypes InWorkingType)
		: IBufferProxy(PCGExTypeOps::TTypeToMetadata<T_REAL>::Type, InWorkingType)
	{
	}

	template <typename T_REAL>
	void TDirectAttributeProxy<T_REAL>::GetVoid(const int32 Index, void* OutValue) const
	{
		check(InAttribute && Data);

		const T_REAL& RealValue = InAttribute->GetValueFromItemKey(Data->GetMetadataEntry(Index));

		if (bWantsSubSelection)
		{
			SubSelection.GetVoid(RealType, &RealValue, WorkingType, OutValue);
		}
		else if (RealType != WorkingType)
		{
			if (RealToWorking)
			{
				RealToWorking(&RealValue, OutValue);
			}
		}
		else
		{
			*static_cast<T_REAL*>(OutValue) = RealValue;
		}
	}

	template <typename T_REAL>
	void TDirectAttributeProxy<T_REAL>::GetCurrentVoid(const int32 Index, void* OutValue) const
	{
		check(OutAttribute && Data);

		const T_REAL& RealValue = OutAttribute->GetValueFromItemKey(Data->GetMetadataEntry(Index));

		if (bWantsSubSelection)
		{
			SubSelection.GetVoid(RealType, &RealValue, WorkingType, OutValue);
		}
		else if (RealType != WorkingType)
		{
			if (RealToWorking)
			{
				RealToWorking(&RealValue, OutValue);
			}
		}
		else
		{
			*static_cast<T_REAL*>(OutValue) = RealValue;
		}
	}

	template <typename T_REAL>
	void TDirectAttributeProxy<T_REAL>::SetVoid(const int32 Index, const void* Value) const
	{
		check(OutAttribute && Data);

		const int64 MetadataEntry = Data->GetMetadataEntry(Index);

		if (bWantsSubSelection)
		{
			T_REAL RealValue = OutAttribute->GetValueFromItemKey(MetadataEntry);
			SubSelection.SetVoid(RealType, &RealValue, WorkingType, Value);
			OutAttribute->SetValue(MetadataEntry, RealValue);
		}
		else if (RealType != WorkingType)
		{
			T_REAL RealValue{};
			if (WorkingToReal)
			{
				WorkingToReal(Value, &RealValue);
			}
			OutAttribute->SetValue(MetadataEntry, RealValue);
		}
		else
		{
			OutAttribute->SetValue(MetadataEntry, *static_cast<const T_REAL*>(Value));
		}
	}

	template <typename T_REAL>
	PCGExValueHash TDirectAttributeProxy<T_REAL>::ReadValueHash(const int32 Index) const
	{
		check(InAttribute && Data);
		return PCGExTypes::ComputeHash(InAttribute->GetValueFromItemKey(Data->GetMetadataEntry(Index)));
	}

	// Explicit instantiations
#define PCGEX_TPL(_TYPE, _NAME, ...) template class TDirectAttributeProxy<_TYPE>;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion

#pragma region TDirectDataAttributeProxy

	template <typename T_REAL>
	TDirectDataAttributeProxy<T_REAL>::TDirectDataAttributeProxy(EPCGMetadataTypes InWorkingType)
		: IBufferProxy(PCGExTypeOps::TTypeToMetadata<T_REAL>::Type, InWorkingType)
	{
	}

	template <typename T_REAL>
	void TDirectDataAttributeProxy<T_REAL>::GetVoid(const int32 Index, void* OutValue) const
	{
		check(InAttribute);

		// Data-domain: always use default entry key
		const T_REAL& RealValue = InAttribute->GetValueFromItemKey(PCGDefaultValueKey);

		if (bWantsSubSelection)
		{
			SubSelection.GetVoid(RealType, &RealValue, WorkingType, OutValue);
		}
		else if (RealType != WorkingType)
		{
			if (RealToWorking)
			{
				RealToWorking(&RealValue, OutValue);
			}
		}
		else
		{
			*static_cast<T_REAL*>(OutValue) = RealValue;
		}
	}

	template <typename T_REAL>
	void TDirectDataAttributeProxy<T_REAL>::GetCurrentVoid(const int32 Index, void* OutValue) const
	{
		check(OutAttribute);

		const T_REAL& RealValue = OutAttribute->GetValueFromItemKey(PCGDefaultValueKey);

		if (bWantsSubSelection)
		{
			SubSelection.GetVoid(RealType, &RealValue, WorkingType, OutValue);
		}
		else if (RealType != WorkingType)
		{
			if (RealToWorking)
			{
				RealToWorking(&RealValue, OutValue);
			}
		}
		else
		{
			*static_cast<T_REAL*>(OutValue) = RealValue;
		}
	}

	template <typename T_REAL>
	void TDirectDataAttributeProxy<T_REAL>::SetVoid(const int32 Index, const void* Value) const
	{
		check(OutAttribute);

		if (bWantsSubSelection)
		{
			T_REAL RealValue = OutAttribute->GetValueFromItemKey(PCGDefaultValueKey);
			SubSelection.SetVoid(RealType, &RealValue, WorkingType, Value);
			OutAttribute->SetDefaultValue(RealValue);
		}
		else if (RealType != WorkingType)
		{
			T_REAL RealValue{};
			if (WorkingToReal)
			{
				WorkingToReal(Value, &RealValue);
			}
			OutAttribute->SetDefaultValue(RealValue);
		}
		else
		{
			OutAttribute->SetDefaultValue(*static_cast<const T_REAL*>(Value));
		}
	}

	template <typename T_REAL>
	PCGExValueHash TDirectDataAttributeProxy<T_REAL>::ReadValueHash(const int32 Index) const
	{
		check(InAttribute);
		return PCGExTypes::ComputeHash(InAttribute->GetValueFromItemKey(PCGDefaultValueKey));
	}

	// Explicit instantiations
#define PCGEX_TPL(_TYPE, _NAME, ...) template class TDirectDataAttributeProxy<_TYPE>;
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL

#pragma endregion
} // namespace PCGExData

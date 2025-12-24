// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/


#include "Details/PCGExSettingsDetails.h"

#include "PCGExCoreMacros.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataHelpers.h"
#include "Data/PCGExPointIO.h"
#include "Types/PCGExTypes.h"

namespace PCGExDetails
{
	template <typename T>
	bool TSettingValueBuffer<T>::Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped, const bool bCaptureMinMax)
	{
		FPCGExContext* Context = InDataFacade->GetContext();
		if (!Context) { return false; }

		PCGEX_VALIDATE_NAME_C(Context, Name)

		Buffer = InDataFacade->GetBroadcaster<T>(Name, bSupportScoped, bCaptureMinMax);
		//Buffer = InDataFacade->GetReadable<T>(Name, PCGExData::EIOSide::In, bSupportScoped);

		if (!Buffer)
		{
			if (!this->bQuiet) { PCGEX_LOG_INVALID_ATTR_C(Context, Attribute, Name) }
			return false;
		}

		return true;
	}

	template <typename T>
	T TSettingValueBuffer<T>::Read(const int32 Index) { return Buffer->Read(Index); }

	template <typename T>
	T TSettingValueBuffer<T>::Min() { return Buffer->Min; }

	template <typename T>
	T TSettingValueBuffer<T>::Max() { return Buffer->Max; }

	template <typename T>
	uint32 TSettingValueBuffer<T>::ReadValueHash(const int32 Index) { return Buffer->ReadValueHash(Index); }

	template <typename T>
	bool TSettingValueSelector<T>::Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped, const bool bCaptureMinMax)
	{
		FPCGExContext* Context = InDataFacade->GetContext();
		if (!Context) { return false; }

		Buffer = InDataFacade->GetBroadcaster<T>(Selector, bSupportScoped && !bCaptureMinMax, bCaptureMinMax, this->bQuiet);
		if (!Buffer) { return false; }

		return true;
	}

	template <typename T>
	T TSettingValueSelector<T>::Read(const int32 Index) { return Buffer->Read(Index); }

	template <typename T>
	T TSettingValueSelector<T>::Min() { return Buffer->Min; }

	template <typename T>
	T TSettingValueSelector<T>::Max() { return Buffer->Max; }

	template <typename T>
	uint32 TSettingValueSelector<T>::ReadValueHash(const int32 Index) { return Buffer->ReadValueHash(Index); }

	template <typename T>
	bool TSettingValueConstant<T>::Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped, const bool bCaptureMinMax) { return true; }

	template <typename T>
	uint32 TSettingValueConstant<T>::ReadValueHash(const int32 Index) { return PCGExTypes::ComputeHash(Constant); }

	template <typename T>
	bool TSettingValueSelectorConstant<T>::Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped, const bool bCaptureMinMax)
	{
		FPCGExContext* Context = InDataFacade->GetContext();
		if (!Context) { return false; }

		if (!PCGExData::Helpers::TryReadDataValue(Context, InDataFacade->GetIn(), Selector, this->Constant, this->bQuiet)) { return false; }

		return true;
	}

	template <typename T>
	bool TSettingValueBufferConstant<T>::Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped, const bool bCaptureMinMax)
	{
		FPCGExContext* Context = InDataFacade->GetContext();
		if (!Context) { return false; }

		PCGEX_VALIDATE_NAME_C(Context, Name)

		if (!PCGExData::Helpers::TryReadDataValue(Context, InDataFacade->GetIn(), Name, this->Constant, this->bQuiet)) { return false; }

		return true;
	}

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const T InConstant)
	{
		TSharedPtr<TSettingValueConstant<T>> V = MakeShared<TSettingValueConstant<T>>(InConstant);
		return StaticCastSharedPtr<TSettingValue<T>>(V);
	}

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const T InConstant)
	{
		if (InInput == EPCGExInputValueType::Attribute)
		{
			if (PCGExMetaHelpers::IsDataDomainAttribute(InSelector))
			{
				TSharedPtr<TSettingValueSelectorConstant<T>> V = MakeShared<TSettingValueSelectorConstant<T>>(InSelector);
				return StaticCastSharedPtr<TSettingValue<T>>(V);
			}
			TSharedPtr<TSettingValueSelector<T>> V = MakeShared<TSettingValueSelector<T>>(InSelector);
			return StaticCastSharedPtr<TSettingValue<T>>(V);
		}

		return MakeSettingValue<T>(InConstant);
	}

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const EPCGExInputValueType InInput, const FName InName, const T InConstant)
	{
		if (InInput == EPCGExInputValueType::Attribute)
		{
			if (PCGExMetaHelpers::IsDataDomainAttribute(InName))
			{
				TSharedPtr<TSettingValueBufferConstant<T>> V = MakeShared<TSettingValueBufferConstant<T>>(InName);
				return StaticCastSharedPtr<TSettingValue<T>>(V);
			}
			TSharedPtr<TSettingValueBuffer<T>> V = MakeShared<TSettingValueBuffer<T>>(InName);
			return StaticCastSharedPtr<TSettingValue<T>>(V);
		}

		return MakeSettingValue<T>(InConstant);
	}

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType InInput, const FName InName, const T InConstant)
	{
		T Constant = InConstant;
		PCGExData::Helpers::TryGetSettingDataValue(InContext, InData, InInput, InName, InConstant, Constant);
		return MakeSettingValue<T>(Constant);
	}

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const T InConstant)
	{
		T Constant = InConstant;
		PCGExData::Helpers::TryGetSettingDataValue(InContext, InData, InInput, InSelector, InConstant, Constant);
		return MakeSettingValue<T>(Constant);
	}

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const TSharedPtr<PCGExData::FPointIO> InData, const EPCGExInputValueType InInput, const FName InName, const T InConstant)
	{
		return MakeSettingValue<T>(InData->GetContext(), InData->GetIn(), InInput, InName, InConstant);
	}

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const TSharedPtr<PCGExData::FPointIO> InData, const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const T InConstant)
	{
		return MakeSettingValue<T>(InData->GetContext(), InData->GetIn(), InInput, InSelector, InConstant);
	}

#pragma region externalization

#define PCGEX_TPL(_TYPE, _NAME, ...)\
template class PCGEXCORE_API TSettingValueBuffer<_TYPE>;\
template class PCGEXCORE_API TSettingValueSelector<_TYPE>;\
template class PCGEXCORE_API TSettingValueConstant<_TYPE>;\
template class PCGEXCORE_API TSettingValueSelectorConstant<_TYPE>;\
template class PCGEXCORE_API TSettingValueBufferConstant<_TYPE>;\
template PCGEXCORE_API TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const _TYPE InConstant); \
template PCGEXCORE_API TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const _TYPE InConstant); \
template PCGEXCORE_API TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const EPCGExInputValueType InInput, const FName InName, const _TYPE InConstant); \
template PCGEXCORE_API TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType InInput, const FName InName, const _TYPE InConstant); \
template PCGEXCORE_API TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const _TYPE InConstant); \
template PCGEXCORE_API TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const TSharedPtr<PCGExData::FPointIO> InData, const EPCGExInputValueType InInput, const FName InName, const _TYPE InConstant); \
template PCGEXCORE_API TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const TSharedPtr<PCGExData::FPointIO> InData, const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const _TYPE InConstant);

	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)

#undef PCGEX_TPL

#pragma endregion
}

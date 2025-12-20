// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExSettingsMacros.h" // Boilerplate dependency
#include "Helpers/PCGExMetaHelpers.h"
#include "Metadata/PCGAttributePropertySelector.h"

enum class EPCGExInputValueType : uint8;
struct FPCGExContext;

namespace PCGExData
{
	struct FProxyPoint;
	struct FConstPoint;
	class FFacade;
	class FPointIO;

	template <typename T>
	class TBuffer;
}

namespace PCGExDetails
{
#pragma region Settings

	template <typename T>
	class TSettingValue : public TSharedFromThis<TSettingValue<T>>
	{
	public:
		virtual ~TSettingValue() = default;
		virtual bool Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true, const bool bCaptureMinMax = false) = 0;
		FORCEINLINE virtual void SetConstant(T InConstant)
		{
		}

		bool bQuiet = false;

		FORCEINLINE virtual bool IsConstant() { return false; }
		FORCEINLINE virtual T Read(const int32 Index) = 0;
		FORCEINLINE virtual T Min() = 0;
		FORCEINLINE virtual T Max() = 0;
		FORCEINLINE virtual uint32 ReadValueHash(const int32 Index) = 0;
	};

	template <typename T>
	class TSettingValueBuffer final : public TSettingValue<T>
	{
	protected:
		TSharedPtr<PCGExData::TBuffer<T>> Buffer = nullptr;
		FName Name = NAME_None;

	public:
		explicit TSettingValueBuffer(const FName InName)
			: Name(InName)
		{
		}

		virtual bool Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true, const bool bCaptureMinMax = false) override;

		virtual T Read(const int32 Index) override;
		virtual T Min() override;
		virtual T Max() override;
		virtual uint32 ReadValueHash(const int32 Index) override;
	};

	template <typename T>
	class TSettingValueSelector final : public TSettingValue<T>
	{
	protected:
		TSharedPtr<PCGExData::TBuffer<T>> Buffer = nullptr;
		FPCGAttributePropertyInputSelector Selector;

	public:
		explicit TSettingValueSelector(const FPCGAttributePropertyInputSelector& InSelector)
			: Selector(InSelector)
		{
		}

		virtual bool Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true, const bool bCaptureMinMax = false) override;

		virtual T Read(const int32 Index) override;
		virtual T Min() override;
		virtual T Max() override;
		virtual uint32 ReadValueHash(const int32 Index) override;
	};

	template <typename T>
	class TSettingValueConstant : public TSettingValue<T>
	{
	protected:
		T Constant = T{};

	public:
		explicit TSettingValueConstant(const T InConstant)
			: Constant(InConstant)
		{
		}

		virtual bool Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true, const bool bCaptureMinMax = false) override;

		FORCEINLINE virtual bool IsConstant() override { return true; }
		FORCEINLINE virtual void SetConstant(T InConstant) override { Constant = InConstant; };

		FORCEINLINE virtual T Read(const int32 Index) override { return Constant; }
		FORCEINLINE virtual T Min() override { return Constant; }
		FORCEINLINE virtual T Max() override { return Constant; }
		virtual uint32 ReadValueHash(const int32 Index) override;
	};

	template <typename T>
	class TSettingValueSelectorConstant final : public TSettingValueConstant<T>
	{
	protected:
		FPCGAttributePropertyInputSelector Selector;

	public:
		explicit TSettingValueSelectorConstant(const FPCGAttributePropertyInputSelector& InSelector)
			: TSettingValueConstant<T>(T{}), Selector(InSelector)
		{
		}

		virtual bool Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true, const bool bCaptureMinMax = false) override;
	};

	template <typename T>
	class TSettingValueBufferConstant final : public TSettingValueConstant<T>
	{
	protected:
		FName Name = NAME_None;

	public:
		explicit TSettingValueBufferConstant(const FName InName)
			: TSettingValueConstant<T>(T{}), Name(InName)
		{
		}

		virtual bool Init(const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true, const bool bCaptureMinMax = false) override;
	};

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const T InConstant);

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const T InConstant);

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const EPCGExInputValueType InInput, const FName InName, const T InConstant);

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType InInput, const FName InName, const T InConstant);

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const T InConstant);

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const TSharedPtr<PCGExData::FPointIO> InData, const EPCGExInputValueType InInput, const FName InName, const T InConstant);

	template <typename T>
	TSharedPtr<TSettingValue<T>> MakeSettingValue(const TSharedPtr<PCGExData::FPointIO> InData, const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const T InConstant);

#pragma region externalization

#define PCGEX_TPL(_TYPE, _NAME, ...) \
extern template class TSettingValueBuffer<_TYPE>; \
extern template class TSettingValueSelector<_TYPE>; \
extern template class TSettingValueConstant<_TYPE>; \
extern template class TSettingValueSelectorConstant<_TYPE>; \
extern template class TSettingValueBufferConstant<_TYPE>; \
extern template TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const _TYPE InConstant); \
extern template TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const _TYPE InConstant); \
extern template TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const EPCGExInputValueType InInput, const FName InName, const _TYPE InConstant); \
extern template TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType InInput, const FName InName, const _TYPE InConstant); \
extern template TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(FPCGExContext* InContext, const UPCGData* InData, const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const _TYPE InConstant); \
extern template TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const TSharedPtr<PCGExData::FPointIO> InData, const EPCGExInputValueType InInput, const FName InName, const _TYPE InConstant); \
extern template TSharedPtr<TSettingValue<_TYPE>> MakeSettingValue(const TSharedPtr<PCGExData::FPointIO> InData, const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const _TYPE InConstant);

	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)

#undef PCGEX_TPL

#pragma endregion

#pragma endregion
}

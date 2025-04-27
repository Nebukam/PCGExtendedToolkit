// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#define PCGEX_SOFT_VALIDATE_NAME_DETAILS(_BOOL, _NAME, _CTX) if(_BOOL){if (!FPCGMetadataAttributeBase::IsValidName(_NAME) || _NAME.IsNone()){ PCGE_LOG_C(Warning, GraphAndLog, _CTX, FTEXT("Invalid user-defined attribute name for " #_NAME)); _BOOL = false; } }
#define PCGEX_SETTING_VALUE_GET(_NAME, _TYPE, _INPUT, _SOURCE, _CONSTANT) TSharedPtr<PCGExDetails::TSettingValue<_TYPE>> GetValueSetting##_NAME() const{ return PCGExDetails::MakeSettingValue<_TYPE>(_INPUT, _SOURCE, _CONSTANT); }
#include "CoreMinimal.h"
#include "PCGExMacros.h"
#include "Data/PCGExData.h"
#include "Data/PCGExDataTag.h"

#include "PCGExDetailsData.generated.h"

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExInfluenceDetails
{
	GENERATED_BODY()

	FPCGExInfluenceDetails()
	{
	}

	/** Type of Weight */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	EPCGExInputValueType InfluenceInput = EPCGExInputValueType::Constant;

	/** Fetch the size from a local attribute. The regular Size parameter then act as a scale.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable, DisplayName="Influence (Attr)", EditCondition="InfluenceInput!=EPCGExInputValueType::Constant", EditConditionHides))
	FPCGAttributePropertyInputSelector LocalInfluence;

	/** Draw size. What it means depends on the selected debug type. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable, DisplayName="Influence", EditCondition="InfluenceInput==EPCGExInputValueType::Constant", EditConditionHides, ClampMin=-1, ClampMax=1))
	double Influence = 1.0;

	/** If enabled, applies influence after each iteration; otherwise applies once at the end of the relaxing.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
	bool bProgressiveInfluence = true;

	TSharedPtr<PCGExData::TBuffer<double>> InfluenceBuffer;

	bool Init(const FPCGContext* InContext, const TSharedRef<PCGExData::FFacade>& InPointDataFacade);
	double GetInfluence(const int32 PointIndex) const;
};

USTRUCT(BlueprintType)
struct PCGEXTENDEDTOOLKIT_API FPCGExComponentTaggingDetails
{
	GENERATED_BODY()

	FPCGExComponentTaggingDetails()
	{
	}

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	bool bForwardInputDataTags = true;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	//bool bOutputTagsToAttributes = false;

	//UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta=(PCG_Overridable))
	//bool bAddTagsToData = false;
};

#undef PCGEX_SOFT_VALIDATE_NAME_DETAILS

namespace PCGExDetails
{
	template <typename T>
	class TSettingValue : public TSharedFromThis<TSettingValue<T>>
	{
	public:
		virtual ~TSettingValue() = default;
		virtual bool Init(const FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true) = 0;
		FORCEINLINE virtual T Read(const int32 Index) = 0;
	};

	template <typename T>
	class TSettingValueBuffer final : public TSettingValue<T>
	{
	protected:
		TSharedPtr<PCGExData::TBuffer<T>> Buffer = nullptr;
		FName Name = NAME_None;

	public:
		explicit TSettingValueBuffer(const FName InName):
			Name(InName)
		{
		}

		virtual bool Init(const FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true) override
		{
			if (bSupportScoped && InDataFacade->bSupportsScopedGet) { Buffer = InDataFacade->GetScopedReadable<T>(Name); }
			else { Buffer = InDataFacade->GetReadable<T>(Name); }

			if (!Buffer)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Attribute \"{0}\" is missing."), FText::FromName(Name)));
				return false;
			}

			return true;
		}

		FORCEINLINE virtual T Read(const int32 Index) override { return Buffer->Read(Index); }
	};

	template <typename T>
	class TSettingValueSelector final : public TSettingValue<T>
	{
	protected:
		TSharedPtr<PCGExData::TBuffer<T>> Buffer = nullptr;
		FPCGAttributePropertyInputSelector Selector;

	public:
		explicit TSettingValueSelector(const FPCGAttributePropertyInputSelector& InSelector):
			Selector(InSelector)
		{
		}

		virtual bool Init(const FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true) override
		{
			if (bSupportScoped && InDataFacade->bSupportsScopedGet) { Buffer = InDataFacade->GetScopedBroadcaster<T>(Selector); }
			else { Buffer = InDataFacade->GetBroadcaster<T>(Selector); }

			if (!Buffer)
			{
				PCGE_LOG_C(Error, GraphAndLog, InContext, FText::Format(FTEXT("Selector \"{0}\" is invalid."), FText::FromString(PCGEx::GetSelectorDisplayName(Selector))));
				return false;
			}


			return true;
		}

		FORCEINLINE virtual T Read(const int32 Index) override { return Buffer->Read(Index); }
	};

	template <typename T>
	class TSettingValueConstant final : public TSettingValue<T>
	{
	protected:
		T Constant = T{};

	public:
		explicit TSettingValueConstant(const T InConstant):
			Constant(InConstant)
		{
		}

		virtual bool Init(const FPCGExContext* InContext, const TSharedPtr<PCGExData::FFacade>& InDataFacade, const bool bSupportScoped = true) override { return true; }

		FORCEINLINE virtual T Read(const int32 Index) override { return Constant; }
	};

	template <typename T>
	static TSharedPtr<TSettingValue<T>> MakeSettingValue(const T InConstant)
	{
		TSharedPtr<TSettingValueConstant<T>> V = MakeShared<TSettingValueConstant<T>>(InConstant);
		return StaticCastSharedPtr<TSettingValue<T>>(V);
	}

	template <typename T>
	static TSharedPtr<TSettingValue<T>> MakeSettingValue(const EPCGExInputValueType InInput, const FPCGAttributePropertyInputSelector& InSelector, const T InConstant)
	{
		if (InInput == EPCGExInputValueType::Attribute)
		{
			TSharedPtr<TSettingValueSelector<T>> V = MakeShared<TSettingValueSelector<T>>(InSelector);
			return StaticCastSharedPtr<TSettingValue<T>>(V);
		}

		return MakeSettingValue<T>(InConstant);
	}

	template <typename T>
	static TSharedPtr<TSettingValue<T>> MakeSettingValue(const EPCGExInputValueType InInput, const FName InName, const T InConstant)
	{
		if (InInput == EPCGExInputValueType::Attribute)
		{
			TSharedPtr<TSettingValueBuffer<T>> V = MakeShared<TSettingValueBuffer<T>>(InName);
			return StaticCastSharedPtr<TSettingValue<T>>(V);
		}

		return MakeSettingValue<T>(InConstant);
	}
}

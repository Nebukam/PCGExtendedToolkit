// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExAxisDeformDetails.h"

#include "Core/PCGExContext.h"
#include "Data/PCGExData.h"
#include "Data/PCGExPointElements.h"
#include "Data/PCGExTaggedData.h"
#include "Details/PCGExSettingsDetails.h"
#include "Engine/EngineTypes.h"

FPCGExAxisDeformDetails::FPCGExAxisDeformDetails(const FString InFirst, const FString InSecond, const double InFirstValue, const double InSecondValue)
{
	FirstAlphaAttribute = FName(TEXT("@Data.") + InFirst);
	FirstAlphaConstant = InFirstValue;

	SecondAlphaAttribute = FName(TEXT("@Data.") + InSecond);
	SecondAlphaConstant = InSecondValue;
}

PCGEX_SETTING_DATA_VALUE_IMPL_BOOL(FPCGExAxisDeformDetails, FirstAlpha, double, FirstAlphaInput != EPCGExSampleSource::Constant, FirstAlphaAttribute, FirstAlphaConstant)
PCGEX_SETTING_VALUE_IMPL_BOOL(FPCGExAxisDeformDetails, FirstAlpha, double, FirstAlphaInput != EPCGExSampleSource::Constant, FirstAlphaAttribute, FirstAlphaConstant)

PCGEX_SETTING_DATA_VALUE_IMPL_BOOL(FPCGExAxisDeformDetails, SecondAlpha, double, SecondAlphaInput != EPCGExSampleSource::Constant, SecondAlphaAttribute, SecondAlphaConstant)
PCGEX_SETTING_VALUE_IMPL_BOOL(FPCGExAxisDeformDetails, SecondAlpha, double, SecondAlphaInput != EPCGExSampleSource::Constant, SecondAlphaAttribute, SecondAlphaConstant)

bool FPCGExAxisDeformDetails::Validate(FPCGExContext* InContext, const bool bSupportPoints) const
{
	if (FirstAlphaInput != EPCGExSampleSource::Constant)
	{
		PCGEX_VALIDATE_NAME_C(InContext, FirstAlphaAttribute)
		if (!bSupportPoints && !PCGExMetaHelpers::IsDataDomainAttribute(FirstAlphaAttribute))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Only @Data attributes are supported."));
			PCGEX_LOG_INVALID_ATTR_C(InContext, First Alpha, FirstAlphaAttribute)
			return false;
		}
	}

	if (SecondAlphaInput != EPCGExSampleSource::Constant)
	{
		PCGEX_VALIDATE_NAME_C(InContext, SecondAlphaAttribute)
		if (!bSupportPoints && !PCGExMetaHelpers::IsDataDomainAttribute(SecondAlphaAttribute))
		{
			PCGE_LOG_C(Error, GraphAndLog, InContext, FTEXT("Only @Data attributes are supported."));
			PCGEX_LOG_INVALID_ATTR_C(InContext, Second Alpha, SecondAlphaAttribute)
			return false;
		}
	}

	return true;
}

bool FPCGExAxisDeformDetails::Init(FPCGExContext* InContext, const TArray<FPCGExTaggedData>& InTargets)
{
	if (FirstAlphaInput == EPCGExSampleSource::Target)
	{
		TargetsFirstValueGetter.Init(nullptr, InTargets.Num());
		for (int i = 0; i < InTargets.Num(); i++) { TargetsFirstValueGetter[i] = GetValueSettingFirstAlpha(InContext, InTargets[i].Data); }
	}

	if (SecondAlphaInput == EPCGExSampleSource::Target)
	{
		TargetsSecondValueGetter.Init(nullptr, InTargets.Num());
		for (int i = 0; i < InTargets.Num(); i++) { TargetsSecondValueGetter[i] = GetValueSettingSecondAlpha(InContext, InTargets[i].Data); }
	}

	return true;
}

bool FPCGExAxisDeformDetails::Init(FPCGExContext* InContext, const FPCGExAxisDeformDetails& Parent, const TSharedRef<PCGExData::FFacade>& InDataFacade, const int32 InTargetIndex, const bool bSupportPoint)
{
	if (Parent.FirstAlphaInput == EPCGExSampleSource::Target && InTargetIndex != -1)
	{
		FirstValueGetter = Parent.TargetsFirstValueGetter[InTargetIndex];
	}
	else
	{
		if (Parent.FirstValueGetter)
		{
			FirstValueGetter = Parent.FirstValueGetter;
		}
		else
		{
			if (bSupportPoint)
			{
				FirstValueGetter = Parent.GetValueSettingFirstAlpha();
				if (!FirstValueGetter->Init(InDataFacade)) { return false; }
			}
			else
			{
				FirstValueGetter = Parent.GetValueSettingFirstAlpha(InContext, InDataFacade->GetIn());
			}
		}
	}

	if (Parent.SecondAlphaInput == EPCGExSampleSource::Target && InTargetIndex != -1)
	{
		SecondValueGetter = Parent.TargetsSecondValueGetter[InTargetIndex];
	}
	else
	{
		if (Parent.SecondValueGetter)
		{
			SecondValueGetter = Parent.SecondValueGetter;
		}
		else
		{
			if (bSupportPoint)
			{
				SecondValueGetter = Parent.GetValueSettingSecondAlpha();
				if (!SecondValueGetter->Init(InDataFacade)) { return false; }
			}
			else
			{
				SecondValueGetter = Parent.GetValueSettingSecondAlpha(InContext, InDataFacade->GetIn());
			}
		}
	}

	return true;
}

void FPCGExAxisDeformDetails::GetAlphas(const int32 Index, double& OutFirst, double& OutSecond, const bool bSort) const
{
	OutFirst = FirstValueGetter->Read(Index);
	OutSecond = SecondValueGetter->Read(Index);

	if (Usage == EPCGExTransformAlphaUsage::CenterAndSize)
	{
		const double Center = OutFirst;
		OutFirst -= OutSecond;
		OutSecond += Center;
	}
	else if (Usage == EPCGExTransformAlphaUsage::StartAndSize)
	{
		OutSecond += OutFirst;
	}

	if (bSort && OutFirst > OutSecond) { std::swap(OutFirst, OutSecond); }
}

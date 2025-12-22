// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExCoreSettingsCache.h"

#include "CoreMinimal.h"
#include "PCGPin.h"

#if WITH_EDITOR
FLinearColor FPCGExCoreSettingsCache::GetColor(const FName InColor) const
{
	const FLinearColor* Col = ColorsMap.Find(InColor);
	return Col ? *Col : FLinearColor::White;
}

FLinearColor FPCGExCoreSettingsCache::GetOptInColor(const FName InColor) const
{
	if (bUseNativeColorsIfPossible) { return FLinearColor::White; }
	const FLinearColor* Col = ColorsMap.Find(InColor);
	return Col ? *Col : FLinearColor::White;
}

FLinearColor FPCGExCoreSettingsCache::GetOptInColor(const FLinearColor InColor) const
{
	return bUseNativeColorsIfPossible ? FLinearColor::White : InColor;
}

bool FPCGExCoreSettingsCache::GetPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip, bool bIsOutPin) const
{
	const FName PinLabel = InPin->Properties.Label;

	if (bIsOutPin)
	{
		if (const int32* Index = OutPinInfosMap.Find(PinLabel))
		{
			const FPinInfos& Infos = OutPinInfos[*Index];
			OutExtraIcon = Infos.Icon;
			OutTooltip = Infos.Tooltip;
			return true;
		}
	}
	else
	{
		if (const int32* Index = InPinInfosMap.Find(PinLabel))
		{
			const FPinInfos& Infos = InPinInfos[*Index];
			OutExtraIcon = Infos.Icon;
			OutTooltip = Infos.Tooltip;
			return true;
		}
	}

	return false;
}
#endif

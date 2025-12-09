// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExGlobalSettings.h"

#include "CoreMinimal.h"
#include "PCGPin.h"

// Define static members
TArray<PCGEx::FPinInfos> UPCGExGlobalSettings::InPinInfos;
TArray<PCGEx::FPinInfos> UPCGExGlobalSettings::OutPinInfos;
TMap<FName, int32> UPCGExGlobalSettings::InPinInfosMap;
TMap<FName, int32> UPCGExGlobalSettings::OutPinInfosMap;
bool UPCGExGlobalSettings::bGeneratedPinMap = false; // Initialize to a default value

FLinearColor UPCGExGlobalSettings::WantsColor(const FLinearColor InColor) const
{
	return bUseNativeColorsIfPossible ? FLinearColor::White : InColor;
}

bool UPCGExGlobalSettings::GetPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip, bool bIsOutPin) const
{
#if WITH_EDITOR

	if (!bGeneratedPinMap)
	{
		UPCGExGlobalSettings* This = const_cast<UPCGExGlobalSettings*>(this);
		This->GeneratePinInfos();
	}

	const FName PinLabel = InPin->Properties.Label;

	if (bIsOutPin)
	{
		if (const int32* Index = OutPinInfosMap.Find(PinLabel))
		{
			const PCGEx::FPinInfos& Infos = OutPinInfos[*Index];
			OutExtraIcon = Infos.Icon;
			OutTooltip = Infos.Tooltip;
			return true;
		}
	}
	else
	{
		if (const int32* Index = InPinInfosMap.Find(PinLabel))
		{
			const PCGEx::FPinInfos& Infos = InPinInfos[*Index];
			OutExtraIcon = Infos.Icon;
			OutTooltip = Infos.Tooltip;
			return true;
		}
	}

#endif

	return false;
}

void UPCGExGlobalSettings::GeneratePinInfos()
{
#if WITH_EDITOR

	if (bGeneratedPinMap) { return; }
	bGeneratedPinMap = true;

	// Is this shit? Yes.
	// Are icons cool? Yes.

	int32 PinIndex = -1;

#pragma region OUT

#define PCGEX_EMPLACE_PIN_OUT(_ID, _TOOLTIP) PinIndex = OutPinInfos.Emplace(FName("PCGEx.Pin." # _ID), TEXT(_TOOLTIP))
#define PCGEX_MAP_PIN_OUT(_ID) OutPinInfosMap.Add(FName(TEXT(_ID)), PinIndex);

	PCGEX_EMPLACE_PIN_OUT(OUT_Vtx, "Point collection formatted for use as cluster vtx.");
	PCGEX_MAP_PIN_OUT("Vtx")
	PCGEX_MAP_PIN_OUT("Unmatched Vtx")

	PCGEX_EMPLACE_PIN_OUT(OUT_Edges, "Point collection formatted for use as cluster edges.");
	PCGEX_MAP_PIN_OUT("Edges")
	PCGEX_MAP_PIN_OUT("Unmatched Edges")

#undef PCGEX_EMPLACE_PIN_OUT
#undef PCGEX_MAP_PIN_OUT
#pragma endregion

#pragma region IN

#define PCGEX_EMPLACE_PIN_IN(_ID, _TOOLTIP) PinIndex = InPinInfos.Emplace(FName("PCGEx.Pin." # _ID), TEXT(_TOOLTIP ", supports multiple inputs."))
#define PCGEX_MAP_PIN_IN(_ID) InPinInfosMap.Add(FName(TEXT(_ID)), PinIndex);

	PCGEX_EMPLACE_PIN_IN(IN_Vtx, "Point collection formatted for use as cluster vtx.");
	PCGEX_MAP_PIN_IN("Vtx")

	PCGEX_EMPLACE_PIN_IN(IN_Edges, "Point collection formatted for use as cluster edges.");
	PCGEX_MAP_PIN_IN("Edges")

	PCGEX_EMPLACE_PIN_IN(IN_Special, "Attribute set whose values will be used to override a specific internal module.");
	PCGEX_MAP_PIN_IN("Overrides : Blending")
	PCGEX_MAP_PIN_IN("Overrides : Refinement")
	PCGEX_MAP_PIN_IN("Overrides : Graph Builder")
	PCGEX_MAP_PIN_IN("Overrides : Tangents")
	PCGEX_MAP_PIN_IN("Overrides : Start Tangents")
	PCGEX_MAP_PIN_IN("Overrides : End Tangents")
	PCGEX_MAP_PIN_IN("Overrides : Goal Picker")
	PCGEX_MAP_PIN_IN("Overrides : Search")
	PCGEX_MAP_PIN_IN("Overrides : Orient")
	PCGEX_MAP_PIN_IN("Overrides : Smoothing")
	PCGEX_MAP_PIN_IN("Overrides : Packer")

#undef PCGEX_EMPLACE_PIN_IN
#undef PCGEX_MAP_PIN_IN
#pragma endregion

#endif
}

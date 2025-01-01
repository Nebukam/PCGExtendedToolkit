// Copyright 2024 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExGlobalSettings.h"

#include "CoreMinimal.h"
#include "PCGPin.h"
#include "PCGExMacros.h"

// Define static members
TArray<PCGEx::FPinInfos> UPCGExGlobalSettings::InPinInfos;
TArray<PCGEx::FPinInfos> UPCGExGlobalSettings::OutPinInfos;
TMap<FName, int32> UPCGExGlobalSettings::InPinInfosMap;
TMap<FName, int32> UPCGExGlobalSettings::OutPinInfosMap;
bool UPCGExGlobalSettings::bGeneratedPinMap = false; // Initialize to a default value

bool UPCGExGlobalSettings::GetPinExtraIcon(const UPCGPin* InPin, FName& OutExtraIcon, FText& OutTooltip, bool bIsOutPin) const
{
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

	return false;
}

void UPCGExGlobalSettings::GeneratePinInfos()
{
	if (bGeneratedPinMap) { return; }
	bGeneratedPinMap = true;

	// Is this shit? Yes.
	// Are icons cool? Yes.

#pragma region OUT

#define PCGEX_EMPLACE_PIN_OUT(_ID, _TOOLTIP) PinIndex = OutPinInfos.Emplace(FName("PCGEx.Pin." # _ID), TEXT(_TOOLTIP))
#define PCGEX_MAP_PIN_OUT(_ID) OutPinInfosMap.Add(FName(TEXT(_ID)), PinIndex);

	int32 PinIndex = -1;
	// Out pin map
	PCGEX_EMPLACE_PIN_OUT(OUT_Filter, "PCGEx Filter");
	PCGEX_MAP_PIN_OUT("Filter")

	PCGEX_EMPLACE_PIN_OUT(OUT_FilterEdges, "PCGEx Edge Filter");
	PCGEX_MAP_PIN_OUT("EdgeFilter")

	PCGEX_EMPLACE_PIN_OUT(OUT_FilterNode, "PCGEx Vtx Filter");
	PCGEX_MAP_PIN_OUT("NodeFilter")

	PCGEX_EMPLACE_PIN_OUT(OUT_NodeFlag, "PCGEx Vtx Node Flag");
	PCGEX_MAP_PIN_OUT("Flag")

	PCGEX_EMPLACE_PIN_OUT(OUT_Heuristic, "PCGEx Heuristic");
	PCGEX_MAP_PIN_OUT("Heuristics")

	PCGEX_EMPLACE_PIN_OUT(OUT_Probe, "PCGEx Probe");
	PCGEX_MAP_PIN_OUT("Probe")

	PCGEX_EMPLACE_PIN_OUT(OUT_Sorting, "PCGEx Sort Rule");
	PCGEX_MAP_PIN_OUT("SortRule")

	PCGEX_EMPLACE_PIN_OUT(OUT_Tex, "PCGEx Texture Params");
	PCGEX_MAP_PIN_OUT("TextureParam")

	PCGEX_EMPLACE_PIN_OUT(OUT_Partition, "PCGEx Partition Rule");
	PCGEX_MAP_PIN_OUT("PartitionRule")

	PCGEX_EMPLACE_PIN_OUT(OUT_VtxProperty, "PCGEx Vtx Property");
	PCGEX_MAP_PIN_OUT("Property")

	PCGEX_EMPLACE_PIN_OUT(OUT_Action, "PCGEx Action");
	PCGEX_MAP_PIN_OUT("Action")

	PCGEX_EMPLACE_PIN_OUT(OUT_Blend, "PCGEx Blending");
	PCGEX_MAP_PIN_OUT("Blending")
	
	PCGEX_EMPLACE_PIN_OUT(OUT_Shape, "PCGEx Shape Builder");
	PCGEX_MAP_PIN_OUT("Shape Builder")

#undef PCGEX_EMPLACE_PIN_OUT
#undef PCGEX_MAP_PIN_OUT
#pragma endregion

#pragma region IN

#define PCGEX_EMPLACE_PIN_IN(_ID, _TOOLTIP) PinIndex = InPinInfos.Emplace(FName("PCGEx.Pin." # _ID), TEXT(_TOOLTIP ", supports multiple inputs."))
#define PCGEX_MAP_PIN_IN(_ID) InPinInfosMap.Add(FName(TEXT(_ID)), PinIndex);

	// In pin map
	PCGEX_EMPLACE_PIN_IN(IN_Filters, "Expects PCGEx Filters, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Filters")
	PCGEX_MAP_PIN_IN("Point Filters")
	PCGEX_MAP_PIN_IN("Break Conditions")
	PCGEX_MAP_PIN_IN("Conditions Filters")
	PCGEX_MAP_PIN_IN("Keep Conditions")
	PCGEX_MAP_PIN_IN("Skip Conditions")
	PCGEX_MAP_PIN_IN("Generator Filters")
	PCGEX_MAP_PIN_IN("Connectable Filters")
	PCGEX_MAP_PIN_IN("Can Be Cut Conditions")
	PCGEX_MAP_PIN_IN("Can Cut Conditions")
	PCGEX_MAP_PIN_IN("Bevel Conditions")
	PCGEX_MAP_PIN_IN("Trigger Conditions")
	PCGEX_MAP_PIN_IN("Shift Conditions")
	PCGEX_MAP_PIN_IN("Split Conditions")
	PCGEX_MAP_PIN_IN("Stop Conditions")
	PCGEX_MAP_PIN_IN("Conditions")
	PCGEX_MAP_PIN_IN("Flip Orientation Conditions")

	PCGEX_EMPLACE_PIN_IN(OUT_FilterEdges, "Expects PCGEx Filers or Edge Filters, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Edge Filters")
	PCGEX_MAP_PIN_IN("EdgeFilters")

	PCGEX_EMPLACE_PIN_IN(OUT_FilterNode, "Expects PCGEx Filters or Vtx Filter, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Vtx Filters")
	PCGEX_MAP_PIN_IN("NodeFilters")

	PCGEX_EMPLACE_PIN_IN(OUT_NodeFlag, "Expects PCGEx Vtx Node Flags, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Flags")
	PCGEX_MAP_PIN_IN("NodeFlags")

	PCGEX_EMPLACE_PIN_IN(IN_Heuristics, "Expects PCGEx Heuristics, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Heuristics")

	PCGEX_EMPLACE_PIN_IN(IN_Probes, "Expects PCGEx Probes, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Probes")

	PCGEX_EMPLACE_PIN_IN(IN_Sortings, "Expects PCGEx Sort Rules, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("SortingRules")
	PCGEX_MAP_PIN_IN("SortRule")
	PCGEX_MAP_PIN_IN("SortRules")

	PCGEX_EMPLACE_PIN_IN(IN_Tex, "Expects PCGEx Texture Params, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("TextureParams")

	PCGEX_EMPLACE_PIN_IN(IN_Partitions, "Expects PCGEx Partition Rules, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("PartitionRules")

	PCGEX_EMPLACE_PIN_IN(OUT_VtxProperty, "Expects PCGEx Vtx Properties, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Properties")

	PCGEX_EMPLACE_PIN_IN(OUT_Action, "Expects PCGEx Actions, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Actions")

	PCGEX_EMPLACE_PIN_IN(OUT_Blend, "Expects PCGEx Blendings, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Blendings")
	
	PCGEX_EMPLACE_PIN_IN(OUT_Shape, "Expects PCGEx Shape Builders, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Shape Builders")
	
	PinIndex = InPinInfos.Emplace(FName("PCGEx.Pin.IN_Vtx"), TEXT("Point collection formatted for use as cluster vtx."));
	PCGEX_MAP_PIN_IN("Vtx")

	PinIndex = InPinInfos.Emplace(FName("PCGEx.Pin.OUT_Edges"), TEXT("Point collection formatted for use as cluster edges."));
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
}

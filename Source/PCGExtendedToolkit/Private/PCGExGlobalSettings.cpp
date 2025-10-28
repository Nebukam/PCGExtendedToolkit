﻿// Copyright 2025 Timothé Lapetite and contributors
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

	// Out pin map
	PCGEX_EMPLACE_PIN_OUT(OUT_Filter, "PCGEx Filter");
	PCGEX_MAP_PIN_OUT("Filter")

	PCGEX_EMPLACE_PIN_OUT(OUT_FilterCollection, "PCGEx Collection Filter");
	PCGEX_MAP_PIN_OUT("C-Filter")

	PCGEX_EMPLACE_PIN_OUT(OUT_FilterEdge, "PCGEx Edge Filter");
	PCGEX_MAP_PIN_OUT("Edge Filter")

	PCGEX_EMPLACE_PIN_OUT(OUT_FilterVtx, "PCGEx Vtx Filter");
	PCGEX_MAP_PIN_OUT("Node Filter")

	PCGEX_EMPLACE_PIN_OUT(OUT_ClusterState, "PCGEx Vtx Node Flag");
	PCGEX_MAP_PIN_OUT("Flag")

	PCGEX_EMPLACE_PIN_OUT(OUT_Heuristics, "PCGEx Heuristic");
	PCGEX_MAP_PIN_OUT("Heuristics")

	PCGEX_EMPLACE_PIN_OUT(OUT_Probe, "PCGEx Probe");
	PCGEX_MAP_PIN_OUT("Probe")

	PCGEX_EMPLACE_PIN_OUT(OUT_SortRule, "PCGEx Sort Rule");
	PCGEX_MAP_PIN_OUT("SortRule")
	PCGEX_MAP_PIN_OUT("SortingRule")

	PCGEX_EMPLACE_PIN_OUT(OUT_TexParam, "PCGEx Texture Params");
	PCGEX_MAP_PIN_OUT("TextureParam")

	PCGEX_EMPLACE_PIN_OUT(OUT_PartitionRule, "PCGEx Partition Rule");
	PCGEX_MAP_PIN_OUT("PartitionRule")

	PCGEX_EMPLACE_PIN_OUT(OUT_VtxProperty, "PCGEx Vtx Property");
	PCGEX_MAP_PIN_OUT("Property")

	PCGEX_EMPLACE_PIN_OUT(OUT_Action, "PCGEx Action");
	PCGEX_MAP_PIN_OUT("Action")

	PCGEX_EMPLACE_PIN_OUT(OUT_BlendOp, "PCGEx Blending");
	PCGEX_MAP_PIN_OUT("Blend Op")

	PCGEX_EMPLACE_PIN_OUT(OUT_Shape, "PCGEx Shape Builder");
	PCGEX_MAP_PIN_OUT("Shape Builder")

	PCGEX_EMPLACE_PIN_OUT(OUT_Tensor, "PCGEx Tensor");
	PCGEX_MAP_PIN_OUT("Tensor")

	PCGEX_EMPLACE_PIN_OUT(OUT_Picker, "PCGEx Picker");
	PCGEX_MAP_PIN_OUT("Picker")

	PCGEX_EMPLACE_PIN_OUT(OUT_FillControl, "PCGEx Fill Control");
	PCGEX_MAP_PIN_OUT("Fill Control")

	PCGEX_EMPLACE_PIN_OUT(OUT_MatchRule, "PCGEx Data Matching Rule");
	PCGEX_MAP_PIN_OUT("Match Rule")

	PCGEX_EMPLACE_PIN_OUT(OUT_Vtx, "Point collection formatted for use as cluster vtx.");
	PCGEX_MAP_PIN_OUT("Vtx")

	PCGEX_EMPLACE_PIN_OUT(OUT_Edges, "Point collection formatted for use as cluster edges.");
	PCGEX_MAP_PIN_OUT("Edges")

#undef PCGEX_EMPLACE_PIN_OUT
#undef PCGEX_MAP_PIN_OUT
#pragma endregion

#pragma region IN

#define PCGEX_EMPLACE_PIN_IN(_ID, _TOOLTIP) PinIndex = InPinInfos.Emplace(FName("PCGEx.Pin." # _ID), TEXT(_TOOLTIP ", supports multiple inputs."))
#define PCGEX_MAP_PIN_IN(_ID) InPinInfosMap.Add(FName(TEXT(_ID)), PinIndex);

	// In pin map
	PCGEX_EMPLACE_PIN_IN(IN_Filter, "Expects PCGEx Filters, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Filters")
	PCGEX_MAP_PIN_IN("Point Filters")
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
	PCGEX_MAP_PIN_IN("Toggle Conditions")
	PCGEX_MAP_PIN_IN("Start Conditions")
	PCGEX_MAP_PIN_IN("Stop Conditions")
	PCGEX_MAP_PIN_IN("Pin Conditions")
	PCGEX_MAP_PIN_IN("Conditions")
	PCGEX_MAP_PIN_IN("Flip Conditions")
	PCGEX_MAP_PIN_IN("Tracker Filters")

	// Ahem.
	for (int f = 0; f < 42; f++)
	{
		FString SI = FString::Printf(TEXT("%d"), f + 0);
		InPinInfosMap.Add(FName(TEXT("→ ") + SI), PinIndex);
	}

	PCGEX_EMPLACE_PIN_IN(IN_FilterEdge, "Expects PCGEx Filers or Edge Filters, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Edge Filters")
	PCGEX_MAP_PIN_IN("EdgeFilters")

	PCGEX_EMPLACE_PIN_IN(IN_FilterVtx, "Expects PCGEx Filters or Vtx Filter, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Vtx Filters")
	PCGEX_MAP_PIN_IN("VtxFilters")
	PCGEX_MAP_PIN_IN("NodeFilters")
	PCGEX_MAP_PIN_IN("Break Conditions")

	PCGEX_EMPLACE_PIN_IN(IN_ClusterState, "Expects PCGEx Vtx Node Flags, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Flags")
	PCGEX_MAP_PIN_IN("NodeFlags")

	PCGEX_EMPLACE_PIN_IN(IN_Heuristics, "Expects PCGEx Heuristics, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Heuristics")

	PCGEX_EMPLACE_PIN_IN(IN_Probe, "Expects PCGEx Probes, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Probes")

	PCGEX_EMPLACE_PIN_IN(IN_SortRule, "Expects PCGEx Sort Rules, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("SortingRules")
	PCGEX_MAP_PIN_IN("SortRule")
	PCGEX_MAP_PIN_IN("SortRules")
	PCGEX_MAP_PIN_IN("Direction Sorting")

	PCGEX_EMPLACE_PIN_IN(IN_TexParam, "Expects PCGEx Texture Params, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("TextureParams")

	PCGEX_EMPLACE_PIN_IN(IN_PartitionRule, "Expects PCGEx Partition Rules, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("PartitionRules")

	PCGEX_EMPLACE_PIN_IN(IN_VtxProperty, "Expects PCGEx Vtx Properties, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Properties")

	PCGEX_EMPLACE_PIN_IN(IN_Action, "Expects PCGEx Actions, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Actions")

	PCGEX_EMPLACE_PIN_IN(IN_BlendOp, "Expects PCGEx Blendings, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Blend Ops")

	PCGEX_EMPLACE_PIN_IN(OUT_Shape, "Expects PCGEx Shape Builders, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Shape Builders")

	PCGEX_EMPLACE_PIN_IN(IN_Tensor, "Expects PCGEx Tensors, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Tensors")
	PCGEX_MAP_PIN_IN("Parent Tensor")

	PCGEX_EMPLACE_PIN_IN(IN_Picker, "PCGEx Pickers, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Pickers")

	PCGEX_EMPLACE_PIN_IN(IN_FillControl, "PCGEx Fill Controls, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Fill Controls")

	PCGEX_EMPLACE_PIN_IN(IN_MatchRule, "PCGEx Data Match Rules, supports multiple inputs.");
	PCGEX_MAP_PIN_IN("Match Rules")
	PCGEX_MAP_PIN_IN("Match Rules (Edges)")

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

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Shapes/PCGExShapes.h"

#include "Details/PCGExDetailsSettings.h"

TSharedPtr<PCGExDetails::TSettingValue<double>> FPCGExShapeConfigBase::GetValueSettingResolution(const bool bQuietErrors) const
{ TSharedPtr<PCGExDetails::TSettingValue<double>> V = PCGExDetails::MakeSettingValue<double>(ResolutionInput, ResolutionAttribute, ResolutionConstant); V->bQuietErrors = bQuietErrors; return V; }

TSharedPtr<PCGExDetails::TSettingValue<FVector>> FPCGExShapeConfigBase::GetValueSettingResolutionVector(const bool bQuietErrors) const
{ TSharedPtr<PCGExDetails::TSettingValue<FVector>> V = PCGExDetails::MakeSettingValue<FVector>(ResolutionInput, ResolutionAttribute, ResolutionConstantVector); V->bQuietErrors = bQuietErrors; return V; }

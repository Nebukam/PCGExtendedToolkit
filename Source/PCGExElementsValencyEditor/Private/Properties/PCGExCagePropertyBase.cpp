// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Properties/PCGExCagePropertyBase.h"

UPCGExCagePropertyBase::UPCGExCagePropertyBase()
{
	PrimaryComponentTick.bCanEverTick = false;
	bWantsInitializeComponent = false;
}

FName UPCGExCagePropertyBase::GetEffectivePropertyName() const
{
	// If user specified a name, use it; otherwise default to component's name
	return PropertyName.IsNone() ? GetFName() : PropertyName;
}

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExSocketOutputDetails.h"

#include "PCGElement.h"
#include "Core/PCGExContext.h"
#include "Helpers/PCGExMetaHelpers.h"
#include "Sampling/PCGExSamplingCommon.h"

bool FPCGExSocketOutputDetails::Init(FPCGExContext* InContext)
{
	PCGEX_VALIDATE_NAME_C(InContext, SocketNameAttributeName)
	PCGEX_VALIDATE_NAME_C(InContext, SocketTagAttributeName)
	PCGEX_VALIDATE_NAME_C(InContext, CategoryAttributeName)
	PCGEX_VALIDATE_NAME_C(InContext, AssetPathAttributeName)
	
	SocketTagFilters.Init();
	SocketNameFilters.Init();
	CarryOverDetails.Init();

#define PCGEX_REGISTER_FLAG(_COMPONENT, _ARRAY) \
if ((_COMPONENT & static_cast<uint8>(EPCGExApplySampledComponentFlags::X)) != 0){ _ARRAY.Add(0); } \
if ((_COMPONENT & static_cast<uint8>(EPCGExApplySampledComponentFlags::Y)) != 0){ _ARRAY.Add(1); } \
if ((_COMPONENT & static_cast<uint8>(EPCGExApplySampledComponentFlags::Z)) != 0){ _ARRAY.Add(2); }

	PCGEX_REGISTER_FLAG(TransformScale, TrScaComponents)

#undef PCGEX_REGISTER_FLAG

	return true;
}

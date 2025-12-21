// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExSocketOutputDetails.h"

#include "PCGElement.h"
#include "Core/PCGExContext.h"
#include "Helpers/PCGExMetaHelpers.h"

bool FPCGExSocketOutputDetails::Init(FPCGExContext* InContext) const
{
	PCGEX_VALIDATE_NAME_C(InContext, SocketNameAttributeName)
	PCGEX_VALIDATE_NAME_C(InContext, SocketTagAttributeName)
	PCGEX_VALIDATE_NAME_C(InContext, CategoryAttributeName)
	PCGEX_VALIDATE_NAME_C(InContext, AssetPathAttributeName)
	return true;
}

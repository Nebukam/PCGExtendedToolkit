// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

// Shared PCH - use only on modules with 25+ cpp files
//
// If you get PCH memory exhaustion errors (C3859/C1076), add to PCGExCore.Build.cs:
//   PublicDefinitions.Add("PCGEX_FAT_PCH=0");

#pragma once

#include "PCGExSharedPCH.h"

// Fat PCH: enabled by default, disable on systems with memory constraints
#ifndef PCGEX_FAT_PCH
#define PCGEX_FAT_PCH 1
#endif

#if PCGEX_FAT_PCH

// Foundational
#include "PCGExCommon.h"
#include "Core/PCGExContext.h"
#include "Containers/PCGExManagedObjects.h"
#include "Data/PCGExDataTags.h"

// Heavy hitter (289 includes across codebase)
#include "Data/PCGExData.h"
#include "Data/PCGExPointIO.h"

#endif // PCGEX_FAT_PCH

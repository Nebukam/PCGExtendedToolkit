// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "PCGExPropertyCompiled.h"

/**
 * Valency uses the shared property system from PCGExProperties module.
 *
 * All cage properties are stored as FPCGExPropertyCompiled derivatives.
 * Use PCGExProperties:: namespace for query helpers (GetProperty, GetAllProperties, etc.)
 *
 * For adding new property types, see PCGExPropertyTypes.h in PCGExProperties module.
 * For component-based authoring, see PCGExCagePropertyTypes.h in PCGExElementsValencyEditor.
 */

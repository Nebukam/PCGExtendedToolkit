// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

// Shared PCH - precompile common UE and PCG headers once for all modules
// PCG headers MUST be here (not in individual headers) to avoid PCH memory overflow

#pragma once

// Core UE
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

// PCG core - precompiled here so modules don't re-process them
#include "Metadata/PCGMetadataCommon.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "PCGCommon.h"

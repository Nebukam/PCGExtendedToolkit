// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

// Ultra-minimal shared PCH - ONLY core UE headers
// PCG headers are excluded to keep PCH size within MSVC limits
// Each module includes PCG headers as needed in their source files

#pragma once

// Core UE only - these are the biggest compilation time savers
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

// NOTE: PCG headers intentionally excluded to reduce PCH memory footprint
// If you need PCG types, include them directly in your .cpp files

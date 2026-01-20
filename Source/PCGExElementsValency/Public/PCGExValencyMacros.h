// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"

// Redraw viewports to show updated connections
#if WITH_EDITOR
#define PCGEX_VALENCY_REDRAW_ALL_VIEWPORT if (GEditor){ GEditor->RedrawAllViewports(); }
#else
#define PCGEX_VALENCY_REDRAW_ALL_VIEWPORT
#endif

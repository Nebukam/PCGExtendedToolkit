// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Types/PCGExTypeOps.h"

#include "Helpers/PCGExMetaHelpers.h"

namespace PCGExTypeOps
{
#define PCGEX_TPL(_TYPE, _NAME, ...)template PCGEXCORE_API const ITypeOpsBase* FTypeOpsRegistry::Get<_TYPE>();
	PCGEX_FOREACH_SUPPORTEDTYPES(PCGEX_TPL)
#undef PCGEX_TPL
}

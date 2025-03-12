// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Data/PCGExProxyData.h"

namespace PCGExData
{
	void FProxyDescriptor::UpdateSubSelection()
	{
		SubSelection = PCGEx::FSubSelection(Selector);
	}
}

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Helpers/PCGExBufferHelper.h"

namespace PCGExData
{
	IBufferHelper::IBufferHelper(const TSharedRef<FFacade>& InDataFacade)
		: DataFacade(InDataFacade)
	{
	}
}

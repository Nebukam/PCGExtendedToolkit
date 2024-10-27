// Copyright Timothé Lapetite 2024
// Released under the MIT license https://opensource.org/license/MIT/

#include "Paths/PCGExPaths.h"

#define LOCTEXT_NAMESPACE "PCGExPaths"
#define PCGEX_NAMESPACE PCGExPaths

void FPCGExPathFilterSettings::RegisterBuffersDependencies(FPCGExContext* InContext, PCGExData::FFacadePreloader& FacadePreloader) const
{
}

bool FPCGExPathFilterSettings::Init(FPCGExContext* InContext)
{
	return true;
}


#undef LOCTEXT_NAMESPACE
#undef PCGEX_NAMESPACE

// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#include "PCGExElementsClipper2.h"

#define LOCTEXT_NAMESPACE "FPCGExElementsClipper2Module"

#undef LOCTEXT_NAMESPACE


void FPCGExElementsClipper2Module::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsClipper2Module::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

PCGEX_IMPLEMENT_MODULE(FPCGExElementsClipper2Module, PCGExElementsClipper2)

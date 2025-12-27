// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/
// Originally ported from cavalier_contours by jbuckmccready (https://github.com/jbuckmccready/cavalier_contours)

#include "PCGExElementsCavalierContours.h"

#define LOCTEXT_NAMESPACE "FPCGExElementsCavalierContoursModule"

#undef LOCTEXT_NAMESPACE


void FPCGExElementsCavalierContoursModule::StartupModule()
{
	IPCGExModuleInterface::StartupModule();
}

void FPCGExElementsCavalierContoursModule::ShutdownModule()
{
	IPCGExModuleInterface::ShutdownModule();
}

PCGEX_IMPLEMENT_MODULE(FPCGExElementsCavalierContoursModule, PCGExElementsCavalierContours)

// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "PCGExtendedToolkitTest.h"

#define LOCTEXT_NAMESPACE "FPCGExtendedToolkitTestModule"

void FPCGExtendedToolkitTestModule::StartupModule()
{
	// Test module loaded - tests are auto-discovered by Unreal's automation framework
	UE_LOG(LogTemp, Log, TEXT("PCGExtendedToolkitTest module loaded"));
}

void FPCGExtendedToolkitTestModule::ShutdownModule()
{
	// Module cleanup
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FPCGExtendedToolkitTestModule, PCGExtendedToolkitTest)

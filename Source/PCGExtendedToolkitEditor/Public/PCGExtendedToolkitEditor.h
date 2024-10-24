#pragma once

#include "CoreMinimal.h"

class FPCGExtendedToolkitEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};

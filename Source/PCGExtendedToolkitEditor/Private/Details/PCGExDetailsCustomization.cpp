// Copyright (c) Nebukam


#include "Details/PCGExDetailsCustomization.h"

#include "Details/Tuple/PCGExTupleBodyCustomization.h"

namespace PCGExDetailsCustomization
{
	void RegisterDetailsCustomization()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomPropertyTypeLayout("PCGExTupleBody", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGExTupleBodyCustomization::MakeInstance));
	}
}

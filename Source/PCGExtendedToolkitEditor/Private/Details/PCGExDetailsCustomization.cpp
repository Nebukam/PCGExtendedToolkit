// Copyright (c) Nebukam


#include "Details/PCGExDetailsCustomization.h"

#include "Details/Enums/PCGExInlineEnumCustomization.h"
#include "Details/Tuple/PCGExTupleBodyCustomization.h"

namespace PCGExDetailsCustomization
{
	void RegisterDetailsCustomization()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		
		PropertyModule.RegisterCustomPropertyTypeLayout(
			"PCGExTupleBody",
			FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FPCGExTupleBodyCustomization::MakeInstance));

#define PCGEX_DECL_INLINE_ENUM(_NAME, _CLASS) PropertyModule.RegisterCustomPropertyTypeLayout(_NAME,FOnGetPropertyTypeCustomizationInstance::CreateStatic(&_CLASS::MakeInstance));

		PCGEX_DECL_INLINE_ENUM("EPCGExInputValueType", FPCGExInputValueTypeCustomization)
		PCGEX_DECL_INLINE_ENUM("EPCGExDataInputValueType", FPCGExDataInputValueTypeCustomization)
		PCGEX_DECL_INLINE_ENUM("EPCGExApplySampledComponentFlags", FPCGExApplyAxisFlagCustomization)
		PCGEX_DECL_INLINE_ENUM("EPCGExOptionState", FPCGExOptionStateCustomization)
		PCGEX_DECL_INLINE_ENUM("EPCGExPointBoundsSource", FPCGExBoundsSourceCustomization)
		PCGEX_DECL_INLINE_ENUM("EPCGExDistance", FPCGExDistanceCustomization)
		PCGEX_DECL_INLINE_ENUM("EPCGExClusterElement", FPCGExClusterElementCustomization)
		PCGEX_DECL_INLINE_ENUM("EPCGExAttributeFilter", FPCGExAttributeFilterCustomization)
		
		PCGEX_DECL_INLINE_ENUM("EPCGExScaleToFit", FPCGExScaleToFitCustomization)
		PCGEX_DECL_INLINE_ENUM("EPCGExJustifyFrom", FPCGExJustifyFromCustomization)
		PCGEX_DECL_INLINE_ENUM("EPCGExJustifyTo", FPCGExJustifyToCustomization)

#undef PCGEX_DECL_INLINE_ENUM
	}
}

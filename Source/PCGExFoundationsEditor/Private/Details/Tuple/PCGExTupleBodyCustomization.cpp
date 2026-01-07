// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/Tuple/PCGExTupleBodyCustomization.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "Elements/Constants/PCGExTuple.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"

TSharedRef<IPropertyTypeCustomization> FPCGExTupleBodyCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExTupleBodyCustomization());
}

void FPCGExTupleBodyCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow.NameContent()[PropertyHandle->CreatePropertyNameWidget()];
}

void FPCGExTupleBodyCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Grab parent composition
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	if (OuterObjects.Num() == 0) { return; }

	const TArray<FPCGExTupleValueHeader>* Composition = nullptr;
	if (const UPCGExTupleSettings* TupleContainer = Cast<UPCGExTupleSettings>(OuterObjects[0])) { Composition = &TupleContainer->Composition; }
	else { return; }

	// Grab row array
	TSharedPtr<IPropertyHandle> RowHandle = PropertyHandle->GetChildHandle(TEXT("Row"));
	if (!RowHandle.IsValid()) { return; }

	uint32 NumElements = 0;
	RowHandle->GetNumChildren(NumElements);

	for (uint32 i = 0; i < NumElements; ++i)
	{
		TSharedPtr<IPropertyHandle> ElementHandle = RowHandle->GetChildHandle(i);
		if (!ElementHandle.IsValid()) { continue; }

		TArray<void*> RawData;
		ElementHandle->AccessRawData(RawData);
		if (RawData.IsEmpty() || !RawData[0]) { continue; }

		// Tuples are FInstanced struct so it's a bit annoying to handle
		FInstancedStruct* Instance = static_cast<FInstancedStruct*>(RawData[0]);
		if (!Instance || !Instance->IsValid()) { continue; }

		UScriptStruct* InnerStruct = const_cast<UScriptStruct*>(Instance->GetScriptStruct());
		if (!InnerStruct) { continue; }

		// Actual wrapped struct data.
		uint8* StructMemory = Instance->GetMutableMemory();
		if (!StructMemory) { continue; }

		// Locate the "Value" property in the actual struct definition
		if (const FProperty* ValueProperty = InnerStruct->FindPropertyByName(TEXT("Value")))
		{
			// Add a direct property row for this field
			IDetailPropertyRow& Row = *ChildBuilder.AddExternalStructureProperty(
				MakeShared<FStructOnScope>(InnerStruct, StructMemory),
				ValueProperty->GetFName()
			);
			Row.DisplayName(FText::FromName((Composition->GetData() + i)->Name));
		}
	}
}

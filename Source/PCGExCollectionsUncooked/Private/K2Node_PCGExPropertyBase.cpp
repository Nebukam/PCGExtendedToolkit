// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "K2Node_PCGExPropertyBase.h"

#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "KismetCompiler.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Core/PCGExAssetCollection.h"
#include "Details/PCGExK2NodeTypeHelpers.h"
#include "Helpers/PCGExCollectionEntryBlueprintLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Metadata/PCGMetadataAttributeTraits.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "K2Node_PCGExPropertyBase"

// Pin names match the library functions' parameter names so ExpandNode can look up the
// spawned UK2Node_CallFunction's pins via FindPinChecked. Display labels are set via
// PinFriendlyName in AllocateDefaultPins.
const FName UK2Node_PCGExPropertyBase::CollectionPinName(TEXT("Collection"));
const FName UK2Node_PCGExPropertyBase::EntryIndexPinName(TEXT("EntryIndex"));
const FName UK2Node_PCGExPropertyBase::CategoryPinName(TEXT("Category"));
const FName UK2Node_PCGExPropertyBase::PropertyNamePinName(TEXT("PropertyName"));
const FName UK2Node_PCGExPropertyBase::OutValuePinName(TEXT("OutValue"));
const FName UK2Node_PCGExPropertyBase::NewValuePinName(TEXT("NewValue"));
const FName UK2Node_PCGExPropertyBase::ReadbackPinName(TEXT("Readback"));
const FName UK2Node_PCGExPropertyBase::SuccessPinName(TEXT("Success"));

namespace PCGExK2PropertyBase
{
	// Forward one of our input pins into every intermediate call: live links are connected to each
	// target, otherwise the literal default is copied to each. The Set path needs it because the
	// same Collection / key / name inputs feed both the write call and the readback call.
	void ForwardInput(const UEdGraphSchema_K2* K2, UEdGraphPin* Source, TArrayView<UEdGraphPin*> Targets)
	{
		if (!Source)
		{
			return;
		}

		for (UEdGraphPin* Target : Targets)
		{
			if (!Target)
			{
				continue;
			}

			if (Source->LinkedTo.Num() > 0)
			{
				for (UEdGraphPin* Linked : Source->LinkedTo)
				{
					K2->TryCreateConnection(Linked, Target);
				}
			}
			else
			{
				Target->DefaultObject = Source->DefaultObject;
				Target->DefaultValue = Source->DefaultValue;
				Target->DefaultTextValue = Source->DefaultTextValue;
			}
		}
	}
}

void UK2Node_PCGExPropertyBase::AllocateDefaultPins()
{
	const bool bSet = IsSetNode();
	const EScope Scope = GetScope();

	if (bSet)
	{
		CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Execute);
		CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, UEdGraphSchema_K2::PN_Then);
	}

	UEdGraphPin* CollectionPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Object, UPCGExAssetCollection::StaticClass(), CollectionPinName);
	CollectionPin->PinFriendlyName = LOCTEXT("CollectionPinFriendlyName", "Collection");

	switch (Scope)
	{
	case EScope::Entry:
		{
			UEdGraphPin* EntryIndexPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Int, EntryIndexPinName);
			EntryIndexPin->PinFriendlyName = LOCTEXT("EntryIndexPinFriendlyName", "Entry Index");
			EntryIndexPin->DefaultValue = TEXT("0");
			break;
		}
	case EScope::Category:
		{
			UEdGraphPin* CategoryPin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Name, CategoryPinName);
			CategoryPin->PinFriendlyName = LOCTEXT("CategoryPinFriendlyName", "Category");
			break;
		}
	case EScope::Collection:
		break;
	default:
		checkNoEntry();
		break;
	}

	UEdGraphPin* NamePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Name, PropertyNamePinName);
	NamePin->PinFriendlyName = LOCTEXT("PropertyNamePinFriendlyName", "Prop");

	TArray<UEdGraphPin*, TInlineAllocator<2>> Wildcards;
	if (bSet)
	{
		UEdGraphPin* NewValuePin = CreatePin(EGPD_Input, UEdGraphSchema_K2::PC_Wildcard, NewValuePinName);
		NewValuePin->PinFriendlyName = LOCTEXT("NewValuePinFriendlyName", "New Value");
		Wildcards.Add(NewValuePin);

		UEdGraphPin* ReadbackPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, ReadbackPinName);
		ReadbackPin->PinFriendlyName = LOCTEXT("ReadbackPinFriendlyName", "Readback");
		Wildcards.Add(ReadbackPin);
	}
	else
	{
		UEdGraphPin* ValuePin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Wildcard, OutValuePinName);
		ValuePin->PinFriendlyName = LOCTEXT("ValuePinFriendlyName", "Value");
		Wildcards.Add(ValuePin);
	}

	// Restore the previously resolved type so the wildcard isn't reset to gray on graph reopen.
	// The engine's pin reconstruction only carries the type across when the pin has live
	// connections; manually-picked types with no wiring would otherwise be lost.
	if (ResolvedPinType.PinCategory != NAME_None &&
		ResolvedPinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard)
	{
		for (UEdGraphPin* Pin : Wildcards)
		{
			Pin->PinType = ResolvedPinType;
		}
	}

	UEdGraphPin* SuccessPin = CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Boolean, SuccessPinName);
	SuccessPin->PinFriendlyName = LOCTEXT("SuccessPinFriendlyName", "Success");

	Super::AllocateDefaultPins();
}

FText UK2Node_PCGExPropertyBase::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return FText::Format(
		IsSetNode() ? LOCTEXT("SetTitle", "Set {0} Property") : LOCTEXT("GetTitle", "Get {0} Property"),
		GetScopeNoun(GetScope()));
}

FText UK2Node_PCGExPropertyBase::GetTooltipText() const
{
	const bool bSet = IsSetNode();
	switch (GetScope())
	{
	case EScope::Entry:
		return bSet
			? LOCTEXT("SetEntryTooltip",
			          "Writes a value to an entry's property override slot on a PCGEx Asset Collection "
			          "(by raw entry index), enables the override, and reads back the resolved value.\n"
			          "The property must be part of the collection's schema.\n"
			          "New Value and Readback share the same type at compile time; connecting either to "
			          "a typed pin retypes both.\n"
			          "Returns true when the write (and conversion) succeeded.")
			: LOCTEXT("GetEntryTooltip",
			          "Reads an entry's resolved property from a PCGEx Asset Collection by raw entry index\n"
			          "(enabled per-entry override first, then the entry's category override, then the collection default).\n"
			          "The output pin's type drives how the stored value is converted.\n"
			          "Returns true when the conversion succeeded.");
	case EScope::Category:
		return bSet
			? LOCTEXT("SetCategoryTooltip",
			          "Writes a value to a category's property override slot on a PCGEx Asset Collection, "
			          "minting the category's override row when it doesn't exist yet, enables the override, "
			          "and reads back the resolved value.\n"
			          "The property must be part of the collection's schema; category None has no row.\n"
			          "New Value and Readback share the same type at compile time; connecting either to "
			          "a typed pin retypes both.\n"
			          "Returns true when the write (and conversion) succeeded.")
			: LOCTEXT("GetCategoryTooltip",
			          "Reads a category's resolved property from a PCGEx Asset Collection\n"
			          "(enabled category override first, collection default otherwise).\n"
			          "The output pin's type drives how the stored value is converted.\n"
			          "Returns true when the conversion succeeded.");
	case EScope::Collection:
		return bSet
			? LOCTEXT("SetCollectionTooltip",
			          "Writes a property's collection-level default on a PCGEx Asset Collection and reads back "
			          "the resolved value.\n"
			          "Locally declared properties are written in place; properties coming from an imported "
			          "schema asset are written to the collection's import override (the asset is never touched).\n"
			          "New Value and Readback share the same type at compile time; connecting either to "
			          "a typed pin retypes both.\n"
			          "Returns true when the write (and conversion) succeeded.")
			: LOCTEXT("GetCollectionTooltip",
			          "Reads a property's collection-level default from a PCGEx Asset Collection\n"
			          "(local schema entry, enabled import override, or imported schema asset default).\n"
			          "The output pin's type drives how the stored value is converted.\n"
			          "Returns true when the conversion succeeded.");
	default:
		checkNoEntry();
		return FText::GetEmpty();
	}
}

FSlateIcon UK2Node_PCGExPropertyBase::GetIconAndTint(FLinearColor& OutColor) const
{
	OutColor = GetNodeTitleColor();
	static FSlateIcon Icon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Function_16x");
	return Icon;
}

FLinearColor UK2Node_PCGExPropertyBase::GetNodeTitleColor() const
{
	return FLinearColor(0.0f, 0.5f, 0.8f);
}

FText UK2Node_PCGExPropertyBase::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "PCGEx|Collection");
}

void UK2Node_PCGExPropertyBase::PinConnectionListChanged(UEdGraphPin* Pin)
{
	Super::PinConnectionListChanged(Pin);

	TArray<UEdGraphPin*> Wildcards;
	GetWildcardPins(Wildcards);
	if (!Pin || !Wildcards.Contains(Pin))
	{
		return;
	}

	if (Pin->LinkedTo.Num() > 0)
	{
		AdoptTypeFromOther(Pin->LinkedTo[0]);
		GetGraph()->NotifyGraphChanged();
		return;
	}

	// Every wildcard fully disconnected -- revert so the node can adapt to the next connection.
	bool bAnyLinked = false;
	bool bAnyTyped = false;
	for (const UEdGraphPin* Wildcard : Wildcards)
	{
		bAnyLinked |= Wildcard->LinkedTo.Num() > 0;
		bAnyTyped |= Wildcard->PinType.PinCategory != UEdGraphSchema_K2::PC_Wildcard;
	}
	if (!bAnyLinked && bAnyTyped)
	{
		ResetWildcardPins();
		GetGraph()->NotifyGraphChanged();
	}
}

void UK2Node_PCGExPropertyBase::GetNodeContextMenuActions(UToolMenu* Menu, UGraphNodeContextMenuContext* Context) const
{
	Super::GetNodeContextMenuActions(Menu, Context);

	if (Context->bIsDebugging || !Context->Pin)
	{
		return;
	}

	TArray<UEdGraphPin*> Wildcards;
	GetWildcardPins(Wildcards);
	if (!Wildcards.Contains(Context->Pin))
	{
		return;
	}

	FToolMenuSection& Section = Menu->AddSection(
		GetClass()->GetFName(),
		IsSetNode() ? LOCTEXT("ChangeValueType", "Change Value Type") : LOCTEXT("ChangeOutputType", "Change Output Type"));

	const UK2Node_PCGExPropertyBase* ConstThis = this;

	for (uint8 i = 0; i < static_cast<uint8>(EPCGMetadataTypes::Count); ++i)
	{
		const EPCGMetadataTypes MetadataType = static_cast<EPCGMetadataTypes>(i);

		FEdGraphPinType PinType;
		if (!PCGExK2NodeTypeHelpers::MakePinTypeForMetadataType(MetadataType, PinType))
		{
			continue;
		}

		Section.AddMenuEntry(
			NAME_None,
			PCGExK2NodeTypeHelpers::GetDisplayNameForMetadataType(MetadataType),
			FText::GetEmpty(),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([ConstThis, PinType]()
			{
				if (UK2Node_PCGExPropertyBase* MutableThis = const_cast<UK2Node_PCGExPropertyBase*>(ConstThis))
				{
					MutableThis->SetWildcardPinsType(PinType);
				}
			})));
	}
}

void UK2Node_PCGExPropertyBase::ExpandNode(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	const bool bSet = IsSetNode();
	const EScope Scope = GetScope();

	UEdGraphPin* ValuePin = GetValuePin();
	UEdGraphPin* ReadbackPin = GetReadbackPin();

	auto IsUnresolved = [](const UEdGraphPin* Pin) { return !Pin || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Wildcard; };
	if (IsUnresolved(ValuePin) || (bSet && IsUnresolved(ReadbackPin)))
	{
		CompilerContext.MessageLog.Error(
			*(bSet
				  ? LOCTEXT("SetWildcardUnresolved", "@@ has unresolved wildcard pins. Connect either New Value or Readback to a typed pin, or pick a type from either pin's right-click menu.")
				  : LOCTEXT("GetWildcardUnresolved", "@@ has an unresolved wildcard output. Connect the Value pin to a typed input or pick a type from its right-click menu."))
			.ToString(),
			this);
		BreakAllNodeLinks();
		return;
	}

	// Object/Class pins go to dedicated typed library functions: the CustomStructureParam wildcard path
	// would stuff them into an `int32&` slot and corrupt the property's soft-path payload. Struct and
	// primitive types stay on the wildcard path, where the BP compiler's frame marshalling behaves.
	const FName Category = ValuePin->PinType.PinCategory;
	const bool bIsObjectLike =
		Category == UEdGraphSchema_K2::PC_Object ||
		Category == UEdGraphSchema_K2::PC_Interface ||
		Category == UEdGraphSchema_K2::PC_SoftObject;
	const bool bIsClassLike =
		Category == UEdGraphSchema_K2::PC_Class ||
		Category == UEdGraphSchema_K2::PC_SoftClass;
	const EFlavor Flavor = bIsObjectLike ? EFlavor::Object : bIsClassLike ? EFlavor::Class : EFlavor::Wildcard;

	UClass* TargetClass = nullptr;
	if (Flavor != EFlavor::Wildcard)
	{
		TargetClass = Cast<UClass>(ValuePin->PinType.PinSubCategoryObject.Get());
		if (!TargetClass)
		{
			CompilerContext.MessageLog.Error(
				*LOCTEXT("ObjectPinMissingClass", "@@ has an Object/Class pin with no resolved class.").ToString(),
				this);
			BreakAllNodeLinks();
			return;
		}
	}

	const FName KeyPinName = GetKeyPinName(Scope);

	// Get call (pure). On Set nodes it is the readback: pure nodes evaluate when their output is consumed,
	// after the Set's exec fired, so it reflects post-write state. Single-wildcard calls only: multi-wildcard
	// CustomStructureParam doesn't reliably construct non-trivially-copyable output buffers.
	UK2Node_CallFunction* GetCall = SpawnLibraryCall(CompilerContext, SourceGraph, /*bSet=*/false, Flavor);
	UEdGraphPin* GetCallCollection = GetCall->FindPinChecked(CollectionPinName);
	UEdGraphPin* GetCallKey = KeyPinName.IsNone() ? nullptr : GetCall->FindPinChecked(KeyPinName);
	UEdGraphPin* GetCallName = GetCall->FindPinChecked(PropertyNamePinName);

	// Typed flavors use DeterminesOutputType on the function's return value: set ExpectedClass to
	// TargetClass and trigger the call node's dynamic-output retyping. The wildcard flavor stamps
	// the user's resolved type onto the CustomStructureParam output instead.
	UEdGraphPin* GetCallValue = nullptr;
	UEdGraphPin* GetCallSuccess = nullptr;
	if (Flavor != EFlavor::Wildcard)
	{
		UEdGraphPin* ExpectedClassPin = GetCall->FindPinChecked(TEXT("ExpectedClass"));
		ExpectedClassPin->DefaultObject = TargetClass;
		GetCall->PinDefaultValueChanged(ExpectedClassPin);
		GetCallValue = GetCall->GetReturnValuePin();
		GetCallSuccess = GetCall->FindPinChecked(TEXT("bSuccess"));
	}
	else
	{
		GetCallValue = GetCall->FindPinChecked(OutValuePinName);
		GetCallValue->PinType = bSet ? ReadbackPin->PinType : ValuePin->PinType;
		GetCallSuccess = GetCall->GetReturnValuePin();
	}

	if (!bSet)
	{
		CompilerContext.MovePinLinksToIntermediate(*GetCollectionPin(), *GetCallCollection);
		if (GetCallKey)
		{
			CompilerContext.MovePinLinksToIntermediate(*GetKeyPin(), *GetCallKey);
		}
		CompilerContext.MovePinLinksToIntermediate(*GetPropertyNamePin(), *GetCallName);
		CompilerContext.MovePinLinksToIntermediate(*ValuePin, *GetCallValue);
		CompilerContext.MovePinLinksToIntermediate(*GetSuccessPin(), *GetCallSuccess);

		BreakAllNodeLinks();
		return;
	}

	// Set call (impure). Carries the user's exec flow and Success bool.
	UK2Node_CallFunction* SetCall = SpawnLibraryCall(CompilerContext, SourceGraph, /*bSet=*/true, Flavor);
	UEdGraphPin* SetCallCollection = SetCall->FindPinChecked(CollectionPinName);
	UEdGraphPin* SetCallKey = KeyPinName.IsNone() ? nullptr : SetCall->FindPinChecked(KeyPinName);
	UEdGraphPin* SetCallName = SetCall->FindPinChecked(PropertyNamePinName);

	// The wildcard flavor needs type stamping; the typed flavors have a concrete UObject*/UClass*
	// pin that accepts the user's connection via the schema's standard upcast.
	UEdGraphPin* SetCallNewValue = nullptr;
	switch (Flavor)
	{
	case EFlavor::Object:
		SetCallNewValue = SetCall->FindPinChecked(TEXT("NewObject"));
		break;
	case EFlavor::Class:
		SetCallNewValue = SetCall->FindPinChecked(TEXT("NewClass"));
		break;
	default:
		SetCallNewValue = SetCall->FindPinChecked(NewValuePinName);
		SetCallNewValue->PinType = ValuePin->PinType;
		break;
	}

	CompilerContext.MovePinLinksToIntermediate(*GetExecInputPin(), *SetCall->GetExecPin());
	CompilerContext.MovePinLinksToIntermediate(*GetThenPin(), *SetCall->GetThenPin());
	CompilerContext.MovePinLinksToIntermediate(*ValuePin, *SetCallNewValue);
	CompilerContext.MovePinLinksToIntermediate(*GetSuccessPin(), *SetCall->GetReturnValuePin());

	const UEdGraphSchema_K2* K2 = GetDefault<UEdGraphSchema_K2>();
	UEdGraphPin* CollectionTargets[] = {SetCallCollection, GetCallCollection};
	UEdGraphPin* KeyTargets[] = {SetCallKey, GetCallKey};
	UEdGraphPin* NameTargets[] = {SetCallName, GetCallName};
	PCGExK2PropertyBase::ForwardInput(K2, GetCollectionPin(), CollectionTargets);
	PCGExK2PropertyBase::ForwardInput(K2, GetKeyPin(), KeyTargets);
	PCGExK2PropertyBase::ForwardInput(K2, GetPropertyNamePin(), NameTargets);

	CompilerContext.MovePinLinksToIntermediate(*ReadbackPin, *GetCallValue);

	BreakAllNodeLinks();
}

UK2Node_CallFunction* UK2Node_PCGExPropertyBase::SpawnLibraryCall(FKismetCompilerContext& CompilerContext, UEdGraph* SourceGraph, bool bSet, EFlavor Flavor)
{
	UK2Node_CallFunction* CallNode = CompilerContext.SpawnIntermediateNode<UK2Node_CallFunction>(this, SourceGraph);
	CallNode->FunctionReference.SetExternalMember(GetLibraryFunctionName(GetScope(), bSet, Flavor), UPCGExCollectionEntryBlueprintLibrary::StaticClass());
	CallNode->AllocateDefaultPins();
	return CallNode;
}

FName UK2Node_PCGExPropertyBase::GetLibraryFunctionName(EScope Scope, bool bSet, EFlavor Flavor)
{
#define PCGEX_LIB_FN(_NAME) GET_FUNCTION_NAME_CHECKED(UPCGExCollectionEntryBlueprintLibrary, _NAME)
	switch (Scope)
	{
	case EScope::Entry:
		switch (Flavor)
		{
		case EFlavor::Object: return bSet ? PCGEX_LIB_FN(TrySetEntryPropertyObject) : PCGEX_LIB_FN(TryGetEntryPropertyObject);
		case EFlavor::Class: return bSet ? PCGEX_LIB_FN(TrySetEntryPropertyClass) : PCGEX_LIB_FN(TryGetEntryPropertyClass);
		default: return bSet ? PCGEX_LIB_FN(TrySetEntryPropertyOverride) : PCGEX_LIB_FN(TryGetEntryPropertyValue);
		}
	case EScope::Category:
		switch (Flavor)
		{
		case EFlavor::Object: return bSet ? PCGEX_LIB_FN(TrySetCategoryPropertyObject) : PCGEX_LIB_FN(TryGetCategoryPropertyObject);
		case EFlavor::Class: return bSet ? PCGEX_LIB_FN(TrySetCategoryPropertyClass) : PCGEX_LIB_FN(TryGetCategoryPropertyClass);
		default: return bSet ? PCGEX_LIB_FN(TrySetCategoryPropertyOverride) : PCGEX_LIB_FN(TryGetCategoryPropertyValue);
		}
	case EScope::Collection:
		switch (Flavor)
		{
		case EFlavor::Object: return bSet ? PCGEX_LIB_FN(TrySetCollectionPropertyObject) : PCGEX_LIB_FN(TryGetCollectionPropertyObject);
		case EFlavor::Class: return bSet ? PCGEX_LIB_FN(TrySetCollectionPropertyClass) : PCGEX_LIB_FN(TryGetCollectionPropertyClass);
		default: return bSet ? PCGEX_LIB_FN(TrySetCollectionPropertyDefault) : PCGEX_LIB_FN(TryGetCollectionPropertyValue);
		}
	default:
		checkNoEntry();
		return NAME_None;
	}
#undef PCGEX_LIB_FN
}

FName UK2Node_PCGExPropertyBase::GetKeyPinName(const EScope Scope)
{
	switch (Scope)
	{
	case EScope::Entry: return EntryIndexPinName;
	case EScope::Category: return CategoryPinName;
	default: return NAME_None;
	}
}

FText UK2Node_PCGExPropertyBase::GetScopeNoun(const EScope Scope)
{
	switch (Scope)
	{
	case EScope::Entry: return LOCTEXT("ScopeEntry", "Entry");
	case EScope::Category: return LOCTEXT("ScopeCategory", "Category");
	default: return LOCTEXT("ScopeCollection", "Collection");
	}
}

UEdGraphPin* UK2Node_PCGExPropertyBase::GetExecInputPin() const
{
	return FindPin(UEdGraphSchema_K2::PN_Execute);
}

UEdGraphPin* UK2Node_PCGExPropertyBase::GetThenPin() const
{
	return FindPin(UEdGraphSchema_K2::PN_Then);
}

UEdGraphPin* UK2Node_PCGExPropertyBase::GetCollectionPin() const
{
	return FindPin(CollectionPinName);
}

UEdGraphPin* UK2Node_PCGExPropertyBase::GetKeyPin() const
{
	const FName KeyPinName = GetKeyPinName(GetScope());
	return KeyPinName.IsNone() ? nullptr : FindPin(KeyPinName);
}

UEdGraphPin* UK2Node_PCGExPropertyBase::GetPropertyNamePin() const
{
	return FindPin(PropertyNamePinName);
}

UEdGraphPin* UK2Node_PCGExPropertyBase::GetValuePin() const
{
	return FindPin(IsSetNode() ? NewValuePinName : OutValuePinName);
}

UEdGraphPin* UK2Node_PCGExPropertyBase::GetReadbackPin() const
{
	return IsSetNode() ? FindPin(ReadbackPinName) : nullptr;
}

UEdGraphPin* UK2Node_PCGExPropertyBase::GetSuccessPin() const
{
	return FindPin(SuccessPinName);
}

void UK2Node_PCGExPropertyBase::GetWildcardPins(TArray<UEdGraphPin*>& OutPins) const
{
	OutPins.Reset();
	if (UEdGraphPin* ValuePin = GetValuePin())
	{
		OutPins.Add(ValuePin);
	}
	if (UEdGraphPin* ReadbackPin = GetReadbackPin())
	{
		OutPins.Add(ReadbackPin);
	}
}

void UK2Node_PCGExPropertyBase::ResetWildcardPins()
{
	TArray<UEdGraphPin*> Wildcards;
	GetWildcardPins(Wildcards);
	for (UEdGraphPin* Pin : Wildcards)
	{
		Pin->PinType = FEdGraphPinType();
		Pin->PinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
		Pin->BreakAllPinLinks();
	}

	ResolvedPinType = FEdGraphPinType();
	ResolvedPinType.PinCategory = UEdGraphSchema_K2::PC_Wildcard;
}

void UK2Node_PCGExPropertyBase::AdoptTypeFromOther(const UEdGraphPin* OtherPin)
{
	if (!OtherPin)
	{
		return;
	}

	FEdGraphPinType InType = OtherPin->PinType;
	InType.ContainerType = EPinContainerType::None;

	TArray<UEdGraphPin*> Wildcards;
	GetWildcardPins(Wildcards);
	for (UEdGraphPin* Pin : Wildcards)
	{
		Pin->PinType = InType;
	}
	ResolvedPinType = InType;
}

void UK2Node_PCGExPropertyBase::SetWildcardPinsType(const FEdGraphPinType& InType)
{
	TArray<UEdGraphPin*> Wildcards;
	GetWildcardPins(Wildcards);
	for (UEdGraphPin* Pin : Wildcards)
	{
		// Break links that no longer match the new type.
		for (int32 i = Pin->LinkedTo.Num() - 1; i >= 0; --i)
		{
			const UEdGraphPin* Linked = Pin->LinkedTo[i];
			if (!Linked || Linked->PinType != InType)
			{
				Pin->BreakLinkTo(Pin->LinkedTo[i]);
			}
		}
		Pin->PinType = InType;
	}
	ResolvedPinType = InType;

	GetGraph()->NotifyGraphChanged();

	if (UBlueprint* BP = GetBlueprint())
	{
		FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
	}
}

#undef LOCTEXT_NAMESPACE

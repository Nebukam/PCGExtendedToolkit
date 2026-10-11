// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Details/PCGExPropertyChoicesCustomization.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "InstancedStructDetails.h"
#include "PCGExInlineWidgetRegistry.h"
#include "PCGExProperty.h"
#include "Framework/Notifications/NotificationManager.h"
#include "PCGExPropertyRowHost.h"
#include "PCGExPropertySchemaAsset.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Textures/SlateIcon.h"
#include "PCGExPropertyChoicesAlign.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FPCGExPropertyChoicesCustomization"

namespace PCGExPropertyChoicesCustomization
{
	/** The object the handle edits: the popover's context first, else the handle's single outer. */
	TWeakObjectPtr<UObject> ResolveOwner(const TSharedRef<IPropertyHandle>& Handle);

	/** The align lever as a small combo: this host's rows, or every loaded importer of a schema asset. */
	TSharedRef<SWidget> MakeAlignButton(const TWeakObjectPtr<UObject>& Owner, FStructView Host);

	/** Editor toast summarizing an align run. */
	void NotifyAlignResult(const PCGExPropertyChoicesAlign::FResult& Result);

	/** The popover's action row, full width, right-aligned: [Add ...] [Align v]. */
	void AddActionsRow(
		IDetailChildrenBuilder& ChildBuilder,
		const TWeakObjectPtr<UObject>& Owner,
		FStructView Host,
		const FText& AddLabel,
		const FText& AddToolTip,
		FOnClicked OnAdd);
	/** Raw data of the nearest ancestor handle whose struct is exactly Struct; null when none or multi-object. */
	template <typename T>
	T* ResolveAncestorRaw(const TSharedRef<IPropertyHandle>& Handle, const UScriptStruct* Struct)
	{
		for (TSharedPtr<IPropertyHandle> Parent = Handle->GetParentHandle(); Parent.IsValid(); Parent = Parent->GetParentHandle())
		{
			const FStructProperty* StructProperty = CastField<FStructProperty>(Parent->GetProperty());
			if (!StructProperty || StructProperty->Struct != Struct)
			{
				continue;
			}
			TArray<void*> RawData;
			Parent->AccessRawData(RawData);
			return RawData.Num() == 1 && RawData[0] ? static_cast<T*>(RawData[0]) : nullptr;
		}
		return nullptr;
	}

	FStructView ViewOf(FInstancedStruct& Instance)
	{
		const UScriptStruct* Type = Instance.GetScriptStruct();
		return Type && Type->IsChildOf(FPCGExProperty::StaticStruct()) ? FStructView(Instance) : FStructView();
	}

	TMap<const void*, FPCGExChoicesEditContext>& GetEditContexts()
	{
		static TMap<const void*, FPCGExChoicesEditContext> Contexts;
		return Contexts;
	}

	// The edit context of the choices (or patch) struct Handle is, or sits under: a scratch copy in the popover has no
	// ancestors to walk, so the popover registers its context under the copy's address.
	const FPCGExChoicesEditContext* FindEditContext(const TSharedRef<IPropertyHandle>& Handle)
	{
		TArray<void*> RawData;
		Handle->AccessRawData(RawData);
		if (RawData.Num() == 1 && RawData[0])
		{
			if (const FPCGExChoicesEditContext* Own = FPCGExChoicesEditScope::Find(RawData[0]))
			{
				return Own;
			}
		}
		if (const void* Choices = ResolveAncestorRaw<void>(Handle, FPCGExPropertyChoices::StaticStruct()))
		{
			return FPCGExChoicesEditScope::Find(Choices);
		}
		if (const void* Patch = ResolveAncestorRaw<void>(Handle, FPCGExPropertyChoicesPatch::StaticStruct()))
		{
			return FPCGExChoicesEditScope::Find(Patch);
		}
		return nullptr;
	}

	// The property struct instance Handle sits in: the nearest ancestor that is a property-derived struct row, the
	// FInstancedStruct holding one (a struct row made from a provider may expose no FProperty), or an override entry,
	// whose Value is the host of its patch's items. Single-object edits only.
	FStructView ResolveHost(const TSharedRef<IPropertyHandle>& Handle)
	{
		for (TSharedPtr<IPropertyHandle> Parent = Handle->GetParentHandle(); Parent.IsValid(); Parent = Parent->GetParentHandle())
		{
			const FStructProperty* StructProperty = CastField<FStructProperty>(Parent->GetProperty());
			if (!StructProperty || !StructProperty->Struct)
			{
				continue;
			}

			const bool bPropertyRow = StructProperty->Struct->IsChildOf(FPCGExProperty::StaticStruct());
			const bool bInstancedStruct = StructProperty->Struct == FInstancedStruct::StaticStruct();
			const bool bOverrideEntry = StructProperty->Struct == FPCGExPropertyOverrideEntry::StaticStruct();
			if (!bPropertyRow && !bInstancedStruct && !bOverrideEntry)
			{
				continue;
			}

			TArray<void*> RawData;
			Parent->AccessRawData(RawData);
			if (RawData.Num() != 1 || !RawData[0])
			{
				return FStructView();
			}
			if (bPropertyRow)
			{
				return FStructView(StructProperty->Struct, static_cast<uint8*>(RawData[0]));
			}
			if (bOverrideEntry)
			{
				return ViewOf(static_cast<FPCGExPropertyOverrideEntry*>(RawData[0])->Value);
			}
			return ViewOf(*static_cast<FInstancedStruct*>(RawData[0]));
		}

		const FPCGExChoicesEditContext* Context = FindEditContext(Handle);
		return Context ? Context->Host : FStructView();
	}

	void NotifyAlignResult(const PCGExPropertyChoicesAlign::FResult& Result)
	{
		FNotificationInfo Info(Result.RowsAligned > 0
			? FText::Format(LOCTEXT("Aligned", "Aligned {0} row(s) across {1} host(s)."), FText::AsNumber(Result.RowsAligned), FText::AsNumber(Result.HostsTouched))
			: LOCTEXT("NothingToAlign", "Every row already sits on a choice."));
		Info.ExpireDuration = 4.0f;
		Info.bUseSuccessFailIcons = true;
		if (const TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info))
		{
			Item->SetCompletionState(Result.RowsAligned > 0 ? SNotificationItem::CS_Success : SNotificationItem::CS_None);
		}
	}

	void AddActionsRow(
		IDetailChildrenBuilder& ChildBuilder,
		const TWeakObjectPtr<UObject>& Owner,
		const FStructView Host,
		const FText& AddLabel,
		const FText& AddToolTip,
		FOnClicked OnAdd)
	{
		ChildBuilder.AddCustomRow(LOCTEXT("ChoicesActionsFilter", "Actions"))
		            .WholeRowContent()
			[
				SNew(SBox)
				.Padding(FMargin(0, 2, 0, 2))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0, 0, 4, 0)
					[
						SNew(SButton)
						.Text(AddLabel)
						.ToolTipText(AddToolTip)
						.OnClicked(OnAdd)
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						MakeAlignButton(Owner, Host)
					]
				]
			];
	}

	TWeakObjectPtr<UObject> ResolveOwner(const TSharedRef<IPropertyHandle>& Handle)
	{
		if (const FPCGExChoicesEditContext* Context = FindEditContext(Handle))
		{
			return Context->Owner;
		}
		TArray<UObject*> Outers;
		Handle->GetOuterObjects(Outers);
		return Outers.Num() == 1 ? Outers[0] : nullptr;
	}

	TSharedRef<SWidget> MakeAlignButton(const TWeakObjectPtr<UObject>& Owner, const FStructView Host)
	{
		const FPCGExProperty* Property = Host.IsValid() ? Host.GetPtr<FPCGExProperty>() : nullptr;
		const int32 HeaderId = Property ? Property->HeaderId : 0;
		const FName PropertyName = Property ? Property->PropertyName : NAME_None;

		return SNew(SComboButton)
			.ComboButtonStyle(FAppStyle::Get(), "SimpleComboButton")
			.HasDownArrow(true)
			.ToolTipText(LOCTEXT("AlignToolTip", "Snap rows whose value is none of the choices onto the default choice (or the first). Rows already on a choice are left alone."))
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AlignLabel", "Align"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			.OnGetMenuContent_Lambda([Owner, HeaderId, PropertyName]() -> TSharedRef<SWidget>
			{
				UObject* OwnerObject = Owner.Get();
				const bool bHostRows = OwnerObject && OwnerObject->GetClass()->ImplementsInterface(UPCGExPropertyRowHost::StaticClass());
				const bool bAsset = OwnerObject && OwnerObject->IsA<UPCGExPropertySchemaAsset>();

				FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection*/ true, nullptr);
				MenuBuilder.AddMenuEntry(
					LOCTEXT("AlignHostRows", "Align this host's rows"),
					LOCTEXT("AlignHostRowsToolTip", "Every enabled row of this collection, node or component whose value is not one of the choices."),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([Owner, HeaderId, PropertyName]()
						{
							FScopedTransaction Transaction(LOCTEXT("AlignHostRowsTransaction", "Align Rows to Choices"));
							PCGExPropertyChoicesCustomization::NotifyAlignResult(PCGExPropertyChoicesAlign::AlignHostRows(Owner.Get(), HeaderId, PropertyName));
						}),
						FCanExecuteAction::CreateLambda([bHostRows]() { return bHostRows; })));
				MenuBuilder.AddMenuEntry(
					LOCTEXT("AlignImporters", "Align all loaded importers"),
					LOCTEXT("AlignImportersToolTip", "Same, across every loaded collection, node or component importing this schema asset. Unloaded assets are not visited."),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([Owner, HeaderId, PropertyName]()
						{
							FScopedTransaction Transaction(LOCTEXT("AlignImportersTransaction", "Align Importers to Choices"));
							PCGExPropertyChoicesCustomization::NotifyAlignResult(PCGExPropertyChoicesAlign::AlignImporters(Cast<UPCGExPropertySchemaAsset>(Owner.Get()), HeaderId, PropertyName));
						}),
						FCanExecuteAction::CreateLambda([bAsset]() { return bAsset; })));
				return MenuBuilder.MakeWidget();
			});
	}
}

#pragma region FPCGExChoicesEditScope

FPCGExChoicesEditScope::FPCGExChoicesEditScope(const void* InKey, const FPCGExChoicesEditContext& InContext)
	: Key(InKey)
{
	if (Key)
	{
		PCGExPropertyChoicesCustomization::GetEditContexts().Add(Key, InContext);
	}
}

FPCGExChoicesEditScope::~FPCGExChoicesEditScope()
{
	if (Key)
	{
		PCGExPropertyChoicesCustomization::GetEditContexts().Remove(Key);
	}
}

const FPCGExChoicesEditContext* FPCGExChoicesEditScope::Find(const void* InKey)
{
	return InKey ? PCGExPropertyChoicesCustomization::GetEditContexts().Find(InKey) : nullptr;
}

#pragma endregion

#pragma region FPCGExPropertyChoicesCustomization

TSharedRef<IPropertyTypeCustomization> FPCGExPropertyChoicesCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExPropertyChoicesCustomization());
}

bool FPCGExPropertyChoicesCustomization::IsUnderReadOnlySchema(const TSharedRef<IPropertyHandle>& Handle)
{
	for (TSharedPtr<IPropertyHandle> Parent = Handle->GetParentHandle(); Parent.IsValid(); Parent = Parent->GetParentHandle())
	{
		if (Parent->HasMetaData(TEXT("ReadOnlySchema")))
		{
			return true;
		}
	}
	return false;
}

void FPCGExPropertyChoicesCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	ChoicesHandlePtr = PropertyHandle;
	WeakPropertyUtilities = CustomizationUtils.GetPropertyUtilities();
	bReadOnly = IsUnderReadOnlySchema(PropertyHandle);
	ItemsHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExPropertyChoices, Items));
	const TSharedPtr<IPropertyHandle> LockedHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExPropertyChoices, bLocked));

	if (bReadOnly || !ItemsHandle.IsValid() || !LockedHandle.IsValid())
	{
		HeaderRow.Visibility(EVisibility::Collapsed);
		return;
	}

	// Rows are added per element below; the engine re-lays-out on a count change but never re-runs this
	// customization, so a shape change needs the deferred full rebuild.
	if (const TSharedPtr<IPropertyHandleArray> ItemsArray = ItemsHandle->AsArray())
	{
		ItemsArray->SetOnNumElementsChanged(FSimpleDelegate::CreateSP(this, &FPCGExPropertyChoicesCustomization::OnNumItemsChanged));
	}

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		.MaxDesiredWidth(3000.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0, 0, 4, 0)
			[
				LockedHandle->CreatePropertyValueWidget()
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("LockedLabel", "Locked"))
				.ToolTipText(LockedHandle->GetToolTipText())
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(12, 0, 0, 0)
			[
				SNew(STextBlock)
				.Text(this, &FPCGExPropertyChoicesCustomization::GetCountText)
				.Font(IDetailLayoutBuilder::GetDetailFontItalic())
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];
}

void FPCGExPropertyChoicesCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	if (bReadOnly || !ItemsHandle.IsValid())
	{
		return;
	}

	// Actions on their own full-width row: the header keeps the state (lock, count) and stays readable.
	PCGExPropertyChoicesCustomization::AddActionsRow(
		ChildBuilder,
		PCGExPropertyChoicesCustomization::ResolveOwner(PropertyHandle),
		PCGExPropertyChoicesCustomization::ResolveHost(PropertyHandle),
		LOCTEXT("AddCurrentValue", "Add Current Value"),
		LOCTEXT("AddCurrentValueToolTip", "Add the property's current value as a new choice."),
		FOnClicked::CreateSP(this, &FPCGExPropertyChoicesCustomization::OnAddCurrentValue));

	// Elements as direct children (no "Items" level): each keeps the engine's insert / delete / duplicate buttons.
	uint32 NumItems = 0;
	ItemsHandle->GetNumChildren(NumItems);
	for (uint32 i = 0; i < NumItems; ++i)
	{
		if (const TSharedPtr<IPropertyHandle> Element = ItemsHandle->GetChildHandle(i))
		{
			ChildBuilder.AddProperty(Element.ToSharedRef());
		}
	}
}

FText FPCGExPropertyChoicesCustomization::GetCountText() const
{
	uint32 NumItems = 0;
	if (ItemsHandle.IsValid())
	{
		ItemsHandle->GetNumChildren(NumItems);
	}
	return NumItems == 1
		? LOCTEXT("OneChoice", "1 choice")
		: FText::Format(LOCTEXT("ChoiceCount", "{0} choices"), FText::AsNumber(static_cast<int32>(NumItems)));
}

FReply FPCGExPropertyChoicesCustomization::OnAddCurrentValue()
{
	const TSharedPtr<IPropertyHandle> ChoicesHandle = ChoicesHandlePtr.Pin();
	if (!ChoicesHandle.IsValid() || !ItemsHandle.IsValid())
	{
		return FReply::Handled();
	}

	const FStructView Host = PCGExPropertyChoicesCustomization::ResolveHost(ChoicesHandle.ToSharedRef());
	TArray<void*> RawData;
	ItemsHandle->AccessRawData(RawData);
	if (!Host.IsValid() || RawData.Num() != 1 || !RawData[0])
	{
		return FReply::Handled();
	}

	TArray<FPCGExPropertyChoice>* Items = static_cast<TArray<FPCGExPropertyChoice>*>(RawData[0]);
	const FName Label(*FString::Printf(TEXT("Choice %d"), Items->Num() + 1));

	// Raw add inside the handle's notify bracket: the owner is Modify()'d by NotifyPreChange, so undo restores the
	// whole array, and NotifyPostChange routes the host's structural sync. The rebuild re-aliases every row.
	FScopedTransaction Transaction(LOCTEXT("AddChoice", "Add Choice"));
	ItemsHandle->NotifyPreChange();
	Items->Add(PCGExProperties::MakeChoiceFromValue(FConstStructView(Host), Label));
	ItemsHandle->NotifyPostChange(EPropertyChangeType::ArrayAdd);
	ItemsHandle->NotifyFinishedChangingProperties();

	OnNumItemsChanged();
	return FReply::Handled();
}

void FPCGExPropertyChoicesCustomization::OnNumItemsChanged()
{
	if (const TSharedPtr<IPropertyUtilities> Utils = WeakPropertyUtilities.Pin())
	{
		Utils->RequestForceRefresh();
	}
}

#pragma endregion

#pragma region FPCGExPropertyChoiceCustomization

TSharedRef<IPropertyTypeCustomization> FPCGExPropertyChoiceCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExPropertyChoiceCustomization());
}

void FPCGExPropertyChoiceCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const TSharedPtr<IPropertyHandle> LabelHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExPropertyChoice, Label));
	CarrierHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExPropertyChoice, Value));
	if (!LabelHandle.IsValid() || !CarrierHandle.IsValid())
	{
		HeaderRow.NameContent()[PropertyHandle->CreatePropertyNameWidget()];
		return;
	}

	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);
	const FPCGExPropertyChoice* Choice = (RawData.Num() == 1 && RawData[0]) ? static_cast<const FPCGExPropertyChoice*>(RawData[0]) : nullptr;
	const FStructView Host = PCGExPropertyChoicesCustomization::ResolveHost(PropertyHandle);
	const UScriptStruct* CarrierType = Choice ? Choice->Value.GetScriptStruct() : nullptr;
	const bool bCompatible = Choice && CarrierType && Host.IsValid() && PCGExProperties::IsChoiceCompatible(FConstStructView(Host), *Choice);

	TSharedRef<SWidget> ValueWidget = SNew(STextBlock)
		.Text(CarrierType
			      ? FText::Format(LOCTEXT("IncompatibleCarrier", "No longer matches the property ({0})"), CarrierType->GetDisplayNameText())
			      : LOCTEXT("EmptyCarrier", "Empty choice"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.ColorAndOpacity(FStyleColors::Warning);

	if (bCompatible)
	{
		// The carrier's fields as real child nodes: edits route through the engine's notify chain like any other row.
		CarrierProvider = MakeShared<FInstancedStructProvider>(CarrierHandle);
		CarrierHandle->RemoveChildren();
		for (const TSharedPtr<IPropertyHandle>& Field : CarrierHandle->AddChildStructure(CarrierProvider.ToSharedRef()))
		{
			if (Field.IsValid() && Field->GetProperty() && Field->GetProperty()->GetFName() == TEXT("Value"))
			{
				CarrierValueHandle = Field;
				break;
			}
		}

		bInlineCarrier = CarrierType->HasMetaData(TEXT("PCGExInlineValue"));
		if (CarrierValueHandle.IsValid())
		{
			if (bInlineCarrier)
			{
				const FPCGExMakeInlineWidgetFn* Factory = FPCGExInlineWidgetRegistry::Find(CarrierType->GetFName(), EPCGExInlineWidgetMode::Compact);
				ValueWidget = Factory ? (*Factory)(CarrierValueHandle.ToSharedRef()) : CarrierValueHandle->CreatePropertyValueWidget();
			}
			else
			{
				// Complex carriers expand to their editor; the header shows a live preview.
				const TWeakPtr<IPropertyHandle> WeakCarrier = CarrierHandle;
				ValueWidget = SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFontItalic())
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					.Text_Lambda([WeakCarrier]() -> FText
					{
						TArray<void*> CarrierRaw;
						if (const TSharedPtr<IPropertyHandle> Pinned = WeakCarrier.Pin())
						{
							Pinned->AccessRawData(CarrierRaw);
						}
						if (CarrierRaw.Num() != 1 || !CarrierRaw[0])
						{
							return FText::GetEmpty();
						}
						return PCGExProperties::GetValuePreviewText(FConstStructView(*static_cast<const FInstancedStruct*>(CarrierRaw[0])));
					});
			}
		}
	}

	HeaderRow
		.NameContent()
		[
			LabelHandle->CreatePropertyValueWidget()
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		.MaxDesiredWidth(3000.0f)
		[
			ValueWidget
		];
}

void FPCGExPropertyChoiceCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	if (!bInlineCarrier && CarrierValueHandle.IsValid())
	{
		ChildBuilder.AddProperty(CarrierValueHandle.ToSharedRef());
	}
}

#pragma endregion

#pragma region FPCGExPropertyChoicesPatchCustomization

TSharedRef<IPropertyTypeCustomization> FPCGExPropertyChoicesPatchCustomization::MakeInstance()
{
	return MakeShareable(new FPCGExPropertyChoicesPatchCustomization());
}

void FPCGExPropertyChoicesPatchCustomization::CustomizeHeader(
	TSharedRef<IPropertyHandle> PropertyHandle,
	FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PatchHandlePtr = PropertyHandle;
	WeakPropertyUtilities = CustomizationUtils.GetPropertyUtilities();
	bReadOnly = FPCGExPropertyChoicesCustomization::IsUnderReadOnlySchema(PropertyHandle);
	ModeHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExPropertyChoicesPatch, Mode));
	LockHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExPropertyChoicesPatch, Lock));
	ItemsHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExPropertyChoicesPatch, Items));
	RemovedIdsHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPCGExPropertyChoicesPatch, RemovedIds));

	if (bReadOnly || !ModeHandle.IsValid() || !LockHandle.IsValid() || !ItemsHandle.IsValid() || !RemovedIdsHandle.IsValid())
	{
		HeaderRow.Visibility(EVisibility::Collapsed);
		return;
	}

	// Every committed edit under the patch re-mirrors the import rows; what changes the row set rebuilds the panel.
	PropertyHandle->SetOnChildPropertyValueChangedWithData(
		TDelegate<void(const FPropertyChangedEvent&)>::CreateSP(this, &FPCGExPropertyChoicesPatchCustomization::OnPatchChanged));
	ModeHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FPCGExPropertyChoicesPatchCustomization::RequestRebuild));
	LockHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FPCGExPropertyChoicesPatchCustomization::RequestRebuild));
	if (const TSharedPtr<IPropertyHandleArray> ItemsArray = ItemsHandle->AsArray())
	{
		ItemsArray->SetOnNumElementsChanged(FSimpleDelegate::CreateSP(this, &FPCGExPropertyChoicesPatchCustomization::RequestRebuild));
	}
	if (const TSharedPtr<IPropertyHandleArray> RemovedArray = RemovedIdsHandle->AsArray())
	{
		RemovedArray->SetOnNumElementsChanged(FSimpleDelegate::CreateSP(this, &FPCGExPropertyChoicesPatchCustomization::RequestRebuild));
	}

	HeaderRow
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("PatchLabel", "Choices"))
			.ToolTipText(PropertyHandle->GetToolTipText())
			.Font(IDetailLayoutBuilder::GetDetailFont())
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		.MaxDesiredWidth(3000.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				ModeHandle->CreatePropertyValueWidget()
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8, 0, 0, 0)
			[
				LockHandle->CreatePropertyValueWidget()
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(12, 0, 0, 0)
			[
				SNew(STextBlock)
				.Text(this, &FPCGExPropertyChoicesPatchCustomization::GetCountText)
				.Font(IDetailLayoutBuilder::GetDetailFontItalic())
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]
		];
}

void FPCGExPropertyChoicesPatchCustomization::CustomizeChildren(
	TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	const FPCGExPropertyChoicesPatch* Patch = GetPatch();
	if (bReadOnly || !Patch || !ItemsHandle.IsValid())
	{
		return;
	}

	PCGExPropertyChoicesCustomization::AddActionsRow(
		ChildBuilder,
		PCGExPropertyChoicesCustomization::ResolveOwner(PropertyHandle),
		GetHost(),
		LOCTEXT("PatchAddCurrentValue", "Add Current Value"),
		LOCTEXT("PatchAddCurrentValueToolTip", "Add this row's current value as a new choice of this importer."),
		FOnClicked::CreateSP(this, &FPCGExPropertyChoicesPatchCustomization::OnAddCurrentValue));

	// Inherited choices, except those this patch already overrides (their Items row stands for them).
	if (Patch->Mode != EPCGExChoicesPatchMode::Replace)
	{
		FPCGExPropertyChoices Inherited;
		GetInheritedChoices(Inherited);
		for (const FPCGExPropertyChoice& Choice : Inherited.Items)
		{
			const FGuid Id = Choice.Id;
			if (Patch->Items.ContainsByPredicate([&Id](const FPCGExPropertyChoice& Item) { return Item.Id == Id; }))
			{
				continue;
			}
			const bool bRemoved = Patch->RemovedIds.Contains(Id);
			const FText Label = Choice.Label.IsNone() ? LOCTEXT("UnnamedInherited", "(unnamed)") : FText::FromName(Choice.Label);

			TSharedRef<SHorizontalBox> Actions = SNew(SHorizontalBox);
			if (bRemoved)
			{
				Actions->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("RestoreChoice", "Restore"))
					.ToolTipText(LOCTEXT("RestoreChoiceToolTip", "Stop removing this inherited choice."))
					.OnClicked(this, &FPCGExPropertyChoicesPatchCustomization::OnRestoreInherited, Id)
				];
			}
			else
			{
				Actions->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("OverrideChoice", "Override"))
					.ToolTipText(LOCTEXT("OverrideChoiceToolTip", "Edit this inherited choice here: label and value, same identity."))
					.OnClicked(this, &FPCGExPropertyChoicesPatchCustomization::OnOverrideInherited, Id)
				];
				Actions->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("RemoveChoice", "Remove"))
					.ToolTipText(LOCTEXT("RemoveChoiceToolTip", "Drop this inherited choice for this importer and everything below it."))
					.OnClicked(this, &FPCGExPropertyChoicesPatchCustomization::OnRemoveInherited, Id)
				];
			}

			ChildBuilder.AddCustomRow(Label)
			            .NameContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(Label)
						.Font(IDetailLayoutBuilder::GetDetailFont())
						.ColorAndOpacity(bRemoved ? FSlateColor::UseSubduedForeground() : FSlateColor::UseForeground())
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(6, 0, 0, 0)
					[
						SNew(STextBlock)
						.Text(bRemoved ? LOCTEXT("RemovedTag", "removed") : LOCTEXT("InheritedTag", "inherited"))
						.Font(IDetailLayoutBuilder::GetDetailFontItalic())
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]
				.ValueContent()
				.MinDesiredWidth(250.0f)
				.MaxDesiredWidth(3000.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(PCGExProperties::GetValuePreviewText(FConstStructView(Choice.Value)))
						.Font(IDetailLayoutBuilder::GetDetailFontItalic())
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						Actions
					]
				];
		}
	}

	// This importer's own items: a rewrite only applies outside Inherit.
	if (Patch->Mode != EPCGExChoicesPatchMode::Inherit)
	{
		uint32 NumItems = 0;
		ItemsHandle->GetNumChildren(NumItems);
		for (uint32 i = 0; i < NumItems; ++i)
		{
			if (const TSharedPtr<IPropertyHandle> Element = ItemsHandle->GetChildHandle(i))
			{
				ChildBuilder.AddProperty(Element.ToSharedRef());
			}
		}
	}
}

FPCGExPropertyChoicesPatch* FPCGExPropertyChoicesPatchCustomization::GetPatch() const
{
	const TSharedPtr<IPropertyHandle> PatchHandle = PatchHandlePtr.Pin();
	if (!PatchHandle.IsValid())
	{
		return nullptr;
	}
	TArray<void*> RawData;
	PatchHandle->AccessRawData(RawData);
	return RawData.Num() == 1 && RawData[0] ? static_cast<FPCGExPropertyChoicesPatch*>(RawData[0]) : nullptr;
}

FStructView FPCGExPropertyChoicesPatchCustomization::GetHost() const
{
	const TSharedPtr<IPropertyHandle> PatchHandle = PatchHandlePtr.Pin();
	if (!PatchHandle.IsValid())
	{
		return FStructView();
	}
	if (const FPCGExChoicesEditContext* Context = PCGExPropertyChoicesCustomization::FindEditContext(PatchHandle.ToSharedRef()))
	{
		return Context->Host;
	}
	FPCGExPropertyOverrideEntry* Entry = PCGExPropertyChoicesCustomization::ResolveAncestorRaw<FPCGExPropertyOverrideEntry>(PatchHandle.ToSharedRef(), FPCGExPropertyOverrideEntry::StaticStruct());
	return Entry ? PCGExPropertyChoicesCustomization::ViewOf(Entry->Value) : FStructView();
}

void FPCGExPropertyChoicesPatchCustomization::GetInheritedChoices(FPCGExPropertyChoices& Out) const
{
	Out = FPCGExPropertyChoices();
	const TSharedPtr<IPropertyHandle> PatchHandle = PatchHandlePtr.Pin();
	if (!PatchHandle.IsValid())
	{
		return;
	}

	// In the popover the patch is a scratch copy: the context says where the real one lives.
	const FPCGExPropertySchemaCollection* Collection = nullptr;
	FName EntryName = NAME_None;
	const FPCGExPropertyChoicesPatch* RealPatch = nullptr;
	if (const FPCGExChoicesEditContext* Context = PCGExPropertyChoicesCustomization::FindEditContext(PatchHandle.ToSharedRef()))
	{
		Collection = Context->Collection;
		EntryName = Context->EntryName;
		RealPatch = Context->RealPatch;
	}
	else if (const FPCGExPropertyOverrideEntry* Entry = PCGExPropertyChoicesCustomization::ResolveAncestorRaw<FPCGExPropertyOverrideEntry>(PatchHandle.ToSharedRef(), FPCGExPropertyOverrideEntry::StaticStruct()))
	{
		Collection = PCGExPropertyChoicesCustomization::ResolveAncestorRaw<FPCGExPropertySchemaCollection>(PatchHandle.ToSharedRef(), FPCGExPropertySchemaCollection::StaticStruct());
		EntryName = Entry->GetPropertyName();
		RealPatch = &Entry->ChoicesPatch;
	}
	if (!Collection)
	{
		return;
	}

	TArray<FPCGExPropertyResolved> Resolved;
	Collection->Resolve(Resolved);
	for (const FPCGExPropertyResolved& Candidate : Resolved)
	{
		if (Candidate.OwningAsset && Candidate.Source && Candidate.Source->Name == EntryName)
		{
			Candidate.GetEffectiveChoices(Out, RealPatch);
			return;
		}
	}
}

FText FPCGExPropertyChoicesPatchCustomization::GetCountText() const
{
	const FStructView Host = GetHost();
	const FPCGExProperty* Property = Host.IsValid() ? Host.GetPtr<FPCGExProperty>() : nullptr;
	const int32 Num = Property ? Property->Choices.Items.Num() : 0;
	return Num == 1
		? LOCTEXT("OneEffectiveChoice", "1 effective")
		: FText::Format(LOCTEXT("EffectiveChoiceCount", "{0} effective"), FText::AsNumber(Num));
}

void FPCGExPropertyChoicesPatchCustomization::EnsureModifyMode()
{
	FPCGExPropertyChoicesPatch* Patch = GetPatch();
	if (!Patch || !ModeHandle.IsValid() || Patch->Mode != EPCGExChoicesPatchMode::Inherit)
	{
		return;
	}
	ModeHandle->NotifyPreChange();
	Patch->Mode = EPCGExChoicesPatchMode::Modify;
	ModeHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	ModeHandle->NotifyFinishedChangingProperties();
}

FReply FPCGExPropertyChoicesPatchCustomization::OnAddCurrentValue()
{
	FPCGExPropertyChoicesPatch* Patch = GetPatch();
	const FStructView Host = GetHost();
	if (!Patch || !Host.IsValid() || !ItemsHandle.IsValid())
	{
		return FReply::Handled();
	}

	FScopedTransaction Transaction(LOCTEXT("PatchAddChoice", "Add Choice"));
	EnsureModifyMode();
	const FName Label(*FString::Printf(TEXT("Choice %d"), Patch->Items.Num() + 1));
	ItemsHandle->NotifyPreChange();
	Patch->Items.Add(PCGExProperties::MakeChoiceFromValue(FConstStructView(Host), Label));
	ItemsHandle->NotifyPostChange(EPropertyChangeType::ArrayAdd);
	ItemsHandle->NotifyFinishedChangingProperties();
	RequestRebuild();
	return FReply::Handled();
}

FReply FPCGExPropertyChoicesPatchCustomization::OnOverrideInherited(const FGuid ChoiceId)
{
	FPCGExPropertyChoicesPatch* Patch = GetPatch();
	FPCGExPropertyChoices Inherited;
	GetInheritedChoices(Inherited);
	const int32 Index = PCGExProperties::FindChoiceById(Inherited, ChoiceId);
	if (!Patch || Index == INDEX_NONE || !ItemsHandle.IsValid())
	{
		return FReply::Handled();
	}

	// Same Id: the item replaces the inherited choice rather than sitting beside it.
	FScopedTransaction Transaction(LOCTEXT("PatchOverrideChoice", "Override Choice"));
	EnsureModifyMode();
	ItemsHandle->NotifyPreChange();
	Patch->Items.Add(Inherited.Items[Index]);
	ItemsHandle->NotifyPostChange(EPropertyChangeType::ArrayAdd);
	ItemsHandle->NotifyFinishedChangingProperties();
	RequestRebuild();
	return FReply::Handled();
}

FReply FPCGExPropertyChoicesPatchCustomization::OnRemoveInherited(const FGuid ChoiceId)
{
	FPCGExPropertyChoicesPatch* Patch = GetPatch();
	if (!Patch || !RemovedIdsHandle.IsValid() || Patch->RemovedIds.Contains(ChoiceId))
	{
		return FReply::Handled();
	}

	FScopedTransaction Transaction(LOCTEXT("PatchRemoveChoice", "Remove Choice"));
	EnsureModifyMode();
	RemovedIdsHandle->NotifyPreChange();
	Patch->RemovedIds.Add(ChoiceId);
	RemovedIdsHandle->NotifyPostChange(EPropertyChangeType::ArrayAdd);
	RemovedIdsHandle->NotifyFinishedChangingProperties();
	RequestRebuild();
	return FReply::Handled();
}

FReply FPCGExPropertyChoicesPatchCustomization::OnRestoreInherited(const FGuid ChoiceId)
{
	FPCGExPropertyChoicesPatch* Patch = GetPatch();
	if (!Patch || !RemovedIdsHandle.IsValid() || !Patch->RemovedIds.Contains(ChoiceId))
	{
		return FReply::Handled();
	}

	FScopedTransaction Transaction(LOCTEXT("PatchRestoreChoice", "Restore Choice"));
	RemovedIdsHandle->NotifyPreChange();
	Patch->RemovedIds.Remove(ChoiceId);
	RemovedIdsHandle->NotifyPostChange(EPropertyChangeType::ArrayRemove);
	RemovedIdsHandle->NotifyFinishedChangingProperties();
	RequestRebuild();
	return FReply::Handled();
}

void FPCGExPropertyChoicesPatchCustomization::OnPatchChanged(const FPropertyChangedEvent& InEvent)
{
	if (InEvent.ChangeType == EPropertyChangeType::Interactive)
	{
		return;
	}
	// Duplicating an item copies its Id; two items with one Id would collapse on Apply.
	if (FPCGExPropertyChoicesPatch* Patch = GetPatch())
	{
		Patch->NormalizeIds();
	}
	SyncImportRowsInPlace();
}

void FPCGExPropertyChoicesPatchCustomization::SyncImportRowsInPlace()
{
	// The host's own post-edit path re-syncs its consumer rows; this collection's import rows are only reconciled by
	// the schema customization on shape changes, so their mirror is refreshed here, in place (no reshape). In the
	// popover the edited patch is a scratch copy: the editor syncs after it commits, not before.
	const TSharedPtr<IPropertyHandle> PatchHandle = PatchHandlePtr.Pin();
	if (!PatchHandle.IsValid() || PCGExPropertyChoicesCustomization::FindEditContext(PatchHandle.ToSharedRef()))
	{
		return;
	}
	FPCGExPropertySchemaCollection* Collection = PCGExPropertyChoicesCustomization::ResolveAncestorRaw<FPCGExPropertySchemaCollection>(PatchHandle.ToSharedRef(), FPCGExPropertySchemaCollection::StaticStruct());
	if (!Collection)
	{
		return;
	}
	TArray<FPCGExPropertyResolved> Resolved;
	Collection->Resolve(Resolved);
	Collection->ImportOverrides.SyncInPlace(Resolved);
}

void FPCGExPropertyChoicesPatchCustomization::RequestRebuild()
{
	if (const TSharedPtr<IPropertyUtilities> Utils = WeakPropertyUtilities.Pin())
	{
		Utils->RequestForceRefresh();
	}
}

#pragma endregion

#undef LOCTEXT_NAMESPACE

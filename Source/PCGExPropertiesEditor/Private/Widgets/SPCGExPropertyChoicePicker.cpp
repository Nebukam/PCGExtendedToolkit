// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Widgets/SPCGExPropertyChoicePicker.h"

#include "DetailLayoutBuilder.h"
#include "Details/PCGExPropertyChoiceHost.h"
#include "Details/PCGExRawPropertyEdit.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Textures/SlateIcon.h"
#include "PCGExProperty.h"
#include "PropertyHandle.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SPCGExPropertyChoicePicker"

void SPCGExPropertyChoicePicker::Construct(const FArguments& InArgs, const TSharedRef<FPCGExChoiceRowState>& InState, const TSharedRef<IPropertyHandle>& InNotifyHandle)
{
	State = InState;
	NotifyHandle = InNotifyHandle;
	bLocked = InArgs._Locked;
	CanPick = InArgs._CanPick;
	OnGetEditContent = InArgs._OnGetEditContent;

	TSharedRef<SWidget> ButtonContent = SNullWidget::NullWidget;
	if (bLocked)
	{
		// Explicit dim, not UseSubduedForeground: the button's foreground carries the arrow's accent.
		FLinearColor Subdued = FStyleColors::Foreground.GetSpecifiedColor();
		Subdued.A *= 0.6f;

		ButtonContent = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(this, &SPCGExPropertyChoicePicker::GetSelectionLabel)
				.ColorAndOpacity(this, &SPCGExPropertyChoicePicker::GetSelectionColor)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(8, 0, 0, 0)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFontItalic())
				.Text(this, &SPCGExPropertyChoicePicker::GetSelectionPreview)
				.ColorAndOpacity(FSlateColor(Subdued))
			];
	}

	ChildSlot
	[
		SAssignNew(ComboButton, SComboButton)
		.ComboButtonStyle(FAppStyle::Get(), bLocked ? "ComboButton" : "SimpleComboButton")
		.ForegroundColor(this, &SPCGExPropertyChoicePicker::GetArrowColor)
		.HasDownArrow(true)
		.ToolTipText(this, &SPCGExPropertyChoicePicker::GetSelectionToolTip)
		.OnGetMenuContent(this, &SPCGExPropertyChoicePicker::BuildMenu)
		.ButtonContent()
		[
			ButtonContent
		]
	];
}

FText SPCGExPropertyChoicePicker::GetSelectionLabel() const
{
	const FPCGExChoiceRowState::FSnapshot& Snapshot = State->Get();
	return Snapshot.Selected == INDEX_NONE ? LOCTEXT("CustomValue", "Custom") : Snapshot.SelectedLabel;
}

FText SPCGExPropertyChoicePicker::GetSelectionPreview() const
{
	return State->Get().ValuePreview;
}

FSlateColor SPCGExPropertyChoicePicker::GetSelectionColor() const
{
	// Explicit, not UseForeground: the button's foreground carries the arrow's accent and must not bleed into the label.
	return State->Get().Selected == INDEX_NONE ? FStyleColors::Warning : FStyleColors::Foreground;
}

FSlateColor SPCGExPropertyChoicePicker::GetArrowColor() const
{
	return State->Get().bBound ? FStyleColors::Primary : FSlateColor::UseStyle();
}

FText SPCGExPropertyChoicePicker::GetSelectionToolTip() const
{
	const FPCGExChoiceRowState::FSnapshot& Snapshot = State->Get();
	if (!bLocked)
	{
		if (Snapshot.bBound)
		{
			return FText::Format(
				LOCTEXT("BoundToolTip", "Bound to '{0}': the value follows this choice and detaches when edited."),
				Snapshot.SelectedLabel);
		}
		return LOCTEXT("CustomToolTipUnlocked", "Custom value. Pick a choice to bind the value to it.");
	}
	return Snapshot.Selected == INDEX_NONE
		? LOCTEXT("CustomToolTip", "This value is not one of the choices. Pick one to align it; the value is kept as is until then.")
		: LOCTEXT("LockedToolTip", "This property is restricted to its choices.");
}

TSharedRef<SWidget> SPCGExPropertyChoicePicker::BuildMenu()
{
	FMenuBuilder MenuBuilder(/*bInShouldCloseWindowAfterMenuSelection*/ true, nullptr);

	// Read for the menu's build only: entries keep ids and texts, never a pointer into the list.
	const FStructView Host = State->GetHost().Resolve();
	const FPCGExProperty* Property = Host.GetPtr<FPCGExProperty>();
	const bool bHasChoices = Property && !Property->Choices.Items.IsEmpty();

	if (OnGetEditContent.IsBound())
	{
		MenuBuilder.AddMenuEntry(
			LOCTEXT("EditChoices", "Edit Choices..."),
			LOCTEXT("EditChoicesToolTip", "Author this property's choices: the list, the lock, and what an import inherits."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Edit"),
			FUIAction(FExecuteAction::CreateSP(this, &SPCGExPropertyChoicePicker::OpenEditor)));
		if (bHasChoices)
		{
			MenuBuilder.AddSeparator();
		}
	}

	if (!bHasChoices)
	{
		if (!OnGetEditContent.IsBound())
		{
			MenuBuilder.AddWidget(
				SNew(STextBlock).Text(LOCTEXT("NoChoices", "No choices")).Font(IDetailLayoutBuilder::GetDetailFont()),
				FText::GetEmpty());
		}
		return MenuBuilder.MakeWidget();
	}

	const int32 Selected = PCGExProperties::ResolveSelectedChoice(Host, Property->Choices);
	const TAttribute<bool> CanPickNow = CanPick;
	for (int32 i = 0; i < Property->Choices.Items.Num(); ++i)
	{
		const FPCGExPropertyChoice& Choice = Property->Choices.Items[i];
		const bool bCompatible = PCGExProperties::IsChoiceCompatible(Host, Choice);
		const bool bSelected = (i == Selected);
		const FText Preview = PCGExProperties::GetValuePreviewText(FConstStructView(Choice.Value));

		FUIAction Action(
			FExecuteAction::CreateSP(this, &SPCGExPropertyChoicePicker::PickChoice, Choice.Id),
			FCanExecuteAction::CreateLambda([bCompatible, CanPickNow]() { return bCompatible && CanPickNow.Get(); }),
			FIsActionChecked::CreateLambda([bSelected]() { return bSelected; }));

		const TSharedRef<SWidget> Row = SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(FPCGExChoiceRowState::LabelOf(Choice))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			.Padding(16, 0, 0, 0)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFontItalic())
				.Text(Preview)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];

		MenuBuilder.AddMenuEntry(
			Action,
			Row,
			NAME_None,
			bCompatible ? Preview : LOCTEXT("IncompatibleChoice", "This choice no longer matches the property's type."),
			EUserInterfaceActionType::Check);
	}

	return MenuBuilder.MakeWidget();
}

void SPCGExPropertyChoicePicker::OpenEditor()
{
	if (!OnGetEditContent.IsBound())
	{
		return;
	}
	// Pushed after the menu is gone, as a popover of the button itself: a child of the menu would die with it.
	if (ComboButton.IsValid())
	{
		ComboButton->SetIsOpen(false);
	}
	const FGeometry& Geometry = GetCachedGeometry();
	const FVector2D Location = Geometry.GetAbsolutePosition() + FVector2D(0.0f, Geometry.GetAbsoluteSize().Y);
	FSlateApplication::Get().PushMenu(
		AsShared(),
		FWidgetPath(),
		OnGetEditContent.Execute(),
		Location,
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
}

void SPCGExPropertyChoicePicker::PickChoice(const FGuid ChoiceId)
{
	// Re-resolved by Id, here and again inside the edit: the menu may have outlived the list, or the host itself.
	auto FindChoice = [this, &ChoiceId](FStructView& OutHost) -> const FPCGExPropertyChoice*
	{
		OutHost = State->GetHost().Resolve();
		const FPCGExProperty* Property = OutHost.GetPtr<FPCGExProperty>();
		const int32 Index = Property ? PCGExProperties::FindChoiceById(Property->Choices, ChoiceId) : INDEX_NONE;
		return Index != INDEX_NONE && PCGExProperties::IsChoiceCompatible(OutHost, Property->Choices.Items[Index])
			? &Property->Choices.Items[Index]
			: nullptr;
	};

	FStructView Host;
	if (!NotifyHandle.IsValid() || !CanPick.Get() || !FindChoice(Host))
	{
		return;
	}

	FPCGExRawPropertyEdit Edit(NotifyHandle.ToSharedRef());
	Edit.Commit(LOCTEXT("PickChoice", "Pick Choice"), [&FindChoice](void*)
	{
		// The pre-change notify ran in between: an owner hook may have moved things.
		FStructView Target;
		if (const FPCGExPropertyChoice* Choice = FindChoice(Target))
		{
			PCGExProperties::ApplyChoice(Target, *Choice);
		}
	});
}

#undef LOCTEXT_NAMESPACE

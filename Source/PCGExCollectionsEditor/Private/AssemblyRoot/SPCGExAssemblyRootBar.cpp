// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "AssemblyRoot/SPCGExAssemblyRootBar.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace PCGExAssemblyRootBar
{
	using EAction = PCGExAssemblyRootEditor::EAction;

	/** Display order. */
	constexpr EAction Actions[] = {EAction::PickAndAttach, EAction::AttachSelection, EAction::DetachSelection};
	constexpr int32 NumActions = UE_ARRAY_COUNT(Actions);
	static_assert(NumActions == 3, "Memo array in SPCGExAssemblyRootBar sized for three actions");

	/** Soft red for rows that break authored attachments, so they never read as benign. */
	const FLinearColor DestructiveTint(1.0f, 0.45f, 0.4f);
}

void SPCGExAssemblyRootBar::Construct(const FArguments& InArgs)
{
	BuildButtonStyle();

	const TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (const PCGExAssemblyRootEditor::EAction Action : PCGExAssemblyRootBar::Actions)
	{
		Row->AddSlot()
		   .AutoWidth()
		   .Padding(3.0f, 0.0f)
		[
			MakeActionButton(Action)
		];
	}

	ChildSlot
		// Bottom-centre, clear of the viewport's own bottom-left stats and bottom-right transform widgets.
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(0.0f, 0.0f, 0.0f, 24.0f)
		[
			Row
		];
}

void SPCGExAssemblyRootBar::BuildButtonStyle()
{
	const FLinearColor Fill(0.015f, 0.015f, 0.02f, 0.88f);
	const FLinearColor Outline(1.0f, 1.0f, 1.0f, 0.25f);
	const float OutlineWidth = 1.5f;
	const float ActiveOutlineWidth = OutlineWidth * 1.35f;

	// Height-square image size leaves the rounded box on its default half-height radius: a pill.
	const FVector2f Size(ButtonHeight, ButtonHeight);

	auto Lighten = [](const FLinearColor& Base, const float TowardWhite)
	{
		FLinearColor Result = FMath::Lerp(Base, FLinearColor::White, TowardWhite);
		Result.A = Base.A;
		return Result;
	};
	auto Alpha = [](const FLinearColor& Base, const float Scale)
	{
		FLinearColor Result = Base;
		Result.A = FMath::Clamp(Base.A * Scale, 0.0f, 1.0f);
		return Result;
	};

	ButtonStyle.SetNormal(FSlateRoundedBoxBrush(Fill, Outline, OutlineWidth, Size));
	ButtonStyle.SetHovered(FSlateRoundedBoxBrush(Lighten(Fill, 0.18f), Alpha(Outline, 2.2f), ActiveOutlineWidth, Size));
	ButtonStyle.SetPressed(FSlateRoundedBoxBrush(Lighten(Fill, 0.30f), Alpha(Outline, 2.6f), ActiveOutlineWidth, Size));
	ButtonStyle.SetDisabled(FSlateRoundedBoxBrush(Alpha(Fill, 0.6f), Alpha(Outline, 0.5f), OutlineWidth, Size));
	ButtonStyle.SetNormalPadding(FMargin(0.0f));
	ButtonStyle.SetPressedPadding(FMargin(0.0f));
}

bool SPCGExAssemblyRootBar::CanExecute(const PCGExAssemblyRootEditor::EAction Action) const
{
	if (MemoFrame != GFrameCounter)
	{
		MemoFrame = GFrameCounter;
		const PCGExAssemblyRootEditor::FSelectionContext Context = PCGExAssemblyRootEditor::GatherSelection();
		for (int32 i = 0; i < PCGExAssemblyRootBar::NumActions; i++)
		{
			bMemoCanExecute[i] = PCGExAssemblyRootEditor::CanExecuteAction(PCGExAssemblyRootBar::Actions[i], Context);
		}
	}

	for (int32 i = 0; i < PCGExAssemblyRootBar::NumActions; i++)
	{
		if (PCGExAssemblyRootBar::Actions[i] == Action) { return bMemoCanExecute[i]; }
	}
	return false;
}

TSharedRef<SWidget> SPCGExAssemblyRootBar::MakeActionButton(const PCGExAssemblyRootEditor::EAction Action)
{
	const FLinearColor ActiveTint = PCGExAssemblyRootEditor::IsDestructiveAction(Action) ? PCGExAssemblyRootBar::DestructiveTint : FLinearColor::White;

	// Slate greys the button's own brush from the style, but not the icon and label inside it.
	const TAttribute<FSlateColor> ContentTint = TAttribute<FSlateColor>::CreateLambda([this, Action, ActiveTint]()
	{
		return FSlateColor(CanExecute(Action) ? ActiveTint : ActiveTint.CopyWithNewOpacity(0.35f));
	});

	return SNew(SBox)
		.HeightOverride(ButtonHeight)
		[
			SNew(SButton)
			.ButtonStyle(&ButtonStyle)
			.ContentPadding(FMargin(10.0f, 0.0f))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			// Never take focus from the level viewport.
			.IsFocusable(false)
			.IsEnabled_Lambda([this, Action]() { return CanExecute(Action); })
			.ToolTipText(PCGExAssemblyRootEditor::GetActionTooltip(Action))
			.OnClicked_Lambda([Action]()
			{
				// Re-gathered on click: the memo only drives presentation.
				PCGExAssemblyRootEditor::ExecuteAction(Action, PCGExAssemblyRootEditor::GatherSelection());
				return FReply::Handled();
			})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(PCGExAssemblyRootEditor::GetActionIcon(Action).GetIcon())
					.ColorAndOpacity(ContentTint)
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(STextBlock)
					.Text(PCGExAssemblyRootEditor::GetActionLabel(Action))
					.ColorAndOpacity(ContentTint)
				]
			]
		];
}

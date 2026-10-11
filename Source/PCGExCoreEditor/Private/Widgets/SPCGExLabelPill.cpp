// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Widgets/SPCGExLabelPill.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Layout/Margin.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"

void SPCGExLabelPill::Construct(const FArguments& InArgs)
{
	// White on purpose: the border tint alone decides the fill, so this one brush serves every colour.
	static const FSlateRoundedBoxBrush PillBrush(FLinearColor::White, 4.0f);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(&PillBrush)
		.BorderBackgroundColor(InArgs._Tint)
		.Padding(FMargin(4.0f, 0.0f))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(InArgs._Label)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 6))
			.ColorAndOpacity(InArgs._TextColor)
		]
	];
}

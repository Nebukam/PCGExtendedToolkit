// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "CoreMinimal.h"
#include "Math/Color.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

namespace PCGExLabelPill
{
	inline constexpr FLinearColor DefaultTint(0.08f, 0.08f, 0.08f);
	inline constexpr FLinearColor DefaultTextColor(0.55f, 0.55f, 0.55f);
}

/** Small rounded label (Bold 6). One shared white brush serves every pill: the fill colour is a tint. */
class PCGEXCOREEDITOR_API SPCGExLabelPill : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPCGExLabelPill)
			: _Tint(PCGExLabelPill::DefaultTint)
			  , _TextColor(PCGExLabelPill::DefaultTextColor)
		{
		}

		SLATE_ATTRIBUTE(FText, Label)
		SLATE_ATTRIBUTE(FSlateColor, Tint)
		SLATE_ATTRIBUTE(FSlateColor, TextColor)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
};

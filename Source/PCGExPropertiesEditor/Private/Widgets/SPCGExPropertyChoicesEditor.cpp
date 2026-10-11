// Copyright 2026 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#include "Widgets/SPCGExPropertyChoicesEditor.h"

#include "DetailsViewArgs.h"
#include "Details/PCGExPropertyChoicesCustomization.h"
#include "Framework/Application/SlateApplication.h"
#include "IDetailsView.h"
#include "IStructureDetailsView.h"
#include "Misc/TransactionObjectEvent.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SPCGExPropertyChoicesEditor"

void SPCGExPropertyChoicesEditor::Construct(const FArguments& InArgs)
{
	Session = InArgs._Session;
	if (!Session.IsValid() || !Session->IsValid())
	{
		// Several objects selected, or a row whose data moved since it was built.
		ChildSlot
		[
			SNew(SBox)
			.Padding(8.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CannotEdit", "Choices can only be edited on a single, up to date row."))
			]
		];
		return;
	}

	FDetailsViewArgs DetailsArgs;
	DetailsArgs.bAllowSearch = false;
	DetailsArgs.bHideSelectionTip = true;
	DetailsArgs.bShowOptions = false;
	DetailsArgs.bShowScrollBar = true;
	DetailsArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	// Created empty: the session-bound layouts have to be on the view before it builds a row.
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	View = PropertyModule.CreateStructureDetailView(DetailsArgs, FStructureDetailsViewArgs(), TSharedPtr<FStructOnScope>());
	if (IDetailsView* DetailsView = View->GetDetailsView())
	{
		PCGExPropertyChoicesUI::RegisterSessionLayouts(*DetailsView, Session.ToSharedRef());
	}
	View->SetStructureData(Session->GetScratch());
	View->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &SPCGExPropertyChoicesEditor::OnFinishedChanging);

	Session->OnScratchChanged.AddSP(this, &SPCGExPropertyChoicesEditor::RequestRebuild);
	TransactedHandle = FCoreUObjectDelegates::OnObjectTransacted.AddSP(this, &SPCGExPropertyChoicesEditor::OnObjectTransacted);
	// The opener's panel can rebuild under the popover for reasons nothing reports here.
	RegisterActiveTimer(0.25f, FWidgetActiveTimerDelegate::CreateSP(this, &SPCGExPropertyChoicesEditor::CheckSession));

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredWidth(560.0f)
		.MaxDesiredHeight(480.0f)
		.Padding(4.0f)
		[
			View->GetWidget().ToSharedRef()
		]
	];
}

SPCGExPropertyChoicesEditor::~SPCGExPropertyChoicesEditor()
{
	FCoreUObjectDelegates::OnObjectTransacted.Remove(TransactedHandle);
}

void SPCGExPropertyChoicesEditor::OnFinishedChanging(const FPropertyChangedEvent& InEvent)
{
	Session->Commit(LOCTEXT("EditChoices", "Edit Choices"));
}

void SPCGExPropertyChoicesEditor::OnObjectTransacted(UObject* Object, const FTransactionObjectEvent& Event)
{
	if (bResyncPending || Event.GetEventType() != ETransactionObjectEventType::UndoRedo || Object != Session->GetOwner())
	{
		return;
	}

	// Not now: the opener's own undo handler may be about to rebuild the panel the session's handles belong to.
	bResyncPending = true;
	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float) -> EActiveTimerReturnType
	{
		bResyncPending = false;
		if (!Session->IsValid())
		{
			Dismiss();
		}
		else if (Session->PullFromReal())
		{
			RequestRebuild();
		}
		return EActiveTimerReturnType::Stop;
	}));
}

void SPCGExPropertyChoicesEditor::RequestRebuild()
{
	if (bRebuildPending)
	{
		return;
	}
	// Rows alias the scratch's element memory: they are rebuilt before the next paint, outside whichever callback
	// of theirs led here.
	bRebuildPending = true;
	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float) -> EActiveTimerReturnType
	{
		bRebuildPending = false;
		if (View.IsValid() && View->GetDetailsView())
		{
			View->GetDetailsView()->ForceRefresh();
		}
		return EActiveTimerReturnType::Stop;
	}));
}

EActiveTimerReturnType SPCGExPropertyChoicesEditor::CheckSession(double InCurrentTime, float InDeltaTime)
{
	if (Session->IsValid())
	{
		return EActiveTimerReturnType::Continue;
	}
	Dismiss();
	return EActiveTimerReturnType::Stop;
}

void SPCGExPropertyChoicesEditor::Dismiss()
{
	FSlateApplication::Get().DismissMenuByWidget(AsShared());
}

#undef LOCTEXT_NAMESPACE

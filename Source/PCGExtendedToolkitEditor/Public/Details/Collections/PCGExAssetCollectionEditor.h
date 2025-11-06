// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Toolkits/AssetEditorToolkit.h"

class UPCGExAssetCollection;

struct FPCGExDetailsTabInfos
{
	FPCGExDetailsTabInfos() = default;

	FPCGExDetailsTabInfos(const FName InId, const TSharedPtr<IDetailsView>& InView, const FName InLabel = NAME_None, const ETabRole InRole = MajorTab)
		: Id(InId), View(InView), Label(InLabel.IsNone() ? InId : InLabel), Role(InRole)
	{
	}

	FName Id = NAME_None;
	TSharedPtr<IDetailsView> View = nullptr;
	FName Label = NAME_None;
	ETabRole Role = MajorTab;
	FString Icon = TEXT("");
};

class FPCGExAssetCollectionEditor : public FAssetEditorToolkit
{
public:
	virtual void InitEditor(UPCGExAssetCollection* InCollection, const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost);
	virtual UPCGExAssetCollection* GetEditedCollection() const;

	virtual FName GetToolkitFName() const override { return FName("PCGExAssetCollectionEditor"); }
	virtual FText GetBaseToolkitName() const override { return INVTEXT("PCGEx Collection Editor"); }
	virtual FString GetWorldCentricTabPrefix() const override { return TEXT("PCGEx"); }
	virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor::White; }

protected:
	TWeakObjectPtr<UPCGExAssetCollection> EditedCollection;

	virtual void CreateTabs(TArray<FPCGExDetailsTabInfos>& OutTabs);
	virtual void FillToolbar(FToolBarBuilder& ToolbarBuilder);
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

	TArray<FPCGExDetailsTabInfos> Tabs;
};

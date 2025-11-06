// Copyright 2025 Timothé Lapetite and contributors
// Released under the MIT license https://opensource.org/license/MIT/

#pragma once

#include "Toolkits/AssetEditorToolkit.h"

class UPCGExAssetCollection;

class FPCGExAssetCollectionEditor : public FAssetEditorToolkit
{
public:
	TWeakObjectPtr<UPCGExAssetCollection> EditedCollection;

	void InitEditor(UPCGExAssetCollection* InCollection, const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost);
	UPCGExAssetCollection* GetEditedCollection() const;

	virtual FName GetToolkitFName() const override { return FName("PCGExAssetCollectionEditor"); }
	virtual FText GetBaseToolkitName() const override { return INVTEXT("PCGEx Collection Editor"); }
	virtual FString GetWorldCentricTabPrefix() const override { return TEXT("PCGEx"); }
	virtual FLinearColor GetWorldCentricTabColorScale() const override { return FLinearColor::White; }

protected:
	const FName DetailsViewTabId = FName("Details");

	virtual void FillToolbar(FToolBarBuilder& ToolbarBuilder);
	virtual void RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager) override;

	TSharedPtr<IDetailsView> DetailsView;
};

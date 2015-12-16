// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SSceneImportNodeTreeView.h"
#include "SSceneImportStaticMeshListView.h"

struct FTreeNodeValue
{
public:
	FbxNodeInfoPtr CurrentNode;
	FbxNodeInfoPtr OriginalNode;
};

class SFbxReimportSceneTreeView : public STreeView<FbxNodeInfoPtr>
{
public:
	~SFbxReimportSceneTreeView();
	SLATE_BEGIN_ARGS(SFbxReimportSceneTreeView)
	: _SceneInfo(nullptr)
	, _SceneInfoOriginal(nullptr)
	, _NodeStatusMap(nullptr)
	{}
		SLATE_ARGUMENT(TSharedPtr<FFbxSceneInfo>, SceneInfo)
		SLATE_ARGUMENT(TSharedPtr<FFbxSceneInfo>, SceneInfoOriginal)
		SLATE_ARGUMENT(FbxSceneReimportStatusMapPtr, NodeStatusMap)
	SLATE_END_ARGS()
	
	/** Construct this widget */
	void Construct(const FArguments& InArgs);
	TSharedRef< ITableRow > OnGenerateRowFbxSceneTreeView(FbxNodeInfoPtr Item, const TSharedRef< STableViewBase >& OwnerTable);
	void OnGetChildrenFbxSceneTreeView(FbxNodeInfoPtr InParent, TArray< FbxNodeInfoPtr >& OutChildren);

	void OnToggleSelectAll(ECheckBoxState CheckType);
	FReply OnExpandAll();
	FReply OnCollapseAll();
protected:
	TSharedPtr<FFbxSceneInfo> SceneInfo;
	TSharedPtr<FFbxSceneInfo> SceneInfoOriginal;
	FbxSceneReimportStatusMapPtr NodeStatusMap;


	/** the elements we show in the tree view */
	TArray<FbxNodeInfoPtr> FbxRootNodeArray;

	/** Open a context menu for the current selection */
	TSharedPtr<SWidget> OnOpenContextMenu();
	void AddSelectionToImport();
	void RemoveSelectionFromImport();
	void SetSelectionImportState(bool MarkForImport);
	void OnSelectionChanged(FbxNodeInfoPtr Item, ESelectInfo::Type SelectionType);

	void GotoAsset(TSharedPtr<FFbxAttributeInfo> AssetAttribute);
	void RecursiveSetImport(FbxNodeInfoPtr NodeInfoPtr, bool ImportStatus);

	// Internal structure and function to create the tree view status data
	TMap<FbxNodeInfoPtr, TSharedPtr<FTreeNodeValue>> NodeTreeData;
};
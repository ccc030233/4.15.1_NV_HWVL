// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Animation/PoseAsset.h"
#include "PreviewScene.h"

class FPoseAssetDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

	virtual ~FPoseAssetDetails();

private:
 	TWeakObjectPtr<UPoseAsset> PoseAsset;
	TWeakObjectPtr<USkeleton> TargetSkeleton;

	// retarget source handler
	TSharedPtr<IPropertyHandle> RetargetSourceNameHandler;

	TSharedPtr<class SComboBox< TSharedPtr<FString> > > RetargetSourceComboBox;
	TArray< TSharedPtr< FString > >						RetargetSourceComboList;

	TSharedRef<SWidget> MakeRetargetSourceComboWidget( TSharedPtr<FString> InItem );
	void OnRetargetSourceChanged( TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo  );
	FText GetRetargetSourceComboBoxContent() const;
	FText GetRetargetSourceComboBoxToolTip() const;
	void OnRetargetSourceComboOpening();
	TSharedPtr<FString> GetRetargetSourceString(FName RetargetSourceName) const;

	USkeleton::FOnRetargetSourceChanged OnDelegateRetargetSourceChanged;
	FDelegateHandle OnDelegateRetargetSourceChangedDelegateHandle;
	void RegisterRetargetSourceChanged();
	void DelegateRetargetSourceChanged();

	// additive setting
	void OnAdditiveToggled(ECheckBoxState NewCheckedState);
	ECheckBoxState IsAdditiveChecked() const;

	// base pose
	TSharedPtr<class SComboBox< TSharedPtr<FString> > > BasePoseComboBox;
	TArray< TSharedPtr< FString > >						BasePoseComboList;

	TSharedRef<SWidget> MakeBasePoseComboWidget(TSharedPtr<FString> InItem);
	void OnBasePoseChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	FText GetBasePoseComboBoxContent() const;
	FText GetBasePoseComboBoxToolTip() const;
	void OnBasePoseComboOpening();
	TSharedPtr<FString> GetBasePoseString(int32 InBasePoseIndex) const;
	bool CanSelectBasePose() const;
	UPoseAsset::FOnPoseListChanged OnDelegatePoseListChanged;
	FDelegateHandle OnDelegatePoseListChangedDelegateHandle;
	void RegisterBasePoseChanged();
	void RefreshBasePoseChanged();

	bool bCachedAdditive;
	int32 CachedBasePoseIndex;
	void CachePoseAssetData();

	bool CanApplySettings() const;
	FReply OnApplyAdditiveSettings();

	TSharedPtr<IPropertyHandle> SourceAnimationPropertyHandle;
	FReply OnUpdatePoseSourceAnimation();

	FText GetButtonText() const;
};


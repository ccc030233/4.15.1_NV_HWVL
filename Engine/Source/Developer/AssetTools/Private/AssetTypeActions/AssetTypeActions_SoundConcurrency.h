// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class FAssetTypeActions_SoundConcurrency : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundConcurrency", "Sound Concurrency"); }
	virtual FColor GetTypeColor() const override { return FColor(77, 100, 139); }
	virtual UClass* GetSupportedClass() const override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
};
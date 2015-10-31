// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class ISequencerSection;
class FTrackNode;


class FSequencerSectionLayoutBuilder
	: public ISectionLayoutBuilder
{
public:
	FSequencerSectionLayoutBuilder( TSharedRef<FTrackNode> InRootNode );

public:

	// ISectionLayoutBuilder interface

	virtual void PushCategory( FName CategoryName, const FText& DisplayLabel ) override;
	virtual void SetSectionAsKeyArea( TSharedRef<IKeyArea> KeyArea ) override;
	virtual void AddKeyArea( FName KeyAreaName, const FText& DisplayName, TSharedRef<IKeyArea> KeyArea ) override;
	virtual void PopCategory() override;

private:

	/** Root node of the tree */
	TSharedRef<FTrackNode> RootNode;

	/** The current node that other nodes are added to */
	TSharedRef<FSequencerDisplayNode> CurrentNode;
};
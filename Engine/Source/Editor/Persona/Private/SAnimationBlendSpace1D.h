// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SAnimationBlendSpaceBase.h"
#include "Sorting.h"

//////////////////////////////////////////////////////////////////////////
// SAnimationBlendSpace1D

// delegate to refresh sample list
DECLARE_DELEGATE( FOnRefreshSamples )

struct FIndexLinePoint
{
	FVector Point;
	int32	Index;
	FIndexLinePoint(FVector& InPoint, int32 InIndex) : Point(InPoint), Index(InIndex) {}
};

struct FLineElement
{
	enum ELine
	{

	};
	const FIndexLinePoint	Start;
	const FIndexLinePoint	End;
	const bool				bIsFirst;
	const bool				bIsLast;

	float Range;

	FLineElement(const FIndexLinePoint& InStart, const FIndexLinePoint& InEnd, bool bInIsFirst, bool bInIsLast) : Start(InStart), End(InEnd), bIsFirst(bInIsFirst), bIsLast(bInIsLast)
	{
		Range = End.Point.X - Start.Point.X;
	}

	bool PopulateElement(float ElementPosition, FEditorElement& Element) const;

	bool IsBlendInputOnLine(const FVector& BlendInput) const
	{
		return (BlendInput.X >= Start.Point.X) && (BlendInput.X <= End.Point.X);
	}
};

/** Generates a line list between the supplied sample points to
	aid blend space sample generation */
class FLineElementGenerator
{

public:

	void Init(float InStartOfEditorRange, float InEndOfEditorRange, float InNumEditorPoints)
	{
		SamplePointList.Reset();
		StartOfEditorRange = InStartOfEditorRange;
		EndOfEditorRange = InEndOfEditorRange;
		NumEditorPoints = InNumEditorPoints;
	}

	void AddSamplePoint(FVector NewPoint) {SamplePointList.Add(NewPoint);}

	/** Populates EditorElements based on the Sample points previously supplied to AddSamplePoint */
	void CalculateEditorElements();

	const FLineElement* GetLineElementForBlendInput(const FVector& BlendInput) const;
	/**
	 * Data Structure for line generation
	 * SamplePointList is the input data
	 */
	TArray<FVector>		SamplePointList;

	/** Editor elements generated by CalculateEditorElements */
	TArray<FEditorElement> EditorElements;

private:

	/** Defines the range of the editor */
	float StartOfEditorRange;
	float EndOfEditorRange;

	/** Number of points that we have to generate FEditorElements for */
	int32 NumEditorPoints;

	TArray<FLineElement> LineElements;
};

/** Widget with a handler for OnPaint; convenient for testing various DrawPrimitives. */
class SBlendSpace1DWidget : public SBlendSpaceWidget
{
public:
	SLATE_BEGIN_ARGS(SBlendSpace1DWidget)
		: _PreviewInput()
		, _BlendSpace1D(NULL)
		{}

		/** allow adding point using mouse **/
		SLATE_ATTRIBUTE(FVector, PreviewInput)
		SLATE_EVENT(FOnRefreshSamples, OnRefreshSamples)
		SLATE_ARGUMENT(UBlendSpace1D*, BlendSpace1D)
		SLATE_EVENT(FOnNotifyUser, OnNotifyUser)
	SLATE_END_ARGS()

	/**
	 * Construct this widget
	 *
	 * @param	InArgs	The declaration data for this widget
	 */
	 void Construct(const FArguments& InArgs);

	virtual int32 OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyClippingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const override;

	/**
	 * A Panel's desired size in the space required to arrange of its children on the screen while respecting all of
	 * the children's desired sizes and any layout-related options specified by the user. See StackPanel for an example.
	 *
	 * @return The desired size.
	 */
	FVector2D ComputeDesiredSize() const override;

	virtual void ResampleData() override;

	/** Preview point that is fed into the blend space to generate output */
	TAttribute<FVector>	PreviewInput;

	/** 
	 * Mapping function between WidgetPos and GridPos
	 */
	virtual TOptional<FVector2D>	GetWidgetPosFromEditorPos(const FVector& EditorPos, const FSlateRect& WindowRect) const override;
	virtual TOptional<FVector>		GetEditorPosFromWidgetPos(const FVector2D & WidgetPos, const FSlateRect& WindowRect) const override;

	/**
	 * Snaps a position in editor space to the editor grid
	 */
	virtual FVector					SnapEditorPosToGrid(const FVector& InPos) const override;

protected:
	/** Utility functions **/
	virtual FText GetInputText(const FVector& GridPos) const override;
	virtual FReply UpdateLastMousePosition( const FGeometry& MyGeometry, const FVector2D& ScreenSpacePosition, bool bClampToWindowRect = false, bool bSnapToGrid = false  ) override;

private:

	/** Generates editor elements */
	FLineElementGenerator ElementGenerator;

	/** Get derived blend space from base class pointer */
	UBlendSpace1D* GetBlendSpace() { return Cast<UBlendSpace1D>(BlendSpace); }
	const UBlendSpace1D* GetBlendSpace() const { return Cast<UBlendSpace1D>(BlendSpace); }
};

class SBlendSpaceEditor1D : public SBlendSpaceEditorBase
{
public:
	SLATE_BEGIN_ARGS(SBlendSpaceEditor1D)
		: _BlendSpace1D(NULL)			
		, _Persona()
		{}

		SLATE_ARGUMENT(UBlendSpace1D*, BlendSpace1D)
		SLATE_ARGUMENT(TSharedPtr<class FPersona>, Persona)
	SLATE_END_ARGS()

	~SBlendSpaceEditor1D();

	void Construct(const FArguments& InArgs);

protected:

	virtual TSharedRef<SWidget> MakeDisplayOptionsBox() const override;

private:

	// Property changed delegate
	FCoreDelegates::FOnObjectPropertyChanged::FDelegate OnPropertyChangedHandle;
	void OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent);

	// Updates the UI to reflect the current blend space parameter values */
	void UpdateBlendParameters();

	/** Handler for when blend space parameters have been changed */
	void OnBlendSpaceParamtersChanged();

	/** Handle display editor vertically checkbox */
	ESlateCheckBoxState::Type IsEditorDisplayedVertically() const;
	void OnChangeDisplayedVertically( ESlateCheckBoxState::Type NewValue );

	/** Get the blend space object we are editing cast to 1D */
	UBlendSpace1D* GetBlendSpace() {return Cast<UBlendSpace1D>(BlendSpace);}
	const UBlendSpace1D* GetBlendSpace() const {return Cast<UBlendSpace1D>(BlendSpace);}

	/** Returns height of options box to control */
	FOptionalSize GetHeightForOptionsBox() const;

	/** Builds the main editor panel, handles horizontal or vertical display modes */
	void BuildEditorPanel(bool bForce = false);
	
	/* Slot that main editor interface is built in, is repopulated when layout is changed */
	SOverlay::FOverlaySlot* EditorSlot;

	/** Display options box, created in Construct to reduce work needed to be done by BuildEditorPanel */
	TSharedPtr<SWidget> DisplayOptionsPanel;

	/** UI components for displaying the axis labels. */
	TSharedPtr<STextBlock> Parameter_Min;
	TSharedPtr<STextBlock> Parameter_Max;

	/** Cache which orientation we have built our editor panel in */
	bool bCachedDisplayVerticalValue;
};


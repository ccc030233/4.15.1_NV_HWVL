// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Anchors.generated.h"

/**
 * Describes how a widget is anchored.
 */
USTRUCT()
struct FAnchors
{
	GENERATED_USTRUCT_BODY()

	/** Holds the minimum anchors, left + top. */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FVector2D Minimum;

	/** Holds the maximum anchors, right + bottom. */
	UPROPERTY(EditAnywhere, Category=Appearance)
	FVector2D Maximum;

public:

	/**
	 * Default constructor.
	 *
	 * The default margin size is zero on all four sides..
	 */
	FAnchors()
		: Minimum(0.0f, 0.0f)
		, Maximum(0.0f, 0.0f)
	{ }

	/** Construct a Anchors with uniform space on all sides */
	FAnchors(float UnifromAnchors)
		: Minimum(UnifromAnchors, UnifromAnchors)
		, Maximum(UnifromAnchors, UnifromAnchors)
	{ }

	/** Construct a Anchors where Horizontal describes Left and Right spacing while Vertical describes Top and Bottom spacing */
	FAnchors(float Horizontal, float Vertical)
		: Minimum(Horizontal, Vertical)
		, Maximum(Horizontal, Vertical)
	{ }

	/** Construct Anchors where the spacing on each side is individually specified. */
	FAnchors(float InLeft, float InTop, float InRight, float InBottom)
		: Minimum(InLeft, InTop)
		, Maximum(InRight, InBottom)
	{ }
};

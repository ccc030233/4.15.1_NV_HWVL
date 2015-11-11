// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "IntegralKeyArea.h"


/* IKeyArea interface
 *****************************************************************************/

TArray<FKeyHandle> FIntegralCurveKeyAreaBase::AddKeyUnique(float Time, EMovieSceneKeyInterpolation InKeyInterpolation, float TimeToCopyFrom)
{
	TArray<FKeyHandle> AddedKeyHandles;
	FKeyHandle CurrentKey = Curve.FindKey(Time);

	if (Curve.IsKeyHandleValid(CurrentKey) == false)
	{
		if (OwningSection->GetStartTime() > Time)
		{
			OwningSection->SetStartTime(Time);
		}

		if (OwningSection->GetEndTime() < Time)
		{
			OwningSection->SetEndTime(Time);
		}

		EvaluateAndAddKey(Time, TimeToCopyFrom, CurrentKey);
		AddedKeyHandles.Add(CurrentKey);
	}

	return AddedKeyHandles;
}


void FIntegralCurveKeyAreaBase::DeleteKey(FKeyHandle KeyHandle)
{
	Curve.DeleteKey(KeyHandle);
}


ERichCurveExtrapolation FIntegralCurveKeyAreaBase::GetExtrapolationMode(bool bPreInfinity) const
{
	return RCCE_None;
}


ERichCurveTangentMode FIntegralCurveKeyAreaBase::GetKeyTangentMode(FKeyHandle KeyHandle) const
{
	return RCTM_None;
}


ERichCurveInterpMode FIntegralCurveKeyAreaBase::GetKeyInterpMode(FKeyHandle KeyHandle) const
{
	return RCIM_None;
}


UMovieSceneSection* FIntegralCurveKeyAreaBase::GetOwningSection()
{
	return OwningSection;
}


float FIntegralCurveKeyAreaBase::GetKeyTime(FKeyHandle KeyHandle) const
{
	return Curve.GetKeyTime(KeyHandle);
}


FRichCurve* FIntegralCurveKeyAreaBase::GetRichCurve()
{
	return nullptr;
};


TArray<FKeyHandle> FIntegralCurveKeyAreaBase::GetUnsortedKeyHandles() const
{
	TArray<FKeyHandle> OutKeyHandles;

	for (auto It(Curve.GetKeyHandleIterator()); It; ++It)
	{
		OutKeyHandles.Add(It.Key());
	}

	return OutKeyHandles;
}


FKeyHandle FIntegralCurveKeyAreaBase::MoveKey(FKeyHandle KeyHandle, float DeltaPosition)
{
	return Curve.SetKeyTime(KeyHandle, Curve.GetKeyTime(KeyHandle) + DeltaPosition);
}


void FIntegralCurveKeyAreaBase::SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity)
{
	// do nothing
}


void FIntegralCurveKeyAreaBase::SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode InterpMode)
{
	// do nothing
}


void FIntegralCurveKeyAreaBase::SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode TangentMode)
{
	// do nothing
}


void FIntegralCurveKeyAreaBase::SetKeyTime(FKeyHandle KeyHandle, float NewKeyTime) const
{
	Curve.SetKeyTime(KeyHandle, NewKeyTime);
}
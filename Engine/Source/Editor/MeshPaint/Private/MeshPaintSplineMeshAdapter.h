// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MeshPaintModule.h"
#include "MeshPaintStaticMeshAdapter.h"

//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForSplineMeshes

class FMeshPaintGeometryAdapterForSplineMeshes : public FMeshPaintGeometryAdapterForStaticMeshes
{
public:
	virtual bool InitializeMeshData() override;
	virtual FVector GetMeshVertex(int32 Index) const override;

protected:
	TArray<FVector> MeshVertices;
};

//////////////////////////////////////////////////////////////////////////
// FMeshPaintGeometryAdapterForSplineMeshesFactory

class FMeshPaintGeometryAdapterForSplineMeshesFactory : public FMeshPaintGeometryAdapterForStaticMeshesFactory
{
public:
	virtual TSharedPtr<IMeshPaintGeometryAdapter> Construct(class UMeshComponent* InComponent, int32 InPaintingMeshLODIndex, int32 InUVChannelIndex) const override;
};
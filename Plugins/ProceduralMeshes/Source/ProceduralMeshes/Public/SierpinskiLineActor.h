// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example Sierpinski pyramid using cylinder lines

#pragma once

#include "GameFramework/Actor.h"
#include "RuntimeMeshComponent.h"
#include "SierpinskiLineActor.generated.h"

// A simple struct to keep some data together
USTRUCT()
struct FPyramidLine
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Start;

	UPROPERTY()
	FVector End;

	UPROPERTY()
	float Width;

	FPyramidLine()
	{
		Start = FVector::ZeroVector;
		End = FVector::ZeroVector;
		Width = 1.0f;
	}

	FPyramidLine(FVector InStart, FVector InEnd)
	{
		Start = InStart;
		End = InEnd;
		Width = 1.0f;
	}

	FPyramidLine(FVector InStart, FVector InEnd, float InWidth)
	{
		Start = InStart;
		End = InEnd;
		Width = InWidth;
	}
};

UCLASS()
class PROCEDURALMESHES_API ASierpinskiLineActor : public AActor
{
	GENERATED_BODY()

public:
	ASierpinskiLineActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float Size = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	int32 Iterations = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float LineThickness = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float ThicknessMultiplierPerGeneration = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	int32 RadialSegmentCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	bool bSmoothNormals = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	UMaterialInterface* Material;

	virtual void PostLoad() override;
	virtual void PostActorCreated() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

protected:

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Default)
	USceneComponent* RootNode;
	
	UPROPERTY()
	URuntimeMeshComponent* MeshComponent;

private:
	void GenerateMesh();

	UPROPERTY(Transient)
	TArray<FPyramidLine> Lines;

	void GenerateLines();
	void AddSection(FVector InBottomLeftPoint, FVector InTopPoint, FVector InBottomRightPoint, FVector InBottomMiddlePoint, int32 InDepth);
	void GenerateCylinder(TArray<FRuntimeMeshVertexSimple>& InVertices, TArray<int32>& InTriangles, FVector StartPoint, FVector EndPoint, float InWidth, int32 InCrossSectionCount, int32& VertexIndex, int32& TriangleIndex, bool bInSmoothNormals = true);

	FVector RotatePointAroundPivot(FVector InPoint, FVector InPivot, FVector InAngles);
	void PreCacheCrossSection();

	int32 LastCachedCrossSectionCount;
	UPROPERTY(Transient)
	TArray<FVector> CachedCrossSectionPoints;

	// Mesh buffers
	void SetupMeshBuffers();
	bool bHaveBuffersBeenInitialized = false;
	TArray<FRuntimeMeshVertexSimple> Vertices;
	TArray<int32> Triangles;
};

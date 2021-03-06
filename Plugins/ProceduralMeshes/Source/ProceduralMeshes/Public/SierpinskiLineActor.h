// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example Sierpinski pyramid using cylinder lines

#pragma once

#include "CoreMinimal.h"
#include "RuntimeMeshActor.h"
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
class PROCEDURALMESHES_API ASierpinskiLineActor : public ARuntimeMeshActor
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

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostLoad() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	URuntimeMeshProviderStatic* StaticProvider;

private:
	void GenerateMesh();

	UPROPERTY(Transient)
	TArray<FPyramidLine> Lines;

	void GenerateLines();
	void AddSection(FVector InBottomLeftPoint, FVector InTopPoint, FVector InBottomRightPoint, FVector InBottomMiddlePoint, int32 InDepth);
	void GenerateCylinder(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FRuntimeMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, const FVector StartPoint, const FVector EndPoint, const float InWidth, const int32 InCrossSectionCount, int32& VertexIndex, int32& TriangleIndex, const bool bInSmoothNormals = true);

	static FVector RotatePointAroundPivot(const FVector InPoint, const FVector InPivot, const FVector InAngles);
	void PreCacheCrossSection();

	int32 LastCachedCrossSectionCount;
	UPROPERTY(Transient)
	TArray<FVector> CachedCrossSectionPoints;

	// Mesh buffers
	void SetupMeshBuffers();
	UPROPERTY(Transient)
	TArray<FVector> Positions;
	UPROPERTY(Transient)
	TArray<int32> Triangles;
	UPROPERTY(Transient)
	TArray<FVector> Normals;
	UPROPERTY(Transient)
	TArray<FRuntimeMeshTangent> Tangents;
	UPROPERTY(Transient)
	TArray<FVector2D> TexCoords;
};

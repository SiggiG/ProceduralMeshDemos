// Copyright Sigurdur Gunnarsson. All Rights Reserved.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
// Example branching lines using cylinder strips

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RuntimeProceduralMeshComponent.h"
#include "BranchingLinesActor.generated.h"

// A simple struct to keep some data together
USTRUCT()
struct FBranchSegment
{
	GENERATED_BODY()

	UPROPERTY()
	FVector Start;

	UPROPERTY()
	FVector End;

	UPROPERTY()
	float Width;

	UPROPERTY()
	int8 ForkGeneration;

	FBranchSegment()
	{
		Start = FVector::ZeroVector;
		End = FVector::ZeroVector;
		Width = 1.f;
		ForkGeneration = 0;
	}

	FBranchSegment(FVector start, FVector end)
	{
		Start = start;
		End = end;
		Width = 1.f;
		ForkGeneration = 0;
	}

	FBranchSegment(FVector start, FVector end, float width)
	{
		Start = start;
		End = end;
		Width = width;
		ForkGeneration = 0;
	}

	FBranchSegment(FVector start, FVector end, float width, int8 forkGeneration)
	{
		Start = start;
		End = end;
		Width = width;
		ForkGeneration = forkGeneration;
	}
};

UCLASS()
class PROCEDURALMESHDEMOS_API ABranchingLinesActor : public AActor
{
	GENERATED_BODY()

public:
	ABranchingLinesActor();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (MakeEditWidget))
	FVector Start;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (MakeEditWidget))
	FVector End = FVector(0, 0, 300);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	uint8 Iterations = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	int32 RadialSegmentCount = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	bool bSmoothNormals = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	int32 RandomSeed = 1238;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (UIMin = "0.1", ClampMin = "0.1"))
	float MaxBranchOffset = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	bool bMaxBranchOffsetAsPercentageOfLength = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float BranchOffsetReductionEachGenerationPercentage = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float TrunkWidth = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters", meta = (UIMin = "0", UIMax = "100", ClampMin = "0", ClampMax = "100"))
	float ChanceOfForkPercentage = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float WidthReductionOnFork = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float ForkLengthMin = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float ForkLengthMax = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float ForkRotationMin = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float ForkRotationMax = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	UMaterialInterface* Material;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostLoad() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	URuntimeProceduralMeshComponent* MeshComponent;

private:
	void GenerateMesh();
	void CreateSegments();

	TArray<FBranchSegment> Segments;

	void GenerateCylinder(TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FProcMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, const FVector StartPoint, const FVector EndPoint, const float InWidth, const int32 InCrossSectionCount, int32& InVertexIndex, int32& InTriangleIndex, const bool bInSmoothNormals = true);

	static FVector RotatePointAroundPivot(const FVector InPoint, const FVector InPivot, const FVector InAngles);
	void PreCacheCrossSection();

	int32 LastCachedCrossSectionCount;
	TArray<FVector> CachedCrossSectionPoints;

	TArray<FVector> OffsetDirections;

	FRandomStream RngStream;

	// Mesh buffers
	void SetupMeshBuffers();
	TArray<FVector> Positions;
	TArray<int32> Triangles;
	TArray<FVector> Normals;
	TArray<FProcMeshTangent> Tangents;
	TArray<FVector2D> TexCoords;
};

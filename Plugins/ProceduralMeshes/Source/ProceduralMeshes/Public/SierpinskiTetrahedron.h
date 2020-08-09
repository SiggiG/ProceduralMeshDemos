// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example Sierpinski tetrahedron

#pragma once

#include "CoreMinimal.h"
#include "RuntimeMeshActor.h"
#include "SierpinskiTetrahedron.generated.h"

UENUM(BlueprintType)
enum class ETetrahedronSide : uint8
{
	Front,
	Left,
	Right,
	Bottom
};

USTRUCT()
struct FTetrahedronStructure
{
	GENERATED_BODY()

	FVector CornerBottomLeft;
	FVector CornerBottomRight;
	FVector CornerBottomMiddle;
	FVector CornerTop;

	// Remove these and provide ways to calculate them instead
	FVector FrontFaceLeftPoint;
	FVector2D FrontFaceLeftPointUV;
	FVector FrontFaceRightPoint;
	FVector2D FrontFaceRightPointUV;
	FVector FrontFaceTopPoint;
	FVector2D FrontFaceTopPointUV;

	FVector LeftFaceLeftPoint;
	FVector2D LeftFaceLeftPointUV;
	FVector LeftFaceRightPoint;
	FVector2D LeftFaceRightPointUV;
	FVector LeftFaceTopPoint;
	FVector2D LeftFaceTopPointUV;

	FVector RightFaceLeftPoint;
	FVector2D RightFaceLeftPointUV;
	FVector RightFaceRightPoint;
	FVector2D RightFaceRightPointUV;
	FVector RightFaceTopPoint;
	FVector2D RightFaceTopPointUV;

	FVector BottomFaceLeftPoint;
	FVector2D BottomFaceLeftPointUV;
	FVector BottomFaceRightPoint;
	FVector2D BottomFaceRightPointUV;
	FVector BottomFaceTopPoint;
	FVector2D BottomFaceTopPointUV;

	FVector FrontFaceNormal;
	FVector LeftFaceNormal;
	FVector RightFaceNormal;
	FVector BottomFaceNormal;

	FTetrahedronStructure()
	{
	}

	FTetrahedronStructure(const FVector InCornerBottomLeft, const FVector InCornerBottomRight, const FVector InCornerBottomMiddle, const FVector InCornerTop)
	{
		CornerBottomLeft = InCornerBottomLeft;
		CornerBottomRight = InCornerBottomRight;
		CornerBottomMiddle = InCornerBottomMiddle;
		CornerTop = InCornerTop;

		// Now setup each face
		FrontFaceLeftPoint = CornerBottomLeft;
		FrontFaceLeftPointUV = FVector2D(0, 1);
		FrontFaceRightPoint = CornerBottomRight;
		FrontFaceRightPointUV = FVector2D(1, 1);
		FrontFaceTopPoint = CornerTop;
		FrontFaceTopPointUV = FVector2D(0.5f, 0);

		FrontFaceNormal = FVector::CrossProduct(FrontFaceTopPoint - FrontFaceLeftPoint, FrontFaceRightPoint - FrontFaceLeftPoint).GetSafeNormal();

		LeftFaceLeftPoint = CornerBottomMiddle;
		LeftFaceLeftPointUV = FVector2D(0, 1);
		LeftFaceRightPoint = CornerBottomLeft;
		LeftFaceRightPointUV = FVector2D(1, 1);
		LeftFaceTopPoint = CornerTop;
		LeftFaceTopPointUV = FVector2D(0.5f, 0);
		
		LeftFaceNormal = FVector::CrossProduct(LeftFaceTopPoint - LeftFaceLeftPoint, LeftFaceRightPoint - LeftFaceLeftPoint).GetSafeNormal();

		RightFaceLeftPoint = CornerBottomRight;
		RightFaceLeftPointUV = FVector2D(0, 1);
		RightFaceRightPoint = CornerBottomMiddle;
		RightFaceRightPointUV = FVector2D(1, 1);
		RightFaceTopPoint = CornerTop;
		RightFaceTopPointUV = FVector2D(0.5f, 0);
		
		RightFaceNormal = FVector::CrossProduct(RightFaceTopPoint - RightFaceLeftPoint, RightFaceRightPoint - RightFaceLeftPoint).GetSafeNormal();

		BottomFaceLeftPoint = CornerBottomRight;
		BottomFaceLeftPointUV = FVector2D(0, 1);
		BottomFaceRightPoint = CornerBottomLeft;
		BottomFaceRightPointUV = FVector2D(1, 1);
		BottomFaceTopPoint = CornerBottomMiddle;
		BottomFaceTopPointUV = FVector2D(0.5f, 0);
		
		BottomFaceNormal = FVector::CrossProduct(BottomFaceTopPoint - BottomFaceLeftPoint, BottomFaceRightPoint - BottomFaceLeftPoint).GetSafeNormal();
	}
};

UCLASS()
class PROCEDURALMESHES_API ASierpinskiTetrahedron : public ARuntimeMeshActor
{
	GENERATED_BODY()
	
public:	
	ASierpinskiTetrahedron();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	float Size = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	int32 Iterations = 5; // 4^5 = 1024 tetrahedrons

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Procedural Parameters")
	UMaterialInterface* Material;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostLoad() override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient)
	URuntimeMeshProviderStatic* StaticProvider;

private:

	void GenerateMesh();
	
	void GenerateTetrahedron(const FTetrahedronStructure& Tetrahedron, int32 InDepth, TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FRuntimeMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, int32& VertexIndex, int32& TriangleIndex) const;
	static void AddTetrahedronPolygons(const FTetrahedronStructure& Tetrahedron, TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FRuntimeMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, int32& VertexIndex, int32& TriangleIndex);
	static void AddPolygon(const FVector& Point1, const FVector2D& Point1UV, const FVector& Point2, const FVector2D& Point2UV, const FVector& Point3, const FVector2D& Point3UV, FVector const FaceNormal, TArray<FVector>& InVertices, TArray<int32>& InTriangles, TArray<FVector>& InNormals, TArray<FRuntimeMeshTangent>& InTangents, TArray<FVector2D>& InTexCoords, int32& VertexIndex, int32& TriangleIndex);
	void SetTetrahedronUV(FTetrahedronStructure& Tetrahedron) const;
	FVector2D GetUVForSide(const FVector Point, const ETetrahedronSide Side) const;

	UPROPERTY(Transient)
	FTetrahedronStructure FirstTetrahedron;

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

	// Pre-calculated vectors that define a quad for each tetrahedron side
	void PrecalculateTetrahedronSideQuads();
	FVector FrontQuadTopLeftPoint;
	FVector FrontQuadTopSide;
	FVector FrontQuadLeftSide;

	FVector LeftQuadTopLeftPoint;
	FVector LeftQuadTopSide;
	FVector LeftQuadLeftSide;

	FVector RightQuadTopLeftPoint;
	FVector RightQuadTopSide;
	FVector RightQuadLeftSide;

	FVector BottomQuadTopLeftPoint;
	FVector BottomQuadTopSide;
	FVector BottomQuadLeftSide;
};

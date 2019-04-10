// Copyright Sigurdur Gunnarsson. All Rights Reserved. 
// Licensed under the MIT License. See LICENSE file in the project root for full license information. 
// Example Sierpinski tetrahedron

#pragma once

#include "GameFramework/Actor.h"
#include "RuntimeMeshComponent.h"
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
	FRuntimeMeshVertexSimple FrontFaceLeftPoint;
	FRuntimeMeshVertexSimple FrontFaceRightPoint;
	FRuntimeMeshVertexSimple FrontFaceTopPoint;

	FRuntimeMeshVertexSimple LeftFaceLeftPoint;
	FRuntimeMeshVertexSimple LeftFaceRightPoint;
	FRuntimeMeshVertexSimple LeftFaceTopPoint;

	FRuntimeMeshVertexSimple RightFaceLeftPoint;
	FRuntimeMeshVertexSimple RightFaceRightPoint;
	FRuntimeMeshVertexSimple RightFaceTopPoint;

	FRuntimeMeshVertexSimple BottomFaceLeftPoint;
	FRuntimeMeshVertexSimple BottomFaceRightPoint;
	FRuntimeMeshVertexSimple BottomFaceTopPoint;

	FVector FrontFaceNormal;
	FVector LeftFaceNormal;
	FVector RightFaceNormal;
	FVector BottomFaceNormal;

	FTetrahedronStructure()
	{
	}

	FTetrahedronStructure(FVector InCornerBottomLeft, FVector InCornerBottomRight, FVector InCornerBottomMiddle, FVector InCornerTop)
	{
		CornerBottomLeft = InCornerBottomLeft;
		CornerBottomRight = InCornerBottomRight;
		CornerBottomMiddle = InCornerBottomMiddle;
		CornerTop = InCornerTop;

		// Now setup each face
		FrontFaceLeftPoint.Position = CornerBottomLeft;
		FrontFaceLeftPoint.UV0 = FVector2D(0, 1);
		FrontFaceRightPoint.Position = CornerBottomRight;
		FrontFaceRightPoint.UV0 = FVector2D(1, 1);
		FrontFaceTopPoint.Position = CornerTop;
		FrontFaceTopPoint.UV0 = FVector2D(0.5f, 0);

		FrontFaceNormal = FVector::CrossProduct(FrontFaceTopPoint.Position - FrontFaceLeftPoint.Position, FrontFaceRightPoint.Position - FrontFaceLeftPoint.Position).GetSafeNormal();
		FrontFaceLeftPoint.Normal = FrontFaceNormal;
		FrontFaceRightPoint.Normal = FrontFaceNormal;
		FrontFaceTopPoint.Normal = FrontFaceNormal;

		LeftFaceLeftPoint.Position = CornerBottomMiddle;
		LeftFaceLeftPoint.UV0 = FVector2D(0, 1);
		LeftFaceRightPoint.Position = CornerBottomLeft;
		LeftFaceRightPoint.UV0 = FVector2D(1, 1);
		LeftFaceTopPoint.Position = CornerTop;
		LeftFaceTopPoint.UV0 = FVector2D(0.5f, 0);
		
		LeftFaceNormal = FVector::CrossProduct(LeftFaceTopPoint.Position - LeftFaceLeftPoint.Position, LeftFaceRightPoint.Position - LeftFaceLeftPoint.Position).GetSafeNormal();
		LeftFaceLeftPoint.Normal = LeftFaceNormal;
		LeftFaceRightPoint.Normal = LeftFaceNormal;
		LeftFaceTopPoint.Normal = LeftFaceNormal;

		RightFaceLeftPoint.Position = CornerBottomRight;
		RightFaceLeftPoint.UV0 = FVector2D(0, 1);
		RightFaceRightPoint.Position = CornerBottomMiddle;
		RightFaceRightPoint.UV0 = FVector2D(1, 1);
		RightFaceTopPoint.Position = CornerTop;
		RightFaceTopPoint.UV0 = FVector2D(0.5f, 0);
		
		RightFaceNormal = FVector::CrossProduct(RightFaceTopPoint.Position - RightFaceLeftPoint.Position, RightFaceRightPoint.Position - RightFaceLeftPoint.Position).GetSafeNormal();
		RightFaceLeftPoint.Normal = RightFaceNormal;
		RightFaceRightPoint.Normal = RightFaceNormal;
		RightFaceTopPoint.Normal = RightFaceNormal;

		BottomFaceLeftPoint.Position = CornerBottomRight;
		BottomFaceLeftPoint.UV0 = FVector2D(0, 1);
		BottomFaceRightPoint.Position = CornerBottomLeft;
		BottomFaceRightPoint.UV0 = FVector2D(1, 1);
		BottomFaceTopPoint.Position = CornerBottomMiddle;
		BottomFaceTopPoint.UV0 = FVector2D(0.5f, 0);
		
		BottomFaceNormal = FVector::CrossProduct(BottomFaceTopPoint.Position - BottomFaceLeftPoint.Position, BottomFaceRightPoint.Position - BottomFaceLeftPoint.Position).GetSafeNormal();
		RightFaceLeftPoint.Normal = RightFaceNormal;
		RightFaceRightPoint.Normal = RightFaceNormal;
		RightFaceTopPoint.Normal = RightFaceNormal;
	}
};

UCLASS()
class PROCEDURALMESHES_API ASierpinskiTetrahedron : public AActor
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
	void GenerateTetrahedron(const FTetrahedronStructure& Tetrahedron, int32 InDepth, TArray<FRuntimeMeshVertexSimple>& InVertices, TArray<int32>& InTriangles, int32& VertexIndex, int32& TriangleIndex);
	void AddPolygon(const FRuntimeMeshVertexSimple& Point1, const FRuntimeMeshVertexSimple& Point2, const FRuntimeMeshVertexSimple& Point3, FVector FaceNormal, TArray<FRuntimeMeshVertexSimple>& InVertices, TArray<int32>& InTriangles, int32& VertexIndex, int32& TriangleIndex);
	FVector2D GetUVForSide(FVector Point, ETetrahedronSide Side);

	FTetrahedronStructure FirstTetrahedron;

	// Mesh buffers
	void SetupMeshBuffers();
	bool bHaveBuffersBeenInitialized = false;
	TArray<FRuntimeMeshVertexSimple> Vertices;
	TArray<int32> Triangles;

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

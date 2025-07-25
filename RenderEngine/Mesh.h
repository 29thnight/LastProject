#pragma once
#include "Core.Minimal.h"
#include "Mesh.generated.h"
#include <assimp/Importer.hpp>

struct ModelNode
{
	std::string m_name;
	Mathf::Matrix m_transform{ XMMatrixIdentity() };
	uint32 m_index{};
	uint32 m_parentIndex{};
	std::vector<uint32> m_childrenIndex;
	uint32 m_numChildren{};
	uint32 m_numMeshes{};
	std::vector<uint32> m_meshes;

	ModelNode() = default;
	ModelNode(const std::string_view& name) : m_name(name) {}
};

struct Vertex
{
	Mathf::Vector3 position;
	Mathf::Vector3 normal;
	Mathf::Vector2 uv0;
	Mathf::Vector2 uv1;
	Mathf::Vector3 tangent;
	Mathf::Vector3 bitangent;
	Mathf::Vector4 boneIndices;
	Mathf::Vector4 boneWeights;

	Vertex() = default;
	Vertex(
		const Mathf::Vector3& _position, 
		const Mathf::Vector3& _normal, 
		const Mathf::Vector2& _uv0, 
		const Mathf::Vector2& _uv1, 
		const Mathf::Vector3& _tangent, 
		const Mathf::Vector3& _bitangent, 
		const Mathf::Vector4& _boneIndices, 
		const Mathf::Vector4& _boneWeights
	) :
		position(_position), 
		normal(_normal), 
		uv0(_uv0),
		uv1(_uv1),
		tangent(_tangent), 
		bitangent(_bitangent), 
		boneIndices(_boneIndices), 
		boneWeights(_boneWeights) {}

	Vertex(const Mathf::Vector3& _position, const Mathf::Vector3& _normal, const Mathf::Vector2& _uv) :
		position(_position), normal(_normal), uv0(_uv) {}

	static Vertex ConvertToAiMesh(aiMesh* mesh, uint32 i)
	{
		if (!mesh->HasPositions() || !mesh->HasNormals() || !mesh->HasTangentsAndBitangents())
		{
			throw std::runtime_error("Mesh does not have required vertex attributes.");
		}

		if (mesh->mVertices == nullptr || mesh->mNormals == nullptr || mesh->mTangents == nullptr || mesh->mBitangents == nullptr)
		{
			throw std::runtime_error("Mesh vertex data is null.");
		}

		bool hasTexCoords = mesh->mTextureCoords[0] != nullptr;
		bool hasTexCoords1 = mesh->mTextureCoords[1] != nullptr;

		Vertex vertex;
		vertex.position		= { mesh->mVertices[i].x,	mesh->mVertices[i].y,	mesh->mVertices[i].z	};
		vertex.normal		= { mesh->mNormals[i].x,	mesh->mNormals[i].y,	mesh->mNormals[i].z		};
		vertex.tangent		= { mesh->mTangents[i].x,	mesh->mTangents[i].y,	mesh->mTangents[i].z	};
		vertex.bitangent	= { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z	};
		if (hasTexCoords)
		{
			vertex.uv0 = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };

			if (hasTexCoords1)
			{
				vertex.uv1 = { mesh->mTextureCoords[1][i].x, mesh->mTextureCoords[1][i].y };
			}
			else
			{
				vertex.uv1 = vertex.uv0;
			}
		}

		return vertex;
	}
};

class Texture;
class Material;
class ModelLoader;
class MeshOptimizer;
class Camera;
class Mesh
{
public:
	// 각 LOD 레벨의 GPU 리소스를 관리하는 구조체
	struct LODResource
	{
		ComPtr<ID3D11Buffer> vertexBuffer;
		ComPtr<ID3D11Buffer> indexBuffer;
		uint32 indexCount;
	};

public:
   ReflectMesh
    [[Serializable]]
	Mesh() = default;
	Mesh(const std::string_view& _name, const std::vector<Vertex>& _vertices, const std::vector<uint32>& _indices);
	Mesh(const std::string_view& _name, std::vector<Vertex>&& _vertices, std::vector<uint32>&& _indices);
	Mesh(Mesh&& _other) noexcept;
	~Mesh();

	bool operator==(const Mesh& _other) const
	{
		return m_hashingMesh == _other.m_hashingMesh;
	}

	void AssetInit();
	void Draw();
	void Draw(ID3D11DeviceContext* _deferredContext);
	void DrawShadow();
	void DrawShadow(ID3D11DeviceContext* _deferredContext);
	void DrawInstanced(ID3D11DeviceContext* _deferredContext, size_t instanceCount);

	// [NEW] 특정 LOD 레벨을 그리는 함수 (렌더링 시스템에서 호출)
	void DrawLOD(ID3D11DeviceContext* context, uint32_t lodIndex);
	void DrawShadowLOD(ID3D11DeviceContext* context, uint32_t lodIndex);
	void DrawInstancedLOD(ID3D11DeviceContext* context, uint32_t lodIndex, size_t instanceCount);

	// LOD 생성 함수
	bool HasLODs() const;
	void GenerateLODs(const std::vector<float>& lodThresholds);
	uint32_t SelectLOD(Camera* camera, const Mathf::Matrix& worldMatrix) const;
	const std::vector<float>& GetLODThresholds() const { return m_LODThresholds; }

	std::string GetName() const { return m_name; }
	std::string GetModelName() const { return m_modelName; }

	const std::vector<Vertex>& GetVertices() { return m_vertices; }
	const std::vector<uint32>& GetIndices() { return m_indices; }

	BoundingBox GetBoundingBox() const { return m_boundingBox; }
	BoundingSphere GetBoundingSphere() const { return m_boundingSphere; }

	std::vector<Vertex> GetVertices() const{ return m_vertices; }
	std::vector<uint32> GetIndices() const { return m_indices; }
	ComPtr<ID3D11Buffer> GetVertexBuffer() { return m_vertexBuffer; }
	ComPtr<ID3D11Buffer> GetIndexBuffer()  { return m_indexBuffer; }
	uint32 GetStride()  { return m_stride; }

	bool IsShadowOptimized() const { return m_isShadowOptimized; }
	void SetShadowOptimized(bool isOptimized) { m_isShadowOptimized = isOptimized; }

	void MakeShadowOptimizedBuffer();
	ComPtr<ID3D11Buffer> GetShadowVertexBuffer() { return m_shadowVertexBuffer; }
	ComPtr<ID3D11Buffer> GetShadowIndexBuffer() { return m_shadowIndexBuffer; }

	HashedGuid m_hashingMesh{ make_guid() };

private:
	friend class ModelLoader;
	friend class MeshOptimizer;

    [[Property]]
	std::string m_name;

    [[Property]]
	uint32 m_materialIndex{};

	std::string m_modelName;

	bool m_isShadowOptimized{ false };

	std::vector<Vertex> m_vertices;
	std::vector<uint32> m_indices;

	std::vector<Vertex> m_shadowVertices;
	std::vector<uint32> m_shadowIndices;

	DirectX::BoundingBox m_boundingBox;
	DirectX::BoundingSphere m_boundingSphere;

	// --- LOD 관련 멤버 변수 ---
	// 인덱스 0: 원본(LOD0), 1: LOD1, ...
	std::vector<LODResource> m_LODs;
	// 화면 공간 크기 기반의 LOD 전환 임계값
	[[Property]]
	std::vector<float> m_LODThresholds;
	// ---

	ComPtr<ID3D11Buffer> m_vertexBuffer{};
	ComPtr<ID3D11Buffer> m_indexBuffer{};

	ComPtr<ID3D11Buffer> m_shadowVertexBuffer{};
	ComPtr<ID3D11Buffer> m_shadowIndexBuffer{};

	static constexpr const uint32 m_stride = sizeof(Vertex);
};

class PrimitiveCreator
{
public:
	static inline std::vector<Vertex> CubeVertices()
	{
		std::vector<Vertex> cube;
		cube.reserve(24);

		cube.push_back({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f,  0.0f, -1.0f), XMFLOAT2(0.0f, 0.0f) });
		cube.push_back({ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f,  0.0f, -1.0f), XMFLOAT2(1.0f, 0.0f) });
		cube.push_back({ XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f,  0.0f, -1.0f), XMFLOAT2(1.0f, 1.0f) });
		cube.push_back({ XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f,  0.0f, -1.0f), XMFLOAT2(0.0f, 1.0f) });

		cube.push_back({ XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f,  0.0f,  1.0f), XMFLOAT2(0.0f, 0.0f) });
		cube.push_back({ XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f,  0.0f,  1.0f), XMFLOAT2(1.0f, 0.0f) });
		cube.push_back({ XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f,  0.0f,  1.0f), XMFLOAT2(1.0f, 1.0f) });
		cube.push_back({ XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f,  0.0f,  1.0f), XMFLOAT2(0.0f, 1.0f) });

		cube.push_back({ XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(-1.0f,  0.0f,  0.0f), XMFLOAT2(0.0f, 0.0f) });
		cube.push_back({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(-1.0f,  0.0f,  0.0f), XMFLOAT2(1.0f, 0.0f) });
		cube.push_back({ XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(-1.0f,  0.0f,  0.0f), XMFLOAT2(1.0f, 1.0f) });
		cube.push_back({ XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(-1.0f,  0.0f,  0.0f), XMFLOAT2(0.0f, 1.0f) });

		cube.push_back({ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f,  0.0f,  0.0f), XMFLOAT2(0.0f, 0.0f) });
		cube.push_back({ XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(1.0f,  0.0f,  0.0f), XMFLOAT2(1.0f, 0.0f) });
		cube.push_back({ XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f,  0.0f,  0.0f), XMFLOAT2(1.0f, 1.0f) });
		cube.push_back({ XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(1.0f,  0.0f,  0.0f), XMFLOAT2(0.0f, 1.0f) });

		cube.push_back({ XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f,  0.0f), XMFLOAT2(0.0f, 0.0f) });
		cube.push_back({ XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, -1.0f,  0.0f), XMFLOAT2(1.0f, 0.0f) });
		cube.push_back({ XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, -1.0f,  0.0f), XMFLOAT2(1.0f, 1.0f) });
		cube.push_back({ XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, -1.0f,  0.0f), XMFLOAT2(0.0f, 1.0f) });

		cube.push_back({ XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f,  1.0f,  0.0f), XMFLOAT2(0.0f, 0.0f) });
		cube.push_back({ XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f,  1.0f,  0.0f), XMFLOAT2(1.0f, 0.0f) });
		cube.push_back({ XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f,  1.0f,  0.0f), XMFLOAT2(1.0f, 1.0f) });
		cube.push_back({ XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f,  1.0f,  0.0f), XMFLOAT2(0.0f, 1.0f) });

		return cube;
	}

	static inline std::vector<uint32> CubeIndices()
	{
		return std::vector<uint32>
		{
			0, 2, 1, 0, 3, 2,
			4, 6, 5, 4, 7, 6,
			8, 10, 9, 8, 11, 10,
			12, 14, 13, 12, 15, 14,
			16, 18, 17, 16, 19, 18,
			20, 22, 21, 20, 23, 22
		};
	}

	static inline std::vector<Vertex> QuadVertices()
	{
		std::vector<Vertex> quad;
		quad.reserve(4);
		quad.push_back({ XMFLOAT3(-1.0f,  0.0f,  1.0f), XMFLOAT3(0.0f,  1.0f,  0.0f), XMFLOAT2(0.0f, 0.0f) });
		quad.push_back({ XMFLOAT3(-1.0f,  0.0f, -1.0f), XMFLOAT3(0.0f,  1.0f,  0.0f), XMFLOAT2(0.0f, 1.0f) });
		quad.push_back({ XMFLOAT3(1.0f,  0.0f, -1.0f), XMFLOAT3(0.0f,  1.0f,  0.0f), XMFLOAT2(1.0f, 1.0f) });
		quad.push_back({ XMFLOAT3(1.0f,  0.0f,  1.0f), XMFLOAT3(0.0f,  1.0f,  0.0f), XMFLOAT2(1.0f, 0.0f) });
		return quad;
	}

	static inline std::vector<uint32> QuadIndices()
	{
		return std::vector<uint32>{ 0, 2, 1, 0, 3, 2 };
	}
};

struct UIvertex
{
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT2 texCoord;
};

class UIMesh
{
public:
	UIMesh();
	~UIMesh();

	void Draw();
	void Draw(ID3D11DeviceContext* _deferredContext);

	const std::string& GetName() { return m_name; }
private:
	friend class ModelLoader;
	std::string m_name;
	std::vector<UIvertex> m_vertices;
	std::vector<uint32> m_indices;
	uint32 m_materialIndex{};

	std::string m_nodeName;

	DirectX::BoundingBox m_boundingBox;
	DirectX::BoundingSphere m_boundingSphere;

	ComPtr<ID3D11Buffer> m_vertexBuffer{};
	ComPtr<ID3D11Buffer> m_indexBuffer{};
	static constexpr uint32 m_stride = sizeof(UIvertex);


	std::vector<UIvertex> UIQuad
	{
		{ {-1.0f,  1.0f, 0.0f}, { 0.0f, 0.0f} },  // 좌상단
		{ { 1.0f,  1.0f, 0.0f}, { 1.0f, 0.0f} },   // 우상단
		{ { 1.0f, -1.0f, 0.0f}, { 1.0f, 1.0f} },   // 우하단
		{ {-1.0f, -1.0f, 0.0f}, { 0.0f, 1.0f} },    // 좌하단
	};
	std::vector<uint32> UIIndices = { 0, 1, 2, 0, 2, 3 };
};

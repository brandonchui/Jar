#pragma once

#include <memory>
#include <string>
#include <cstdint>
#include <utility>
#include <vectormath.hpp>
#include "Material.h"

class Mesh;

/// The transform "component" for the Entity class.
struct TransformEntity
{
public:
	Vector3 position = Vector3(0, 0, 0);
	Vector3 rotation = Vector3(0, 0, 0);
	Vector3 scale = Vector3(1, 1, 1);

	Matrix4 ToMatrix() const;
	Matrix4 ToInverseTransposeMatrix() const;
};

/// Entity holds the data for a renderable object, mainly for
/// the meshes and the material that it has.
///
/// This is NOT part of an Entity Component System, the Scene class
/// has an array of Entities that it will loop through and render.
class Entity
{
public:
	Entity(uint32_t id, std::string name);

	/// The Scene class will have internal bump pointer and will assign
	/// an integer id when adding Entity to the Scene.
	uint32_t GetId() const { return mId; }

	const std::string& GetName() const { return mName; }
	std::shared_ptr<Mesh> GetMesh() const { return mMesh; }

	bool IsVisible() const { return mVisible; }
	bool IsSelected() const { return mSelected; }

	const TransformEntity& GetTransform() const { return mTransform; }
	TransformEntity& GetTransform() { return mTransform; }
	void SetTransform(const TransformEntity& t) { mTransform = t; }

	const Material& GetMaterial() const { return mMaterial; }
	Material& GetMaterial() { return mMaterial; }

	void SetVisible(bool visible) { mVisible = visible; }
	void SetSelected(bool selected) { mSelected = selected; }
	void SetMesh(std::shared_ptr<Mesh> mesh) { mMesh = std::move(mesh); }

	uint32_t GetRenderFlags() const { return mRenderFlags; }
	void SetRenderFlags(uint32_t flags) { mRenderFlags = flags; }

	enum RenderFlags
	{
		CASTS_SHADOWS = 1 << 0,
		RECEIVES_SHADOWS = 1 << 1,
		RAYTRACING_ENABLED = 1 << 2,
		MESH_SHADER_PATH = 1 << 3
	};

private:
	uint32_t mId;
	std::string mName;
	bool mVisible = true;
	bool mSelected = false;

	TransformEntity mTransform;
	std::shared_ptr<Mesh> mMesh;
	Material mMaterial;

	uint32_t mRenderFlags = CASTS_SHADOWS | RECEIVES_SHADOWS;
};

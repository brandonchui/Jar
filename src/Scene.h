#pragma once
#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include "Entity.h"

class Mesh;

/// For a renderer that is not for games or making heavy use
/// interactivity like a simulation or games, I was not sure
/// if a scene graph is the right move. Did not think an ECS
/// method was beneficial either, so opted to use a simple
/// Scene class that just holds an array of all renderable
/// Entity for the entire renderer.
class Scene
{
public:
	Scene() = default;

	Entity* AddEntity(const std::string& name, std::shared_ptr<Mesh> mesh);
	void RemoveEntity(uint32_t id);

	Entity* GetEntity(uint32_t id);

	/// Mainly for use in the UI side of things for later.
	Entity* GetSelectedEntity();

	const std::vector<std::unique_ptr<Entity>>& GetEntities() const;

	void SetSelected(uint32_t id, bool selected);
	void ClearSelection();

private:
	std::vector<std::unique_ptr<Entity>> mEntities;

	/// Internal tracker of the next id available when adding a mesh.
	uint32_t mNextEntityId = 0;
};

#include "Scene.h"
#include "Entity.h"
#include "Mesh.h"
#include <algorithm>
#include <utility>

Entity* Scene::AddEntity(const std::string& name, std::shared_ptr<Mesh> mesh)
{
	auto entity = std::make_unique<Entity>(mNextEntityId++, name);
	entity->SetMesh(std::move(mesh));

	Entity* ptr = entity.get();
	mEntities.push_back(std::move(entity));

	return ptr;
}

void Scene::RemoveEntity(uint32_t id)
{
	auto it = std::remove_if(mEntities.begin(), mEntities.end(),
							 [id](const std::unique_ptr<Entity>& e) {
								 return e->GetId() == id;
							 });

	mEntities.erase(it, mEntities.end());
}

Entity* Scene::GetEntity(uint32_t id)
{
	for (auto& entity : mEntities)
	{
		if (entity->GetId() == id)
		{
			return entity.get();
		}
	}
	return nullptr;
}

Entity* Scene::GetSelectedEntity()
{
	for (auto& entity : mEntities)
	{
		if (entity->IsSelected())
		{
			return entity.get();
		}
	}
	return nullptr;
}

const std::vector<std::unique_ptr<Entity>>& Scene::GetEntities() const
{
	return mEntities;
}

void Scene::SetSelected(uint32_t id, bool selected)
{
	for (auto& entity : mEntities)
	{
		if (entity->GetId() == id)
		{
			entity->SetSelected(selected);
		}
		else if (selected)
		{
			entity->SetSelected(false);
		}
	}
}

void Scene::ClearSelection()
{
	for (auto& entity : mEntities)
	{
		entity->SetSelected(false);
	}
}

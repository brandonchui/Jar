#pragma once

#include <vectormath.hpp>

/// Interface for camera implementations.
/// Provides view and projection matrices for rendering.
class ICamera
{
public:
	virtual ~ICamera() = default;

	virtual void Update(float deltaTime) = 0;

	virtual Matrix4 GetViewMatrix() const = 0;
	virtual Matrix4 GetProjectionMatrix() const = 0;

	/// The camera world position.
	virtual Vector3 GetPosition() const = 0;

	/// Handle mouse motion for camera control, though this
	/// might be useless for FPS cameras.
	virtual void OnMouseMove(float deltaX, float deltaY, bool isRotating) = 0;

	/// Handle mouse wheel for zoom control, though again
	/// might be useless for FPS cameras.
	virtual void OnMouseWheel(float delta) = 0;

	virtual void SetAspectRatio(float aspectRatio) = 0;
};

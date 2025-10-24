#pragma once

#include "ICamera.h"
#include <vectormath.hpp>

/// Orbital camera that rotates around a target point.
class OrbitCamera : public ICamera
{
public:
	OrbitCamera(const Vector3& target = Vector3(0.0F, 0.0F, 0.0F), float distance = 20.0F,
				float fovY = 1.22173F, float aspectRatio = 16.0F / 9.0F, float nearZ = 0.1F,
				float farZ = 100.0F);

	~OrbitCamera() override = default;

	void Update(float deltaTime) override;

	Matrix4 GetViewMatrix() const override;
	Matrix4 GetProjectionMatrix() const override;
	Vector3 GetPosition() const override;

	void OnMouseMove(float deltaX, float deltaY, bool isRotating) override;
	void OnMouseWheel(float delta) override;

	void SetAspectRatio(float aspectRatio) override;

	void SetTarget(const Vector3& target);
	void SetDistance(float distance);
	void SetAngles(float azimuth, float elevation);
	void SetRotationSpeed(float speed) { mRotationSpeed = speed; }
	void SetZoomSpeed(float speed) { mZoomSpeed = speed; }

	float GetAzimuth() const { return mAzimuth; }
	float GetElevation() const { return mElevation; }
	float GetDistance() const { return mDistance; }

private:
	void ClampAngles();
	void UpdatePosition();

	Vector3 mTarget;
	float mDistance;
	float mMinDistance = 1.0F;
	float mMaxDistance = 100.0F;

	/// Radians
	float mAzimuth;
	float mElevation;
	float mMinElevation = -1.5F;
	float mMaxElevation = 1.5F;

	float mFovY;
	float mAspectRatio;
	float mNearZ;
	float mFarZ;

	float mRotationSpeed = 0.005F;
	float mZoomSpeed = 1.0F;

	Vector3 mPosition;
};

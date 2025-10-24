#include "OrbitCamera.h"
#include <algorithm>
#include <cmath>
#include <numbers>

OrbitCamera::OrbitCamera(const Vector3& target, float distance, float fovY, float aspectRatio,
						 float nearZ, float farZ)
: mTarget(target)
, mDistance(distance)
, mAzimuth(0.0F)
, mElevation(0.0F)
, mFovY(fovY)
, mAspectRatio(aspectRatio)
, mNearZ(nearZ)
, mFarZ(farZ)
{
	UpdatePosition();
}

void OrbitCamera::Update(float deltaTime)
{
	// TODO smooth interpolation?
}

Matrix4 OrbitCamera::GetViewMatrix() const
{
	Vector3 forward = normalize(mTarget - mPosition);
	Vector3 right = normalize(cross(forward, Vector3(0.0F, 1.0F, 0.0F)));
	Vector3 up = cross(right, forward);

	Matrix4 view = Matrix4::identity();

	view.setCol0(Vector4(right.getX(), up.getX(), -forward.getX(), 0.0F));
	view.setCol1(Vector4(right.getY(), up.getY(), -forward.getY(), 0.0F));
	view.setCol2(Vector4(right.getZ(), up.getZ(), -forward.getZ(), 0.0F));

	view.setCol3(
		Vector4(-dot(right, mPosition), -dot(up, mPosition), dot(forward, mPosition), 1.0F));

	return view;
}

Matrix4 OrbitCamera::GetProjectionMatrix() const
{
	return Matrix4::perspective(mFovY, mAspectRatio, mNearZ, mFarZ);
}

Vector3 OrbitCamera::GetPosition() const
{
	return mPosition;
}

void OrbitCamera::OnMouseMove(float deltaX, float deltaY, bool isRotating)
{
	if (!isRotating)
		return;

	mAzimuth -= deltaX * mRotationSpeed;
	mElevation += deltaY * mRotationSpeed;

	ClampAngles();
	UpdatePosition();
}

void OrbitCamera::OnMouseWheel(float delta)
{
	mDistance -= delta * mZoomSpeed;
	mDistance = std::clamp(mDistance, mMinDistance, mMaxDistance);

	UpdatePosition();
}

void OrbitCamera::SetAspectRatio(float aspectRatio)
{
	mAspectRatio = aspectRatio;
}

void OrbitCamera::SetTarget(const Vector3& target)
{
	mTarget = target;
	UpdatePosition();
}

void OrbitCamera::SetDistance(float distance)
{
	mDistance = std::clamp(distance, mMinDistance, mMaxDistance);
	UpdatePosition();
}

void OrbitCamera::SetAngles(float azimuth, float elevation)
{
	mAzimuth = azimuth;
	mElevation = elevation;
	ClampAngles();
	UpdatePosition();
}

void OrbitCamera::ClampAngles()
{
	// Wrap azimuth to [-PI, PI].
	const float PI = std::numbers::pi_v<float>;
	while (mAzimuth > PI)
		mAzimuth -= 2.0F * PI;
	while (mAzimuth < -PI)
		mAzimuth += 2.0F * PI;

	// Prevent gimbal lock.
	mElevation = std::clamp(mElevation, mMinElevation, mMaxElevation);
}

void OrbitCamera::UpdatePosition()
{
	// Spherical -> Cartesian
	float cosElev = std::cos(mElevation);
	float sinElev = std::sin(mElevation);
	float cosAzim = std::cos(mAzimuth);
	float sinAzim = std::sin(mAzimuth);

	Vector3 offset(mDistance * cosElev * sinAzim, mDistance * sinElev,
				   mDistance * cosElev * cosAzim);

	mPosition = mTarget + offset;
}

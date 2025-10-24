#include "Entity.h"
#include <cmath>

Entity::Entity(uint32_t id, std::string name)
: mId(id)
, mName(std::move(name))
{
}

Matrix4 TransformEntity::ToMatrix() const
{
	Matrix4 scaleMat = Matrix4::scale(scale);

	float cosX = std::cos(rotation.getX());
	float sinX = std::sin(rotation.getX());
	float cosY = std::cos(rotation.getY());
	float sinY = std::sin(rotation.getY());
	float cosZ = std::cos(rotation.getZ());
	float sinZ = std::sin(rotation.getZ());

	Matrix4 rotX(Vector4(1, 0, 0, 0), Vector4(0, cosX, sinX, 0), Vector4(0, -sinX, cosX, 0),
				 Vector4(0, 0, 0, 1));

	Matrix4 rotY(Vector4(cosY, 0, -sinY, 0), Vector4(0, 1, 0, 0), Vector4(sinY, 0, cosY, 0),
				 Vector4(0, 0, 0, 1));

	Matrix4 rotZ(Vector4(cosZ, sinZ, 0, 0), Vector4(-sinZ, cosZ, 0, 0), Vector4(0, 0, 1, 0),
				 Vector4(0, 0, 0, 1));

	Matrix4 rotMat = rotZ * rotY * rotX;

	Matrix4 transMat = Matrix4::translation(position);

	return transMat * rotMat * scaleMat;
}

Matrix4 TransformEntity::ToInverseTransposeMatrix() const
{
	Matrix4 world = ToMatrix();
	Matrix4 worldInv = inverse(world);
	return transpose(worldInv);
}

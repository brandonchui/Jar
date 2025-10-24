#include "GpuResource.h"

GpuResource::GpuResource() = default;

GpuResource::~GpuResource()
{
	Destroy();
}

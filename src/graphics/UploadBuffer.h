#pragma once

#include "GpuResource.h"
#include <D3D12MemAlloc.h>

/// A CPU writable and GPU readable block of memory.
/// NOTE This class might be a bit confusing as it supports the
/// pattern for both:
/// Persistant mapping: Mainly for constant buffers.
/// Temporary staging: One time transfers, like for Texture
/// which can Unmap() afterwards.
class UploadBuffer : public GpuResource
{
public:
	UploadBuffer();
	~UploadBuffer() override;

	/// For a temporarily upload for a single time only. Note this
	/// differs from the other overload as this will immediately
	/// Unmap() once the memcpy is done.
	void Initialize(const void* data, UINT sizeInBytes);

	/// For use in persistent constant buffers where we don't want
	/// to Unmap(). Use the Copy() to update the mapped buffer.
	void Initialize(UINT sizeInBytes);

	/// Returns CPU accessible pointer to the mapped GPU memory.
	/// Used only if you Initialize(UINT)
	void* GetMappedData() const { return mMappedData; }

	/// Writes data to the persistently mapped GPU memory at the given
	/// offset. Essentially a wrapper around memcpy.
	/// Only works for buffers initialized with Initialize(UINT).
	void Copy(const void* data, UINT sizeInBytes, UINT offset);

private:
	void* mMappedData = nullptr;
	D3D12MA::Allocation* mAllocation = nullptr;
};

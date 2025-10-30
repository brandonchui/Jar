#pragma once

#include "ColorBuffer.h"
struct GBuffer
{
	ColorBuffer albedo;
	ColorBuffer normal;
	ColorBuffer roughMetalAO;

	// Emissive?
	// Material id?
};

#pragma once


#include "..\Common\DeviceResources.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct Entity
{
	DirectX::XMFLOAT2	position;	// of its centre, in view units
	DirectX::XMFLOAT2	velocity;	// view units per second
	float				size;
	float				angle;
	float				spin;		// radians per second
	DirectX::XMFLOAT4	color;
	bool				isBadge;	// drawn from the image; the others are plain squares
};
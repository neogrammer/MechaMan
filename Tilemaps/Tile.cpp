#include "pch.h"
#include "Tile.h"

PairF MechaMan::Tile::unIso() const
{
    PairF temp{};
    temp.x = ((spr.position.x - spr.position.y)*(sqrtf(3.f)/2.f)) + ((spr.position.x + spr.position.y) * 0.5f);
    temp.y = ((spr.position.x - spr.position.y) * 0.5f) + ((spr.position.x + spr.position.y) * (sqrtf(3.f) / 2.f));
    return temp;
}

void MechaMan::Tile::setSprite(const Sprite& spr_)
{
    spr.texture = spr_.texture;
	spr.texRect = spr_.texRect;
	spr.size = spr_.size;  
	spr.rotation = spr_.rotation;
	spr.flipX = spr_.flipX;
	spr.flipY = spr_.flipY;
	spr.color = spr_.color;
	spr.origin = spr_.origin;
	spr.layer = spr_.layer;
	spr.position = spr_.position;

}





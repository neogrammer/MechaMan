#include "pch.h"
#include "Tileset.h"

void MechaMan::Tileset::addTile(const Tile& tile)
{
	tiles.push_back(Tile{});
	Tile& t = tiles[tiles.size() - 1];
	Sprite temp{};
	temp.color = tile.spr.color;
	temp.flipX = tile.spr.flipX;
	temp.flipY = tile.spr.flipY;
	temp.layer = tile.spr.layer;
	temp.origin = tile.spr.origin;
	temp.position = tile.spr.position;
	temp.rotation = tile.spr.rotation;
	temp.size = tile.spr.size;
	temp.texRect = tile.spr.texRect;
	temp.texture = tile.spr.texture;

	t.setSprite(temp);
}

void MechaMan::Tileset::setTSet(int pitch_)
{
	pitch = pitch_; 
}
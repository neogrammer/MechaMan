#include "pch.h"
#include "Tileset.h"

void MechaMan::Tileset::addTile(Sprite& tile)
{
	tiles.push_back(Sprite{});
	Sprite& t = tiles[tiles.size() - 1];
	t.texture = tile.texture;
	t.texRect = tile.texRect;
	t.flipX = tile.flipX;
	t.flipY = tile.flipY;
	t.position = tile.position;
	t.size = tile.size;
	t.layer = tile.layer;
	t.rotation = tile.rotation;
}

void MechaMan::Tileset::setTSet(int pitch_, int width_, int height_)
{
	pitch = pitch_; 
	tw = width_; 
	th = height_;
}
flux/deck
{
	cull none
	{
		map $whiteimage
		rgbGen vertex
	}
}

textures/flux/concrete
{
	q3map_lightmapSampleSize 32
	{
		map $lightmap
		rgbGen identity
	}
	{
		map $whiteimage
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen const ( 0.22 0.28 0.34 )
	}
}

textures/flux/rail
{
	{
		map $lightmap
		rgbGen identity
	}
	{
		map $whiteimage
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen const ( 0.12 0.70 0.58 )
	}
}

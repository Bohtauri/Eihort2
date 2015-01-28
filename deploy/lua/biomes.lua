
require "assets"

function loadBiomeTextures( blocks, worldRoot )
	-- Find the biome textures
	local grassBiomeImage = loadTexture( "assets/minecraft/textures/colormap/grass.png", 256, 256 );
	local foliageBiomeImage	= loadTexture( "assets/minecraft/textures/colormap/foliage.png", 256, 256 );

	if not grassBiomeImage and not foliageBiomeImage then
		blocks:setBiomeRoot( "" );
	end

	-- Set up the biome channels
	blocks:setBiomeRoot( worldRoot .. "biomes/" );
	blocks:setBiomeDefaultPos( 56/255, 142/255 );
	if grassBiomeImage then
		blocks:setBiomeChannel( 0, false, grassBiomeImage );
	else
		blocks:setBiomeChannel( 0, 0x38/255, 0xa7/255, 0x88/255 );
	end
	if foliageBiomeImage then
		blocks:setBiomeChannel( 1, false, foliageBiomeImage );
		blocks:setBiomeChannel( 2, true, foliageBiomeImage );
	else
		blocks:setBiomeChannel( 1, 0x37/255, 0x9e/255, 0x7f/255 );
		blocks:setBiomeChannel( 2, 0x4c/255, 0x81/255, 0x67/255 );
	end
end



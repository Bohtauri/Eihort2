
-- Block ID -> Geometry description for Eihort 0.3.0+

-- The first part of this file contains helper functions used to construct
-- the various block geometry objects.
-- The actual Block ID -> Geometry list is at the end of the file

require "assets";

function loadBlockDesc()
	local blocks = eihort.newBlockDesc();

	-- Basic texture loading
	local textureCache = { };
	local defAniso = 'aniso_' .. tostring( Config.anisotropy or 0 );
	local function texUpload( img, ... )
		return img:uploadToGL( 'repeat', 'mag_nearest', 'min_linear', 'mip_linear', 'mipgen_alphawt_box', defAniso, ... );
	end
	function TEX( filename, ... )
		local cached = textureCache[filename];
		if cached then
			return cached;
		end
		local teximg = loadTexture( filename, 1, 1 );
		local tex = texUpload( teximg, ... );
		textureCache[filename] = tex;
		teximg:destroy();
		return tex;
	end
	function BTEX( blocktex, ... )
		return TEX( "assets/minecraft/textures/blocks/" .. blocktex .. ".png", ... );
	end

	-- Some special cases where the alpha channels must be modified
	local function Tex_AlphaFromGray( t1, t2, ... )
		t1:grayToAlpha( t2 );
		local tex = texUpload( t1, 'mipgen_box', ... );
		t1:destroy();
		t2:destroy();
		return tex;
	end
	local function BTEX_AlphaFromGray( basetex, graytex, ... )
		return Tex_AlphaFromGray( loadTexture( "assets/minecraft/textures/blocks/" .. basetex .. ".png", 1, 1 ), loadTexture( "assets/minecraft/textures/blocks/" .. graytex .. ".png", 1, 1 ), ... );
	end
	local function BTEX_InAlpha( alphatex, ... )
		local img = loadTexture( "assets/minecraft/textures/blocks/" .. alphatex .. ".png", 1, 1 );
		return Tex_AlphaFromGray( eihort.newImage( img.width, img.height, 0, 0, 0, 0 ), img, ... );
	end
	local function BTEX_NoAlpha( blocktex, ... )
		local img = loadTexture( "assets/minecraft/textures/blocks/" .. blocktex .. ".png", 1, 1 );
		return Tex_AlphaFromGray( img, eihort.newImage( img.width, img.height, 0, 0, 0, 0 ), ... );
	end

	-- Special cases for water, lava and portals
	local function BTEX_Water( watertex, columns, ... )
		columns = columns or 1;
		local teximg = loadTexture( "assets/minecraft/textures/blocks/" .. watertex .. ".png", columns, 1 );

		local teximg_sub = teximg:sub( 0, 0, teximg.width / columns, teximg.width / columns );
		local tex = texUpload( teximg_sub, ... );
		teximg:destroy();
		teximg_sub:destroy();
		return tex;
	end

	-- Load chest.png
	local chestImage = loadTexture( "assets/minecraft/textures/entity/chest/" .. "normal.png", 64, 64 );
	if math.fmod( chestImage.width, 64 ) ~= 0 or math.fmod( chestImage.height, 64 ) ~= 0 then
		error( "chest.png's dimensions must be multiples of 64." );
	end
	function ChestPNG( x, y, ... )
		local tw = chestImage.width/4;
		local th = chestImage.height/4;
		local tex = eihort.newImage( tw, th, 0, 0, 0, 0 );
		assert(y<4);
		if y == 0 then
			local t1 = chestImage:sub( tw*x*14/16, 0, tw*14/16, th*14/16 );
			tex:put( t1, tw*1/16, th*1/16 );
			t1:destroy();
		elseif y == 1 then
			local t1 = chestImage:sub( x*tw*14/16, th*14/16, tw*14/16, th*5/16 );
			local t2 = chestImage:sub( x*tw*14/16, th*34/16, tw*14/16, th*9/16 );
			tex:put( t1, tw*1/16, th*2/16 );
			tex:put( t2, tw*1/16, th*7/16 );
			t1:destroy();
			t2:destroy();
		else
			local t1 = chestImage:sub( tw*x*14/16, th*19/16, tw*14/16, th*14/16 );
			tex:put( t1, tw*1/16, th*1/16 );
			t1:destroy();
		end
		local tx = texUpload( tex, ... );
		tex:destroy();
		return tx;
	end
	chestSide = ChestPNG( 0, 1 );
	chestTop = ChestPNG( 1, 0 );


	-- Block geometry adapters
	local function DataAdapter( mask, ... ) -- "mask" is the bitmask for the data value.
		local geoms = { ... };
		local geomList = { };
		local solidity = 0x3f;
		for i, v in ipairs( geoms ) do
			if type( v ) == "number" then
				geomList[i] = geomList[v+1];
			else
				geomList[i] = v[1];
				solidity = eihort.intAnd( solidity, v[2] );
			end
		end
		return { eihort.geom.dataAdapter( mask, unpack( geomList ) ), solidity };
	end
	local function RotatingAdapter( normalGeom, faceGeom )
		return { eihort.geom.rotatingAdapter( normalGeom[1], faceGeom[1] ),
			     eihort.intAnd( normalGeom[2], eihort.intOr( 0x60, faceGeom[2] ) ) };
	end
	local function FaceBitAdapter( faceGeom )
		return { eihort.geom.faceBitAdapter( faceGeom[1] ), 0x00 };
	end
	local function FacingAdapter( normalGeom, faceGeom )
		if not normalGeom then
			normalGeom = { false, 0x00 };
		end
		return { eihort.geom.facingAdapter( normalGeom[1], faceGeom[1] ),
			     eihort.intAnd( normalGeom[2], eihort.intOr( 0x60, faceGeom[2] ) ) };
	end
	local function TopDifferentAdapter( normalGeom, diffGeom, topId, topGeom )
		return { eihort.geom.topDifferentAdapter( normalGeom[1], diffGeom[1], topId, topGeom and topGeom[1] ),
		         eihort.intAnd( normalGeom[2], diffGeom[2] ) };
	end
	local function SetTexScale( geom, s, t )
		geom[1]:setTexScale( s, t );
		return geom;
	end
	local function DelayRender( geom, delta )
		geom[1]:renderGroupAdd( delta or 1 );
		return geom;
	end

	-- Block Geometry creators
	-- In all the following, ... represents a list of textures with different
	-- meaning depending on how many textures are given:
	--     One texture: All sides of the block are covered with the same texture
	--     Two textures: The four sides take the first texture, the top and
	--                   bottom take the second
	--     Three textures: Sides, bottom, top
	--     Four textures: First two textures are the sides, which go on alternate
	--                    faces, last two textures are bottom and top
	--     Six textures: All 6 sides in the order X- X+ Z- Z+ Y- Y+
	local function OpaqueBlock( ... )
		return { eihort.geom.opaqueBlock( ... ), 0x3f };
	end
	local function HollowOpaqueBlock( ... )
		return { eihort.geom.opaqueBlock( ... ), 0x00 };
	end
	local function BrightOpaqueBlock( ... )
		return { eihort.geom.brightOpaqueBlock( ... ), 0x3f };
	end
	local function TransparentBlock( order, ... )
		return { eihort.geom.transparentBlock( order, ... ), 0x00 };
	end
	local function Slab( topOffset, bottomOffset, ... )
		local solidity = 0;
		if topOffset == 0 then
			solidity = solidity + 0x10;
		end
		if bottomOffset == 0 then
			solidity = solidity + 0x20;
		end
		return { eihort.geom.squashedBlock( topOffset, bottomOffset, ... ), solidity };
	end
	local function CompactedBlock( offsetXn, offsetXp, offsetZn, offsetZp, offsetYn, offsetYp, ... )
		return { eihort.geom.compactedBlock( offsetXn, offsetXp, offsetZn, offsetZp, offsetYn, offsetYp, ... ), 0x00 };
	end
	local function MultiBlockInBlock( offsets, ... )
		return { eihort.geom.multiBlockInBlock( offsets, ... ) , 0x00 };
	end
	local function MultiCompactedBlock( x, y, z, ... )
		return { eihort.geom.multiCompactedBlock( x, y, z, ... ) , 0x00 };
	end
	local function BiomeOpaqueBlock( biomeChannel, ... )
		return { eihort.geom.biomeOpaqueBlock( biomeChannel, ... ), 0x3f };
	end
	local function BiomeHollowOpaqueBlock( biomeChannel, ... )
		return { eihort.geom.biomeOpaqueBlock( biomeChannel, ... ), 0x00 };
	end
	local function BiomeAlphaOpaqueBlock( biomeChannel, ... )
		return { eihort.geom.biomeAlphaOpaqueBlock( biomeChannel, ... ), 0x3f };
	end
	local function Portal( tex )
		return { eihort.geom.portal( tex ), 0x00 };
	end
	local function Cactus( offset, ... )
		return { eihort.geom.cactus( offset, ... ), 0x00 };
	end
	local function BiomeCactus( biomeChannel, offset, ... )
		return { eihort.geom.biomeCactus( biomeChannel, offset, ... ), 0x00 };
	end
	local function Rail( straight, turn )
		return { eihort.geom.rail( straight, turn ), 0x00 };
	end
	local function Door( bottom, top )
		return DataAdapter( 0x8,
			{ eihort.geom.door( bottom ), 0x00 },
			{ eihort.geom.door( top ), 0x00 } );
	end
	local function Stairs( ... )
		return { eihort.geom.stairs( ... ), 0x00 };
	end
	local function Torch( tex )
		return { eihort.geom.torch( tex ), 0x00 };
	end
	local function Flower( tex )
		return { eihort.geom.flower( tex ), 0x00 };
	end
	local function BiomeFlower( biomeChannel, tex )
		return { eihort.geom.biomeFlower( biomeChannel, tex ), 0x00 };
	end
	local function Fence( tex )
		return MultiCompactedBlock(
		          {  10,  10,  -7,  -7, -11,  -2,
				     10,  10,  -7,  -7,  -5,  -8 },
				  {  -7,  -7,  10,  10, -11,  -2,
				     -7,  -7,  10,  10,  -5,  -8 },
				  {  -6,  -6,  -6,  -6,  -0,  -0 },
				  tex );
	end
	local function FenceGate( tex )
		return DataAdapter( 0x4, -- Fence Gate
		          DataAdapter( 0x3, -- fence fate CLOSED
		            MultiBlockInBlock( -- facing south
		              { -7,  -7,  0,  -14,  -5,   0, -- end bars
		                -7,  -7,  -14,  0,  -5,   0, 
		                -7,  -7,  -2,  -2, -12,  -1, -- gate parts
		                -7,  -7,  -6,  -6,  -9,  -4,
		                -7,  -7,  -2,  -2,  -6,  -7 },
		              tex ),
		            MultiBlockInBlock( -- facing west
		              {-14,   0,  -7,  -7,  -5,   0, -- end bars
		                 0, -14,  -7,  -7,  -5,   0, 
		                -2,  -2,  -7,  -7, -12,  -1, -- gate parts
		                -6,  -6,  -7,  -7,  -9,  -4,
		                -2,  -2,  -7,  -7,  -6,  -7 },
		              tex ),
		            MultiBlockInBlock( -- facing north
		              { -7,  -7,   0, -14,  -5,   0, -- end bars
		                -7,  -7, -14,   0,  -5,   0, 
		                -7,  -7,  -2,  -2,  -12, -1, -- gate parts
		                -7,  -7,  -6,  -6,  -9,  -4,
		                -7,  -7,  -2,  -2,  -6,  -7 },
		              tex ),
		            MultiBlockInBlock( -- facing east
		              {-14,   0,  -7,  -7,  -5,   0, -- end bars
		                 0, -14,  -7,  -7,  -5,   0, 
		                -2,  -2,  -7,  -7, -12,  -1, -- gate parts
		                -6,  -6,  -7,  -7,  -9,  -4,
		                -2,  -2,  -7,  -7,  -6,  -7 },
		              tex ) ),
		          DataAdapter( 0x3, -- fence gate OPEN
		            MultiBlockInBlock( -- facing south
		              { -7,  -7,   0,  -14,  -5,  0, -- end bars
		                -7,  -7, -14,   0,  -5,   0, 
		                -9,  -1,   0, -14, -12,  -1, -- gate right
		               -13,  -1,   0, -14,  -9,  -4,
		                -9,  -1,   0, -14,  -6,  -7,
		                -9,  -1, -14,   0, -12,  -1, -- gate left
		               -13,  -1, -14,   0,  -9,  -4,
		                -9,  -1, -14,   0,  -6,  -7 },
		              tex ),
		            MultiBlockInBlock( -- facing west
		              {-14,   0,  -7,  -7,  -5,   0, -- end bars
		                 0, -14,  -7,  -7,  -5,   0, 
		               -14,   0,  -1,  -9, -12,  -1, -- gate left
		               -14,   0,  -1, -13,  -9,  -4,
		               -14,   0,  -1,  -9,  -6,  -7,
		                 0, -14,  -1,  -9, -12,  -1, -- gate right
		                 0, -14,  -1, -13,  -9,  -4,
		                 0, -14,  -1,  -9,  -6,  -7 },
		              tex ),
		            MultiBlockInBlock( -- facing north
		              { -7,  -7,   0, -14,  -5,   0, -- end bars
		                -7,  -7, -14,   0,  -5,   0, 
		                -1,  -9,   0, -14, -12,  -1, -- gate left
		                -1, -13,   0, -14,  -9,  -4,
		                -1,  -9,   0, -14,  -6,  -7,
		                -1,  -9, -14,   0, -12,  -1, -- gate right
		                -1, -13, -14,   0,  -9,  -4,
		                -1,  -9, -14,   0,  -6,  -7 },
		              tex ),
		            MultiBlockInBlock( -- facing east
		              {-14,   0,  -7,  -7,  -5,   0, -- end bars
		                 0, -14,  -7,  -7,  -5,   0, 
		               -14,   0,  -9,  -1, -12,  -1, -- gate right
		               -14,   0, -13,  -1,  -9,  -4,
		               -14,   0,  -9,  -1,  -6,  -7,
		                 0, -14,  -9,  -1, -12,  -1, -- gate left
		                 0, -14, -13,  -1,  -9,  -4,
		                 0, -14,  -9,  -1,  -6,  -7 },
		              tex ) ) );
	end

	-------------------------------------------------------------------
	-- The actual block geometry list
	FurnaceBody = OpaqueBlock( BTEX("furnace_side"), BTEX("furnace_top") );
	MinecraftBlocks = {
		-- [<block id>] = <geometry>;
		[1]   = DataAdapter( 0x7, -- Stone Type
		          OpaqueBlock( BTEX("stone") ), -- stone
				  OpaqueBlock( BTEX("stone_granite") ), -- granite
				  OpaqueBlock( BTEX("stone_granite_smooth") ), -- polished granite
				  OpaqueBlock( BTEX("stone_diorite") ), -- diorite
				  OpaqueBlock( BTEX("stone_diorite_smooth") ), -- polished diorite
				  OpaqueBlock( BTEX("stone_andesite") ), -- andasite
				  OpaqueBlock( BTEX("stone_andesite_smooth") ), -- polished andasite
				  0 );
		[2]   = TopDifferentAdapter( -- Grass
		          BiomeAlphaOpaqueBlock( 0, BTEX_AlphaFromGray( "grass_side", "grass_side_overlay" ), BTEX_NoAlpha("dirt"), BTEX_InAlpha("grass_top") ), -- regular grass
		          OpaqueBlock( BTEX("grass_side_snowed"), BTEX("dirt") ), 78 ); -- snowy grass
		[3]   = DataAdapter( 0x3, -- Dirt
		          OpaqueBlock( BTEX("dirt") ), -- dirt
		          OpaqueBlock( BTEX("coarse_dirt") ), -- coarse
		          TopDifferentAdapter( -- Pozdol
					OpaqueBlock( BTEX("dirt_podzol_side"), BTEX("dirt"), BTEX("dirt_podzol_top") ), -- regular pozdol
					OpaqueBlock( BTEX("grass_side_snowed"), BTEX("dirt"), BTEX("dirt_podzol_top") ), 78 ), 0 ); -- snowy pozdol
		[4]   = OpaqueBlock( BTEX("cobblestone") ); -- Cobblestone
		[5]   = DataAdapter( 0x7, -- Wooden Plank
		          OpaqueBlock( BTEX("planks_oak") ), -- oak
		          OpaqueBlock( BTEX("planks_spruce") ), -- spruce
		          OpaqueBlock( BTEX("planks_birch") ), -- birch
		          OpaqueBlock( BTEX("planks_jungle") ), -- jungle
				  OpaqueBlock( BTEX("planks_acacia") ), -- acacia
				  OpaqueBlock( BTEX("planks_big_oak") ), -- dark oak
				  0, 0 );
		[6]   = DataAdapter( 0x7, -- Sapling
		          Flower( BTEX("sapling_oak") ), -- oak
		          Flower( BTEX("sapling_spruce") ), -- spruce
		          Flower( BTEX("sapling_birch") ), -- birch
		          Flower( BTEX("sapling_jungle") ), -- jungle
				  Flower( BTEX("sapling_acacia") ), -- acacia
				  Flower( BTEX("sapling_roofed_oak") ), -- dark oak
				  0, 0 );
		[7]   = OpaqueBlock( BTEX("bedrock") ); -- Bedrock
		[8]   = TransparentBlock( 1, BTEX_Water( "water_flow" ), BTEX_Water( "water_still" ) ); -- Water
		[9]   = TransparentBlock( 2, BTEX_Water( "water_still" ) ); -- Stationary Water
		[10]  = BrightOpaqueBlock( BTEX_Water( "lava_flow", 2 ), BTEX_Water( "lava_still" ) ); -- Lava
		[11]  = BrightOpaqueBlock( BTEX_Water( "lava_still" ) ); -- Stationary Lava
		[12]  = DataAdapter( 0x1, -- Sand
					OpaqueBlock( BTEX("sand") ), -- regular
					OpaqueBlock( BTEX("red_sand") ) ); -- red
		[13]  = OpaqueBlock( BTEX("gravel") ); -- Gravel
		[14]  = OpaqueBlock( BTEX("gold_ore") ); -- Gold Ore
		[15]  = OpaqueBlock( BTEX("iron_ore") ); -- Iron Ore
		[16]  = OpaqueBlock( BTEX("coal_ore") ); -- Coal Ore
		[17]  = DataAdapter( 0x3, -- Wood
		          OpaqueBlock( BTEX("log_oak"), BTEX("log_oak_top") ), -- oak
		          OpaqueBlock( BTEX("log_spruce"), BTEX("log_spruce_top") ), -- spruce
		          OpaqueBlock( BTEX("log_birch"), BTEX("log_birch_top") ), -- birch
		          OpaqueBlock( BTEX("log_jungle"), BTEX("log_jungle_top") ) ); -- jungle
		[18]  = DataAdapter( 0x3, -- Leaves
		          BiomeHollowOpaqueBlock( 1, BTEX("leaves_oak") ), -- oak
		          BiomeHollowOpaqueBlock( 2, BTEX("leaves_spruce") ), -- spruce
		          BiomeHollowOpaqueBlock( 1, BTEX("leaves_birch") ), -- birch
		          BiomeHollowOpaqueBlock( 1, BTEX("leaves_jungle") ) ); -- jungle
		[19]  = DataAdapter( 0x1, -- Sponge
		          OpaqueBlock( BTEX("sponge") ), -- dry
		          OpaqueBlock( BTEX("sponge_wet") ) ); -- wet
		[20]  = HollowOpaqueBlock( BTEX("glass") ); -- Glass
		[21]  = OpaqueBlock( BTEX("lapis_ore") ); -- Lapis Lazuli Ore
		[22]  = OpaqueBlock( BTEX("lapis_block") ); -- Lapis Lazuli Block
		[23]  = DataAdapter( 0x6, -- Dispenser
					DataAdapter( 0x1,
						OpaqueBlock( BTEX("furnace_top"), BTEX("dispenser_front_vertical"), BTEX("furnace_top") ), -- facing down
						OpaqueBlock( BTEX("furnace_top"), BTEX("furnace_top"), BTEX("dispenser_front_vertical") ) ), -- facing up
					DataAdapter( 0x1,
						OpaqueBlock( BTEX("furnace_side"), BTEX("furnace_side"), BTEX("dispenser_front_horizontal"), BTEX("furnace_side"), BTEX("furnace_top"), BTEX("furnace_top") ), -- facing north
						OpaqueBlock( BTEX("furnace_side"), BTEX("furnace_side"), BTEX("furnace_side"), BTEX("dispenser_front_horizontal"), BTEX("furnace_top"), BTEX("furnace_top") ) ), -- facing south
					DataAdapter( 0x1,
						OpaqueBlock( BTEX("dispenser_front_horizontal"), BTEX("furnace_side"), BTEX("furnace_side"), BTEX("furnace_side"), BTEX("furnace_top"), BTEX("furnace_top") ), -- facing west
						OpaqueBlock( BTEX("furnace_side"), BTEX("dispenser_front_horizontal"), BTEX("furnace_side"), BTEX("furnace_side"), BTEX("furnace_top"), BTEX("furnace_top") ) ), -- facing east
					0, 0 );
		[24]  = DataAdapter( 0x3, -- Sandstone
		          OpaqueBlock( BTEX("sandstone_normal"), BTEX("sandstone_bottom"), BTEX("sandstone_top") ), -- regular
		          OpaqueBlock( BTEX("sandstone_carved"), BTEX("sandstone_top"), BTEX("sandstone_top") ), -- chiseled
		          OpaqueBlock( BTEX("sandstone_smooth"), BTEX("sandstone_top"), BTEX("sandstone_top") ), -- smooth
		          0 );
		[25]  = OpaqueBlock( BTEX("noteblock") ); -- Note Block
		-- [26] -- Bed
		[27]  = DataAdapter( 0x8, -- Powered Rails
		          Rail( BTEX("rail_golden") ), -- not powered
		          Rail( BTEX("rail_golden_powered") ) ); -- powered
		[28]  = DataAdapter( 0x8, -- Detector Rails
		          Rail( BTEX("rail_detector") ), -- not powered
		          Rail( BTEX("rail_detector_powered") ) ); -- powered
		-- Sticky Piston needs special code: the trick is that the side tiles need to rotate to face the front piece TODO
		[29]  = OpaqueBlock( BTEX("piston_side"), BTEX("piston_bottom"), BTEX("piston_top_sticky") ); -- Sticky Piston up
		[30]  = Flower( BTEX("web") ); -- Web
		[31]  = DataAdapter( 0x3, -- Tall Grass
		          Flower( BTEX("deadbush") ), -- dead bush like
		          BiomeFlower( 0, BTEX("tallgrass") ), -- tall grass
		          BiomeFlower( 0, BTEX("fern") ), 0 ); -- fern
		[32]  = Flower( BTEX("deadbush") ), -- Dead Bush
		-- Piston needs special code: the trick is that the side tiles need to rotate to face the front piece TODO
		[33]  = OpaqueBlock( BTEX("piston_side"), BTEX("piston_bottom"), BTEX("piston_top_normal") ); -- Piston up
		-- [34] -- Piston Extension
		[35]  = DataAdapter( 0xf, -- Wool
		          OpaqueBlock( BTEX("wool_colored_white") ), -- white
		          OpaqueBlock( BTEX("wool_colored_orange") ), -- orange
		          OpaqueBlock( BTEX("wool_colored_magenta") ), -- magenta
		          OpaqueBlock( BTEX("wool_colored_light_blue") ), -- light blue
		          OpaqueBlock( BTEX("wool_colored_yellow") ), -- yellow
		          OpaqueBlock( BTEX("wool_colored_lime") ), -- lime
		          OpaqueBlock( BTEX("wool_colored_pink") ), -- pink
		          OpaqueBlock( BTEX("wool_colored_gray") ), -- grey
		          OpaqueBlock( BTEX("wool_colored_silver") ), -- light grey	
		          OpaqueBlock( BTEX("wool_colored_cyan") ), -- cyan
		          OpaqueBlock( BTEX("wool_colored_purple") ), -- purple
		          OpaqueBlock( BTEX("wool_colored_blue") ), -- blue
		          OpaqueBlock( BTEX("wool_colored_brown") ), -- brown
		          OpaqueBlock( BTEX("wool_colored_green") ), -- green
		          OpaqueBlock( BTEX("wool_colored_red") ), -- red
		          OpaqueBlock( BTEX("wool_colored_black") ) ); -- black
		[37]  = Flower( BTEX("flower_dandelion") ); -- Dandelion
		[38]  = DataAdapter( 0xf, -- Flowers
		          Flower( BTEX("flower_rose") ), -- poppy
				  Flower( BTEX("flower_blue_orchid") ), -- blue orchid
				  Flower( BTEX("flower_allium") ), -- allium
				  Flower( BTEX("flower_houstonia") ), -- azure bluet
				  Flower( BTEX("flower_tulip_red") ), -- red tulip
				  Flower( BTEX("flower_tulip_orange") ), -- orange tulip
				  Flower( BTEX("flower_tulip_white") ), -- white tulip
				  Flower( BTEX("flower_tulip_pink") ), -- pink tulip
				  Flower( BTEX("flower_oxeye_daisy") ), -- oxeye daisy
				  0, 0, 0, 0, 0, 0, 0 );
		[39]  = Flower( BTEX("mushroom_brown") ); -- Brown Mushroom
		[40]  = Flower( BTEX("mushroom_red") ); -- Red Mushroom
		[41]  = OpaqueBlock( BTEX("gold_block") ); -- Gold block
		[42]  = OpaqueBlock( BTEX("iron_block") ); -- Iron block
		[43]  = DataAdapter( 0x7, -- Double Slab
		          DataAdapter(0x8, -- Stone
					OpaqueBlock( BTEX("stone_slab_side"), BTEX("stone_slab_top") ), -- stone double slab
					OpaqueBlock( BTEX("stone_slab_top") ) ), -- smooth stone slab
		          DataAdapter(0x8, -- Sandstone
					OpaqueBlock( BTEX("sandstone_normal"), BTEX("sandstone_bottom"), BTEX("sandstone_top") ), -- sandstone double slab
					OpaqueBlock( BTEX("sandstone_top") ) ), -- smooth sandstone slab
		          OpaqueBlock( BTEX("planks_oak") ), -- wooden planks
		          OpaqueBlock( BTEX("cobblestone") ), -- cobblestone
		          OpaqueBlock( BTEX("brick") ), -- brick
		          OpaqueBlock( BTEX("stonebrick") ), -- stone brick
				  OpaqueBlock( BTEX("nether_brick") ), -- nether brick
				  OpaqueBlock( BTEX("quartz_block_side"), BTEX("quartz_block_bottom"), BTEX("quartz_block_top") ), -- nether Quartz
		          0, 0 );
		[44]  = DataAdapter( 0x8, -- Slabs
		          DataAdapter( 0x7, -- Regular Slabs
		            Slab( -8, 0, BTEX("stone_slab_side"), BTEX("stone_slab_top") ), -- stone slab
		            Slab( -8, 0, BTEX("sandstone_normal"), BTEX("sandstone_bottom"), BTEX("sandstone_top") ), -- sandstone slab
		            Slab( -8, 0, BTEX("planks_oak") ), -- wooden plank slab
		            Slab( -8, 0, BTEX("cobblestone") ), -- cobblestone slab
		            Slab( -8, 0, BTEX("brick") ), -- brick slab
		            Slab( -8, 0, BTEX("stonebrick") ), -- stone brick
					Slab( -8, 0, BTEX("nether_brick") ), -- nether brick
					Slab( -8, 0, BTEX("quartz_block_side"), BTEX("quartz_block_bottom"), BTEX("quartz_block_top") ), -- quartz slab
		            0, 0 ),
		          DataAdapter( 0x7, -- Upside-down slabs
		            Slab( 0, -8, BTEX("stone_slab_side"), BTEX("stone_slab_top") ), -- stone slab
		            Slab( 0, -8, BTEX("sandstone_normal"), BTEX("sandstone_bottom"), BTEX("sandstone_top") ), -- sandstone slab
		            Slab( 0, -8, BTEX("planks_oak") ), -- wooden plank slab
		            Slab( 0, -8, BTEX("cobblestone") ), -- cobblestone slab
		            Slab( 0, -8, BTEX("brick") ), -- brick slab
		            Slab( 0, -8, BTEX("stonebrick") ), -- stone brick
					Slab( 0, -8, BTEX("nether_brick") ), -- nether brick
					Slab( 0, -8, BTEX("quartz_block_side"), BTEX("quartz_block_bottom"), BTEX("quartz_block_top") ), -- quartz slab
		            0, 0 ) );
		[45]  = OpaqueBlock( BTEX("brick") ); -- Brick
		[46]  = OpaqueBlock( BTEX("tnt_side"), BTEX("tnt_bottom"), BTEX("tnt_top") ); -- TNT
		[47]  = OpaqueBlock( BTEX("bookshelf"), BTEX("planks_oak") ); -- Bookshelf
		[48]  = OpaqueBlock( BTEX("cobblestone_mossy") ); -- Mossy Cobblestone
		[49]  = OpaqueBlock( BTEX("obsidian") ); -- Obsidian
		[50]  = Torch( BTEX("torch_on","clamp") ); -- Torch
		-- [51] -- Fire
		[52]  = HollowOpaqueBlock( BTEX("mob_spawner") ); -- Monster spawner
		[53]  = Stairs( BTEX("planks_oak") ); -- Oak Wood Stairs
		[54]  = CompactedBlock( -1, -1, -1, -1, 0, -2, chestSide, chestTop ); -- Chest
		-- [55] -- Redstone Wire
		[56]  = OpaqueBlock( BTEX("diamond_ore") ); -- Diamond Ore
		[57]  = OpaqueBlock( BTEX("diamond_block") ); -- Diamond Block
		[58]  = OpaqueBlock( BTEX("crafting_table_front"), BTEX("crafting_table_side"), BTEX("planks_oak"), BTEX("crafting_table_top") ); -- Crafting Table
		[59]  = DataAdapter( 0x7, -- Wheat Crops
		          Cactus( -4, BTEX("wheat_stage_0"), 0 ), -- growthstate 0
		          Cactus( -4, BTEX("wheat_stage_1"), 0 ), -- growthstate 1
		          Cactus( -4, BTEX("wheat_stage_2"), 0 ), -- growthstate 2
		          Cactus( -4, BTEX("wheat_stage_3"), 0 ), -- growthstate 3
		          Cactus( -4, BTEX("wheat_stage_4"), 0 ), -- growthstate 4
		          Cactus( -4, BTEX("wheat_stage_5"), 0 ), -- growthstate 5
		          Cactus( -4, BTEX("wheat_stage_6"), 0 ), -- growthstate 6
		          Cactus( -4, BTEX("wheat_stage_7"), 0 ) ); -- growthstate 7 
		[60]  = DataAdapter( 0x1, -- Farmland
		          CompactedBlock( 0, 0, 0, 0, 0, -1, BTEX("dirt"), BTEX("dirt"), BTEX("farmland_dry") ), -- not hydrated
		          CompactedBlock( 0, 0, 0, 0, 0, -1, BTEX("dirt"), BTEX("dirt"), BTEX("farmland_wet") ) ); -- hydrated
		[61]  = FacingAdapter( FurnaceBody, OpaqueBlock( BTEX("furnace_front_off") ) ); -- Furnace
		[62]  = FacingAdapter( FurnaceBody, OpaqueBlock( BTEX("furnace_front_on") ) ); -- Burning furnace
		-- [63] -- Sign Post
		[64]  = Door( BTEX("door_wood_lower"), BTEX("door_wood_upper") ); -- Oak Wood Door
		[65]  = FacingAdapter( false, Cactus( -15, BTEX("ladder") ) ); -- Ladder
		[66]  = Rail( BTEX("rail_normal"), BTEX("rail_normal_turned") ); -- Rails
		[67]  = Stairs( BTEX("cobblestone") ); -- Cobblestone Stairs
		[68]  = DataAdapter( 0x7, -- Wall Sign
		          SetTexScale( CompactedBlock( 0, 0, -14, 0, -4, -4, BTEX("planks_oak") ), 2, 2 ), -- north
		          0, 0,
		          SetTexScale( CompactedBlock( 0, 0, 0, -14, -4, -4, BTEX("planks_oak") ), 2, 2 ), -- south
		          SetTexScale( CompactedBlock( -14, 0, 0, 0, -4, -4, BTEX("planks_oak") ), 2, 2 ), -- west
		          SetTexScale( CompactedBlock( 0, -14, 0, 0, -4, -4, BTEX("planks_oak") ), 2, 2 ), -- east
		          0, 0 );
		-- [69] -- Lever
		[70]  = CompactedBlock( -1, -1, -1, -1, 0, -15, BTEX("stone") ); -- Stone Pressure Plate
		[71]  = Door( BTEX("door_iron_lower"), BTEX("door_iron_upper") ); -- Iron Door
		[72]  = CompactedBlock( -1, -1, -1, -1, 0, -15, BTEX("planks_oak") ); -- Wooden Pressure plate
		[73]  = OpaqueBlock( BTEX("redstone_ore") ); -- Redstone Ore
		[74]  = OpaqueBlock( BTEX("redstone_ore") ); -- Glowing Redstone Ore
		[75]  = Torch( BTEX("redstone_torch_off","clamp") ); -- Redstone Torch (off)
		[76]  = Torch( BTEX("redstone_torch_on","clamp") ); -- Redstone Torch (on)
		[77]  = DataAdapter( 0x7+0x8, -- Stone Button
		          CompactedBlock( -5, -5, -6, -6, -14, 0, BTEX("stone") ), -- off, bottom
				  CompactedBlock( 0, -14, -5, -5, -6, -6, BTEX("stone") ), -- off, east
		          CompactedBlock( -14, 0, -5, -5, -6, -6, BTEX("stone") ), -- off, west
				  CompactedBlock( -5, -5, 0, -14, -6, -6, BTEX("stone") ), -- off, south
				  CompactedBlock( -5, -5, -14, 0, -6, -6, BTEX("stone") ), -- off, north
				  CompactedBlock( -5, -5, -6, -6, 0, -14, BTEX("stone") ), -- off, top
				  0, 0,
		          CompactedBlock( -5, -5, -6, -6, -15, 0, BTEX("stone") ), -- on, bottom
				  CompactedBlock( 0, -15, -5, -5, -6, -6, BTEX("stone") ), -- on, east
		          CompactedBlock( -15, 0, -5, -5, -6, -6, BTEX("stone") ), -- on, west
				  CompactedBlock( -5, -5, 0, -15, -6, -6, BTEX("stone") ), -- on, south
		          CompactedBlock( -5, -5, -15, 0, -6, -6, BTEX("stone") ), -- on, north
		          CompactedBlock( -5, -5, -6, -6, 0, -15, BTEX("stone") ), -- on, top
				  0, 0 );
		[78]  = DataAdapter( 0x7, -- Snow
		          Slab( -14, 0, BTEX("snow") ), -- 1 layer
		          Slab( -12, 0, BTEX("snow") ), -- 2 layer
		          Slab( -10, 0, BTEX("snow") ), -- 3 layer
		          Slab( -8, 0, BTEX("snow") ), -- 4 layer
		          Slab( -6, 0, BTEX("snow") ), -- 5 layer
		          Slab( -4, 0, BTEX("snow") ), -- 6 layer
		          Slab( -2, 0, BTEX("snow") ), -- 7 layer
		          OpaqueBlock( BTEX("snow") ) ); -- 8 layer/block
		[79]  = DelayRender( TransparentBlock( 0, BTEX("ice") ), 5 ); -- Ice
		[80]  = OpaqueBlock( BTEX("snow") ); -- Snow Block
		[81]  = Cactus( -1, BTEX("cactus_side"), BTEX("cactus_bottom"), BTEX("cactus_top") ); -- Cactus
		[82]  = OpaqueBlock( BTEX("clay") ); -- Clay
		[83]  = Flower( BTEX("reeds") ); -- Sugar Cane
		[84]  = OpaqueBlock( BTEX("noteblock"), BTEX("noteblock"), BTEX("jukebox_top") ); -- Jukebox
		[85]  = Fence( BTEX("planks_oak") ); -- Oak Wood Fence
		[86]  = RotatingAdapter( OpaqueBlock( BTEX("pumpkin_side"), BTEX("pumpkin_top") ), OpaqueBlock( BTEX("pumpkin_face_off") ) ); -- Pumpkin
		[87]  = OpaqueBlock( BTEX("netherrack") ); -- Netherrack
		[88]  = OpaqueBlock( BTEX("soul_sand") ); -- Soul Sand
		[89]  = OpaqueBlock( BTEX("glowstone") ); -- Glowstone Block
		[90]  = DelayRender( Portal( BTEX_Water("portal") ), 10 ); -- Portal
		[91]  = RotatingAdapter( OpaqueBlock( BTEX("pumpkin_side"), BTEX("pumpkin_top") ), OpaqueBlock( BTEX("pumpkin_face_on") ) ); -- Jack-o-lantern
		[92]  = DataAdapter( 0x7, -- Cake
		          CompactedBlock( -1, -1, -1, -1, 0, -8, BTEX("cake_side"), BTEX("cake_bottom"), BTEX("cake_top") ), -- full cake
		          CompactedBlock( -3, -1, -1, -1, 0, -8, BTEX("cake_inner"), BTEX("cake_side"), BTEX("cake_side"), BTEX("cake_side"), BTEX("cake_bottom"), BTEX("cake_top") ), -- eaten 1
		          CompactedBlock( -5, -1, -1, -1, 0, -8, BTEX("cake_inner"), BTEX("cake_side"), BTEX("cake_side"), BTEX("cake_side"), BTEX("cake_bottom"), BTEX("cake_top") ), -- eaten 2
		          CompactedBlock( -7, -1, -1, -1, 0, -8, BTEX("cake_inner"), BTEX("cake_side"), BTEX("cake_side"), BTEX("cake_side"), BTEX("cake_bottom"), BTEX("cake_top") ), -- eaten 3
		          CompactedBlock( -9, -1, -1, -1, 0, -8, BTEX("cake_inner"), BTEX("cake_side"), BTEX("cake_side"), BTEX("cake_side"), BTEX("cake_bottom"), BTEX("cake_top") ), -- eaten 4
		          CompactedBlock( -11, -1, -1, -1, 0, -8, BTEX("cake_inner"), BTEX("cake_side"), BTEX("cake_side"), BTEX("cake_side"), BTEX("cake_bottom"), BTEX("cake_top") ), 0, 0 ); -- eaten 5
		-- [93] -- Redstone repeater (off)
		-- [94] -- Redstone repeater (on)
		[95]  = DataAdapter( 0xf, -- Stained Glass
				  DelayRender( TransparentBlock( 0, BTEX("glass_white") ), 15 ), -- white
				  DelayRender( TransparentBlock( 0, BTEX("glass_orange") ), 15 ), -- orange
				  DelayRender( TransparentBlock( 0, BTEX("glass_magenta") ), 15 ), -- magenta
				  DelayRender( TransparentBlock( 0, BTEX("glass_light_blue") ), 15 ), -- light blue
				  DelayRender( TransparentBlock( 0, BTEX("glass_yellow") ), 15 ), -- yellow
				  DelayRender( TransparentBlock( 0, BTEX("glass_lime") ), 15 ), -- lime
				  DelayRender( TransparentBlock( 0, BTEX("glass_pink") ), 15 ), -- pink
				  DelayRender( TransparentBlock( 0, BTEX("glass_gray") ), 15 ), -- grey
				  DelayRender( TransparentBlock( 0, BTEX("glass_silver") ), 15 ), -- light grey
				  DelayRender( TransparentBlock( 0, BTEX("glass_cyan") ), 15 ), -- cyan
				  DelayRender( TransparentBlock( 0, BTEX("glass_purple") ), 15 ), -- purple
				  DelayRender( TransparentBlock( 0, BTEX("glass_blue") ), 15 ), -- blue				  
				  DelayRender( TransparentBlock( 0, BTEX("glass_brown") ), 15 ), -- brown
				  DelayRender( TransparentBlock( 0, BTEX("glass_green") ), 15 ), -- green
				  DelayRender( TransparentBlock( 0, BTEX("glass_red") ), 15 ), -- red
				  DelayRender( TransparentBlock( 0, BTEX("glass_black") ), 15 ) ); -- black
		[96]  = DataAdapter( 0x8, -- Wooden Trapdoor
		          DataAdapter( 0x4,
		            Slab( -13, 0, BTEX("trapdoor") ), -- bottom
		            DataAdapter( 0x3,
		              CompactedBlock( 0, 0, -13, 0, 0, 0, BTEX("trapdoor") ), -- north
		              CompactedBlock( 0, 0, 0, -13, 0, 0, BTEX("trapdoor") ), -- south
		              CompactedBlock( -13, 0, 0, 0, 0, 0, BTEX("trapdoor") ), -- west
		              CompactedBlock( 0, -13, 0, 0, 0, 0, BTEX("trapdoor") ) ) ), -- east
		          DataAdapter( 0x4,
		            Slab( 0, -13, BTEX("trapdoor") ), -- top
		            DataAdapter( 0x3,
		              CompactedBlock( 0, 0, -13, 0, 0, 0, BTEX("trapdoor") ), -- north
		              CompactedBlock( 0, 0, 0, -13, 0, 0, BTEX("trapdoor") ), -- south
		              CompactedBlock( -13, 0, 0, 0, 0, 0, BTEX("trapdoor") ), -- west
		              CompactedBlock( 0, -13, 0, 0, 0, 0, BTEX("trapdoor") ) ) ) ); -- east
		[97]  = DataAdapter( 0x7, -- Hidden Silverfish
		          OpaqueBlock( BTEX("stone") ), -- stone
		          OpaqueBlock( BTEX("cobblestone") ), -- cobblestone
		          OpaqueBlock( BTEX("stonebrick") ), -- stonebrick
		          OpaqueBlock( BTEX("stonebrick_mossy") ), -- mossy stonebrick
		          OpaqueBlock( BTEX("stonebrick_cracked") ), -- cracked stonebrick
		          OpaqueBlock( BTEX("stonebrick_carved") ),  -- chiseld stonebrick
				  0, 0 );
		[98]  = DataAdapter( 0x3, -- Stone Brick
		          OpaqueBlock( BTEX("stonebrick") ), -- regular
		          OpaqueBlock( BTEX("stonebrick_mossy") ), -- mossy
		          OpaqueBlock( BTEX("stonebrick_cracked") ), -- cracked
		          OpaqueBlock( BTEX("stonebrick_carved") ) ); -- chisled
		[99] = DataAdapter( 0xf, -- Brown Mushroom
		          OpaqueBlock( BTEX("mushroom_block_skin_brown") ), -- pores
		          OpaqueBlock( BTEX("mushroom_block_skin_brown"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_brown"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_brown") ), -- west + north
		          OpaqueBlock( BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_brown"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_brown") ), -- north
		          OpaqueBlock( BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_brown"), BTEX("mushroom_block_skin_brown"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_brown") ), -- east + north
		          OpaqueBlock( BTEX("mushroom_block_skin_brown"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_brown") ), -- west
		          OpaqueBlock( BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_brown") ), -- top
		          OpaqueBlock( BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_brown"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_brown") ), -- east
		          OpaqueBlock( BTEX("mushroom_block_skin_brown"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_brown"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_brown") ), -- west + south
		          OpaqueBlock( BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_brown"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_brown") ), -- south
		          OpaqueBlock( BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_brown"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_brown"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_brown") ), -- east + south
		          OpaqueBlock( BTEX("mushroom_block_skin_stem"), BTEX("mushroom_block_inside") ), -- stem
		          0, 0, 0, 0, 0 );
		[100] = DataAdapter( 0xf, -- Red Mushroom
		          OpaqueBlock( BTEX("mushroom_block_skin_red") ), -- pores
		          OpaqueBlock( BTEX("mushroom_block_skin_red"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_red"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_red") ), -- west + north
		          OpaqueBlock( BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_red"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_red") ), -- north
		          OpaqueBlock( BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_red"), BTEX("mushroom_block_skin_red"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_red") ), -- east + north
		          OpaqueBlock( BTEX("mushroom_block_skin_red"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_red") ), -- west
		          OpaqueBlock( BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_red") ), -- top
		          OpaqueBlock( BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_red"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_red") ), -- east
		          OpaqueBlock( BTEX("mushroom_block_skin_red"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_red"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_red") ), -- west + south
		          OpaqueBlock( BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_red"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_red") ), -- south
		          OpaqueBlock( BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_red"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_red"), BTEX("mushroom_block_inside"), BTEX("mushroom_block_skin_red") ), -- east + south
		          OpaqueBlock( BTEX("mushroom_block_skin_stem"), BTEX("mushroom_block_inside") ), -- stem
		          0, 0, 0, 0, 0 );
		-- [101] -- Iron Bars TODO
		[102] = HollowOpaqueBlock( BTEX("glass") ); -- Glass Panes (for now, a solid glass block - TODO)
		[103] = OpaqueBlock( BTEX("melon_side"), BTEX("melon_top") ); -- Melon
		-- [104] -- Pumpkin Stem
		-- [105] -- Melon Stem
		[106] = FaceBitAdapter( BiomeCactus( 1, {-15}, BTEX("vine"), BTEX("vine"), 0 ) ); -- Vines
		[107] = FenceGate( BTEX("planks_oak") ); -- Oak Fence Gate
		[108] = Stairs( BTEX("brick") ); -- Brick Stairs
		[109] = Stairs( BTEX("stonebrick") ); -- Stone Brick Stairs
		[110] = TopDifferentAdapter( -- Mycelium
					OpaqueBlock( BTEX("mycelium_side"), BTEX("dirt"), BTEX("mycelium_top") ), -- regular mycelium
					OpaqueBlock( BTEX("grass_side_snowed"), BTEX("dirt"), BTEX("mycelium_top") ), 78 ); -- snowy mycelium
		[111] = BiomeCactus( 0, {-15}, 0, 0, BTEX("waterlily") ); -- Lily pad
		[112] = OpaqueBlock( BTEX("nether_brick") ); -- Nether Brick
		[113] = Fence( BTEX("nether_brick") ); -- Nether Brick Fence
		[114] = Stairs( BTEX("nether_brick") ); -- Nether Brick Stairs
		[115] = DataAdapter( 0x3, -- Nether Wart
		          Cactus( -4, BTEX("nether_wart_stage_0"), 0 ), -- growthstate 0
		          Cactus( -4, BTEX("nether_wart_stage_1"), 0 ), 1, -- growthstate 1
		          Cactus( -4, BTEX("nether_wart_stage_2"), 0 ) ); -- growthstate 2
		[116] = Slab( -4, 0, BTEX("enchanting_table_side"), BTEX("enchanting_table_bottom"), BTEX("enchanting_table_top") ); -- Enchantment Table
		-- [117] -- Brewing Stand
		-- [118] -- Cauldron
		-- [119] -- End Portal
		[120] = CompactedBlock( 0, 0, 0, 0, 0, -3, BTEX("endframe_side"), BTEX("end_stone"), BTEX("endframe_top") ); -- End Portal Frame
		[121] = OpaqueBlock( BTEX("end_stone") ); -- End Stone
		[122] = MultiBlockInBlock( -- Dragon Egg
		          { -6, -6, -6, -6, -15,   0,
		            -5, -5, -5, -5, -14,  -1,
		            -4, -4, -4, -4, -13,  -2,
		            -3, -3, -3, -3, -11,  -3,
		            -2, -2, -2, -2,  -8,  -5,
		            -1, -1, -1, -1,  -3,  -8,
		            -3, -3, -3, -3,  -1, -13,
		            -6, -6, -6, -6,   0, -15}, -- Egg
		          BTEX("dragon_egg") );
		[123] = OpaqueBlock( BTEX("redstone_lamp_off") ); -- Redstone Lamp (off)
		[124] = OpaqueBlock( BTEX("redstone_lamp_on") ); -- Redstone Lamp (on)
		[125] = DataAdapter( 0x7, -- Wooden Double-Slab
		          OpaqueBlock( BTEX("planks_oak") ), -- oak
		          OpaqueBlock( BTEX("planks_spruce") ), -- spruce
		          OpaqueBlock( BTEX("planks_birch") ), -- birch
		          OpaqueBlock( BTEX("planks_jungle") ), -- jungle
		          OpaqueBlock( BTEX("planks_acacia") ), -- acacia
		          OpaqueBlock( BTEX("planks_big_oak") ), -- dark oak
				  0, 0 );
		[126] = DataAdapter( 0x7+0x8, -- Wooden Slab
		          Slab( -8, 0, BTEX("planks_oak") ), -- bottom, oak
		          Slab( -8, 0, BTEX("planks_spruce") ), -- bottom, spruce
		          Slab( -8, 0, BTEX("planks_birch") ), -- bottom, birch
		          Slab( -8, 0, BTEX("planks_jungle") ), -- bottom, jungle
		          Slab( -8, 0, BTEX("planks_acacia") ), -- bottom, acacia
		          Slab( -8, 0, BTEX("planks_big_oak") ), -- bottom, dark oak
				  0, 0,
		          Slab( 0, -8, BTEX("planks_oak") ), -- top, oak
		          Slab( 0, -8, BTEX("planks_spruce") ), -- top, spruce
		          Slab( 0, -8, BTEX("planks_birch") ), -- top, birch
		          Slab( 0, -8, BTEX("planks_jungle") ), -- top, jungle
		          Slab( 0, -8, BTEX("planks_acacia") ), -- top, acacia
		          Slab( 0, -8, BTEX("planks_big_oak") ), -- top, dark oak
				  0, 0 );
		-- [127] -- Cocoa Plant
		[128] = DataAdapter( 0x4, -- Sandstone Stairs
		          Stairs( BTEX("sandstone_normal"), BTEX("sandstone_bottom"), BTEX("sandstone_top") ), -- regular
				  Stairs( BTEX("sandstone_normal"), BTEX("sandstone_top"), BTEX("sandstone_bottom") ) ); -- upside-down
		[129] = OpaqueBlock( BTEX("emerald_ore") ); -- Emerald Ore
		-- [130] -- Ender Chest
		-- [131] -- Tripwire Hook
		-- [132] -- Tripwire
		[133] = OpaqueBlock( BTEX("emerald_block") ); -- Block of Emerald
		[134] = Stairs( BTEX("planks_spruce") ); -- Spruce Wood Stairs
		[135] = Stairs( BTEX("planks_birch") ); -- Birch Wood Stairs
		[136] = Stairs( BTEX("planks_jungle") ); -- Jungle Wood Stairs
		[137] = OpaqueBlock( BTEX("command_block") ); -- Command Block
		-- [138] -- Beacon Block
		-- [139] -- Cobblestone Wall
		-- [140] -- Flower Pot
		[141] = DataAdapter( 0x7, -- Carrots
		          Cactus( -4, BTEX("carrots_stage_0"), 0 ), Cactus( -4, BTEX("carrots_stage_0"), 0 ), -- growthstate 0
		          Cactus( -4, BTEX("carrots_stage_1"), 0 ), Cactus( -4, BTEX("carrots_stage_1"), 0 ), -- growthstate 1
		          Cactus( -4, BTEX("carrots_stage_2"), 0 ), Cactus( -4, BTEX("carrots_stage_2"), 0 ), Cactus( -4, BTEX("carrots_stage_2"), 0 ), -- growthstate 2
		          Cactus( -4, BTEX("carrots_stage_3"), 0 ) ); -- growthstate 3
		[142] = DataAdapter( 0x7, -- Potatoes
		          Cactus( -4, BTEX("potatoes_stage_0"), 0 ), Cactus( -4, BTEX("potatoes_stage_0"), 0 ), -- growthstate 0
		          Cactus( -4, BTEX("potatoes_stage_1"), 0 ), Cactus( -4, BTEX("potatoes_stage_1"), 0 ), -- growthstate 1
		          Cactus( -4, BTEX("potatoes_stage_2"), 0 ), Cactus( -4, BTEX("potatoes_stage_2"), 0 ), Cactus( -4, BTEX("potatoes_stage_2"), 0 ), -- growthstate 2
		          Cactus( -4, BTEX("potatoes_stage_3"), 0 ) ); -- growthstate 3
		[143] = DataAdapter( 0x7+0x8, -- Wooden Button
		          CompactedBlock( -5, -5, -6, -6, -14, 0, BTEX("planks_oak") ), -- off, bottom
				  CompactedBlock( 0, -14, -5, -5, -6, -6, BTEX("planks_oak") ), -- off, east
		          CompactedBlock( -14, 0, -5, -5, -6, -6, BTEX("planks_oak") ), -- off, west
				  CompactedBlock( -5, -5, 0, -14, -6, -6, BTEX("planks_oak") ), -- off, south
				  CompactedBlock( -5, -5, -14, 0, -6, -6, BTEX("planks_oak") ), -- off, north
				  CompactedBlock( -5, -5, -6, -6, 0, -14, BTEX("planks_oak") ), -- off, top
				  0, 0,
		          CompactedBlock( -5, -5, -6, -6, -15, 0, BTEX("planks_oak") ), -- on, bottom
				  CompactedBlock( 0, -15, -5, -5, -6, -6, BTEX("planks_oak") ), -- on, east
		          CompactedBlock( -15, 0, -5, -5, -6, -6, BTEX("planks_oak") ), -- on, west
				  CompactedBlock( -5, -5, 0, -15, -6, -6, BTEX("planks_oak") ), -- on, south
		          CompactedBlock( -5, -5, -15, 0, -6, -6, BTEX("planks_oak") ), -- on, north
		          CompactedBlock( -5, -5, -6, -6, 0, -15, BTEX("planks_oak") ), -- on, top
				  0, 0 );
		-- [144] -- Head Block
		-- [145] -- Anvil
		-- [146] -- Trapped Chest
		[147] = CompactedBlock( -1, -1, -1, -1, 0, -15, BTEX("gold_block") ); -- Weighted Pressure Plate (Light)
		[148] = CompactedBlock( -1, -1, -1, -1, 0, -15, BTEX("iron_block") ); -- Weighted Pressure Plate (Heavy)
		-- [149] -- Redstone Comparator (inactive)
		-- [150] -- ??? -> Redstone Comparator (active)
		[151] = CompactedBlock( 0, 0, 0, 0, 0, -10, BTEX("daylight_detector_side"), BTEX("daylight_detector_side"), BTEX("daylight_detector_top") ); -- Daylight Sensor
		[152] = OpaqueBlock( BTEX("redstone_block") ); -- Block of Redstone
		[153] = OpaqueBlock( BTEX("quartz_ore") ); -- Nether Quartz (Ore)
		-- [154] -- Hopper
		[155] = DataAdapter( 0x4, -- Block of Quartz
					DataAdapter( 0x3,
						OpaqueBlock( BTEX("quartz_block_side"), BTEX("quartz_block_bottom"), BTEX("quartz_block_top") ), -- regular
						OpaqueBlock( BTEX("quartz_block_chiseled"), BTEX("quartz_block_chiseled_top") ), -- chiseld
						OpaqueBlock( BTEX("quartz_block_lines"), BTEX("quartz_block_lines_top") ), -- lines, orientation top-bottom
						OpaqueBlock( BTEX("quartz_block_lines"), BTEX("quartz_block_lines_top") ) ), -- lines, orientation west-east		
					OpaqueBlock( BTEX("quartz_block_lines"), BTEX("quartz_block_lines_top") ) ); -- lines, orientation north-south
		[156] = DataAdapter( 0x4, -- Quartz Stairs
		          Stairs( BTEX("quartz_block_side"), BTEX("quartz_block_bottom"), BTEX("quartz_block_top") ), -- regular
				  Stairs( BTEX("quartz_block_side"), BTEX("quartz_block_top"), BTEX("quartz_block_bottom") ) ); -- upside-down
		[157] = DataAdapter( 0x8, -- Activator Rail
		          Rail( BTEX("rail_activator") ), -- not powered
		          Rail( BTEX("rail_activator_powered") ) ); -- powered
		[158] = DataAdapter( 0x6, -- Dropper
					DataAdapter( 0x1,
						OpaqueBlock( BTEX("furnace_top"), BTEX("dropper_front_vertical"), BTEX("furnace_top") ), -- facing down
						OpaqueBlock( BTEX("furnace_top"), BTEX("furnace_top"), BTEX("dropper_front_vertical") ) ), -- facing up
					DataAdapter( 0x1,
						OpaqueBlock( BTEX("furnace_side"), BTEX("furnace_side"), BTEX("dropper_front_horizontal"), BTEX("furnace_side"), BTEX("furnace_top"), BTEX("furnace_top") ), -- facing north
						OpaqueBlock( BTEX("furnace_side"), BTEX("furnace_side"), BTEX("furnace_side"), BTEX("dropper_front_horizontal"), BTEX("furnace_top"), BTEX("furnace_top") ) ), -- facing south
					DataAdapter( 0x1,
						OpaqueBlock( BTEX("dropper_front_horizontal"), BTEX("furnace_side"), BTEX("furnace_side"), BTEX("furnace_side"), BTEX("furnace_top"), BTEX("furnace_top") ), -- facing west
						OpaqueBlock( BTEX("furnace_side"), BTEX("dropper_front_horizontal"), BTEX("furnace_side"), BTEX("furnace_side"), BTEX("furnace_top"), BTEX("furnace_top") ) ), -- facing east
					0, 0 );
		[159] = DataAdapter( 0xf, -- Stained Hardened Clay
		          OpaqueBlock( BTEX("hardened_clay_stained_white") ), -- white
		          OpaqueBlock( BTEX("hardened_clay_stained_orange") ), -- orange
		          OpaqueBlock( BTEX("hardened_clay_stained_magenta") ), -- magenta
		          OpaqueBlock( BTEX("hardened_clay_stained_light_blue") ), -- light blue
		          OpaqueBlock( BTEX("hardened_clay_stained_yellow") ), -- yellow
		          OpaqueBlock( BTEX("hardened_clay_stained_lime") ), -- lime
		          OpaqueBlock( BTEX("hardened_clay_stained_pink") ), -- pink
		          OpaqueBlock( BTEX("hardened_clay_stained_gray") ), -- grey
		          OpaqueBlock( BTEX("hardened_clay_stained_silver") ), -- light grey	
		          OpaqueBlock( BTEX("hardened_clay_stained_cyan") ), -- cyan
		          OpaqueBlock( BTEX("hardened_clay_stained_purple") ), -- purple
		          OpaqueBlock( BTEX("hardened_clay_stained_blue") ), -- blue
		          OpaqueBlock( BTEX("hardened_clay_stained_brown") ), -- brown
		          OpaqueBlock( BTEX("hardened_clay_stained_green") ), -- green
		          OpaqueBlock( BTEX("hardened_clay_stained_red") ), -- red
		          OpaqueBlock( BTEX("hardened_clay_stained_black") ) ); -- black
		-- [160] -- Stained Glass Panes
		[161] = DataAdapter( 0x1,-- Leaves (Acacia/Dark Oak)
		          BiomeHollowOpaqueBlock( 1, BTEX("leaves_acacia") ), -- acacia
		          BiomeHollowOpaqueBlock( 1, BTEX("leaves_big_oak") ) ); -- dark oak
		[162] = DataAdapter( 0x1, -- Wood (Acacia/Dark Oak)
		          OpaqueBlock( BTEX("log_acacia"), BTEX("log_acacia_top") ), -- acacia
		          OpaqueBlock( BTEX("log_big_oak"), BTEX("log_big_oak_top") ) ); -- dark oak
		[163] = Stairs( BTEX("planks_acacia") ); -- Acacia Wood Stairs
		[164] = Stairs( BTEX("planks_big_oak") ); -- Dark Oak Wood Stairs
		[165] = DelayRender( TransparentBlock( 0, BTEX("slime") ), 15 ); -- Slime Block
		-- [166] -- Barrier (not visible like air)
		[167] = DataAdapter( 0x8, -- Iron Trapdoor
		          DataAdapter( 0x4,
		            Slab( -13, 0, BTEX("iron_trapdoor") ), -- bottom
		            DataAdapter( 0x3,
		              CompactedBlock( 0, 0, -13, 0, 0, 0, BTEX("iron_trapdoor") ), -- north
		              CompactedBlock( 0, 0, 0, -13, 0, 0, BTEX("iron_trapdoor") ), -- south
		              CompactedBlock( -13, 0, 0, 0, 0, 0, BTEX("iron_trapdoor") ), -- west
		              CompactedBlock( 0, -13, 0, 0, 0, 0, BTEX("iron_trapdoor") ) ) ), -- east
		          DataAdapter( 0x4,
		            Slab( 0, -13, BTEX("iron_trapdoor") ), -- top
		            DataAdapter( 0x3,
		              CompactedBlock( 0, 0, -13, 0, 0, 0, BTEX("iron_trapdoor") ), -- north
		              CompactedBlock( 0, 0, 0, -13, 0, 0, BTEX("iron_trapdoor") ), -- south
		              CompactedBlock( -13, 0, 0, 0, 0, 0, BTEX("iron_trapdoor") ), -- west
		              CompactedBlock( 0, -13, 0, 0, 0, 0, BTEX("iron_trapdoor") ) ) ) ); -- east
		[168] = DataAdapter( 0x3, -- Prismarine
		              OpaqueBlock( BTEX_Water("prismarine_rough") ), -- regular
		              OpaqueBlock( BTEX("prismarine_bricks") ), -- bricks
		              OpaqueBlock( BTEX("prismarine_dark") ), -- dark
					  0 );
		[169] = OpaqueBlock( BTEX_Water("sea_lantern") ); -- Sea Lantern
		[170] = OpaqueBlock( BTEX("hay_block_side"), BTEX("hay_block_top") ); -- Hay Bale
		[171] = DataAdapter( 0xf, -- Carpet
		          CompactedBlock( 0, 0, 0, 0, 0, -15, BTEX("wool_colored_white") ), -- white
		          CompactedBlock( 0, 0, 0, 0, 0, -15, BTEX("wool_colored_orange") ), -- orange
		          CompactedBlock( 0, 0, 0, 0, 0, -15, BTEX("wool_colored_magenta") ), -- magenta
		          CompactedBlock( 0, 0, 0, 0, 0, -15, BTEX("wool_colored_light_blue") ), -- light blue
		          CompactedBlock( 0, 0, 0, 0, 0, -15, BTEX("wool_colored_yellow") ), -- yellow
		          CompactedBlock( 0, 0, 0, 0, 0, -15, BTEX("wool_colored_lime") ), -- lime
		          CompactedBlock( 0, 0, 0, 0, 0, -15, BTEX("wool_colored_pink") ), -- pink
		          CompactedBlock( 0, 0, 0, 0, 0, -15, BTEX("wool_colored_gray") ), -- grey
		          CompactedBlock( 0, 0, 0, 0, 0, -15, BTEX("wool_colored_silver") ), -- light grey	
		          CompactedBlock( 0, 0, 0, 0, 0, -15, BTEX("wool_colored_cyan") ), -- cyan
		          CompactedBlock( 0, 0, 0, 0, 0, -15, BTEX("wool_colored_purple") ), -- purple
		          CompactedBlock( 0, 0, 0, 0, 0, -15, BTEX("wool_colored_blue") ), -- blue
		          CompactedBlock( 0, 0, 0, 0, 0, -15, BTEX("wool_colored_brown") ), -- brown
		          CompactedBlock( 0, 0, 0, 0, 0, -15, BTEX("wool_colored_green") ), -- green
		          CompactedBlock( 0, 0, 0, 0, 0, -15, BTEX("wool_colored_red") ), -- red
		          CompactedBlock( 0, 0, 0, 0, 0, -15, BTEX("wool_colored_black") ) ); -- black
		[172] = OpaqueBlock( BTEX("hardened_clay") ); -- Hardened Clay
		[173] = OpaqueBlock( BTEX("coal_block") ); -- Block of Coal
		[174] = OpaqueBlock( BTEX("ice_packed") ); -- Packed Ice
		-- [175] -- Double Plants
		-- [176] -- Standing Banner
		-- [177] -- Wall Banner
		[178] = CompactedBlock( 0, 0, 0, 0, 0, -10, BTEX("daylight_detector_side"), BTEX("daylight_detector_side"), BTEX("daylight_detector_inverted_top") ); -- Inverted Daylight Sensor
		[179] = DataAdapter( 0x3, -- Red Sandstone
		          OpaqueBlock( BTEX("red_sandstone_normal"), BTEX("red_sandstone_bottom"), BTEX("red_sandstone_top") ), -- regular
		          OpaqueBlock( BTEX("red_sandstone_carved"), BTEX("red_sandstone_top"), BTEX("red_sandstone_top") ), -- chiseled
		          OpaqueBlock( BTEX("red_sandstone_smooth"), BTEX("red_sandstone_top"), BTEX("red_sandstone_top") ), -- smooth
		          0 );
		[180] = DataAdapter( 0x4, -- Red Sandstone Stairs
		          Stairs( BTEX("red_sandstone_normal"), BTEX("red_sandstone_bottom"), BTEX("red_sandstone_top") ), -- regular
				  Stairs( BTEX("red_sandstone_normal"), BTEX("red_sandstone_top"), BTEX("red_sandstone_bottom") ) ); -- upside-down
		[181] = DataAdapter( 0x8, -- Red Sandstone Double-Slab
		          OpaqueBlock( BTEX("red_sandstone_normal"), BTEX("red_sandstone_bottom"), BTEX("red_sandstone_top") ), -- regular
				  OpaqueBlock( BTEX("red_sandstone_top") ) ); -- smooth
		[182] = DataAdapter( 0x8, -- Red Sandstone Slab
		          Slab( -8, 0, BTEX("red_sandstone_normal"), BTEX("red_sandstone_bottom"), BTEX("red_sandstone_top") ), -- regular slab
				  Slab( 0, -8, BTEX("red_sandstone_normal"), BTEX("red_sandstone_bottom"), BTEX("red_sandstone_top") ) ); -- upside-down slab
		[183] = FenceGate( BTEX("planks_spruce") ); -- Spruce Fence Gate
		[184] = FenceGate( BTEX("planks_birch") ); -- Birch Fence Gate
		[185] = FenceGate( BTEX("planks_jungle") ); -- Jungle Fence Gate
		[186] = FenceGate( BTEX("planks_big_oak") ); -- Dark Oak Fence Gate
		[187] = FenceGate( BTEX("planks_acacia") ); -- Acacia Fence Gate
		[188] = Fence( BTEX("planks_spruce") ); -- Spruce Wood Fence
		[189] = Fence( BTEX("planks_birch") ); -- Birch Wood Fence
		[190] = Fence( BTEX("planks_jungle") ); -- Jungle Wood Fence
		[191] = Fence( BTEX("planks_big_oak") ); -- Dark Oak Wood Fence
		[192] = Fence( BTEX("planks_acacia") ); -- Acacia Wood Fence
		[193] = Door( BTEX("door_spruce_lower"), BTEX("door_spruce_upper") ); -- Spruce Wood Door
		[194] = Door( BTEX("door_birch_lower"), BTEX("door_birch_upper") ); -- Birch Wood Door
		[195] = Door( BTEX("door_jungle_lower"), BTEX("door_jungle_upper") ); -- Jungle Wood Door
		[196] = Door( BTEX("door_acacia_lower"), BTEX("door_acacia_upper") ); -- Acacia Wood Door
		[197] = Door( BTEX("door_dark_oak_lower"), BTEX("door_dark_oak_upper") ); -- Dark Oak Wood Door
	};
	for id, blk in pairs( MinecraftBlocks ) do
		blocks:setGeometry( id, blk[1] );
		blocks:setSolidity( id, blk[2] );
	end

	return blocks;
end


require "ui"
require "blockids"
require "biomes"
require "spline"
require "assets"

local sunLightColor = { 0.99, 0.99, 0.99 };
local endLightColor = { 61/255, 76/255, 68/255 };
local moonLightColor = { 59/255, 53/255, 78/255 };
local dawnDuskGlowColor = { 0.5*0x9b/255, 0.5*0x40/255, 0.5*0x16/255 };
local blockLightColor = { 1.7, 1.39, 1 };

-- Block highlights
local blockHighlights = {
	--{ "name", id1, id2, ... }
	{ "Coal Ore", 16 };
	{ "Iron Ore", 15 };
	{ "Gold Ore", 14 };
	{ "Diamond", 56 };
	{ "Redstone Ore", 73, 74 };
	{ "Lapis Lazuli Ore", 21 };
	{ "Emerald", 129, 133 };
	{ "Clay", 82 };
	{ "Moss Stone", 48 };
	{ "Rails", 66, 27, 28, 157 };
	{ "Redstone Wire", 55, 75 };
	{ "Trees", 17, 18 };
	{ "Water", 8, 9 };
	{ "Chests", 54, 95 };
	{ "Spawner", 52 };
	{ "Portal", 90 };
};

-- Helper functions
local function takeScreenshotNow( worldName )
	local name = Config.screenshot_path;
	local fnroot;
	if name then
		fnroot = os.date( string.gsub( name, "%%(.)", {O=worldName} ) );
		local path = string.match( fnroot, "^(.*[/\\])[^/\\]*$" );
		if path then
			eihort.createDirectory( path );
		end
	else
		fnroot = os.date( eihort.ProgramPath .. "eihort-%Y.%m.%d-%H.%M.%S" );
	end

	local fn = fnroot .. ".png";
	local f = io.open( fn );
	local i = 0;
	while f do
		f:close();
		fn = fnroot .. "-" .. i .. ".png";
		i = i + 1;
		f = io.open( fn );
	end
	local screen = eihort.screengrab();
	local success, msg = screen:writePNG( fn );
	if not success then
		eihort.errorDialog( "Screenshot failed", msg );
	end
end

local function setGpuAllowance( view )
	local allowance = Config.max_gpu_mem or 0;
	if allowance == 0 then
		allowance = eihort.getVideoMem();
		if not allowance or allowance == 0 then
			eihort.errorDialog( "VRAM Autodetect", "Could not detect the amount of video memory available. Please set max_gpu_mem in eihort.config to an appropriate value. Defaulting to 512MB." );
			allowance = 512*1024*1024;
		end
		allowance = allowance * 0.9;
	else
		allowance = allowance * 1024 * 1024;
	end
		
	view:setGpuAllowance( allowance );
end

local function moveSpawnHere( worldPath, dim, x, y, z )
	if dim ~= 0 then
		eihort.errorDialog( "Move spawn", "The spawn must be located in the overworld." );
		return;
	end

	local levelDatPath = worldPath .. "level.dat";
	local rootName, levelDat = eihort.loadNBT( levelDatPath );
	if not levelDat then
		eihort.errorDialog( "Move spawn", "Failed to open level.dat." );
		return;
	end

	local levelData = levelDat:get("Data");
	if not levelData then
		eihort.errorDialog( "Move spawn", "level.dat doesn't have TAG_Compound \"Data\"?" );
		return;
	end

	levelData:set( "SpawnX", x, "int" );
	levelData:set( "SpawnY", y, "int" );
	levelData:set( "SpawnZ", z, "int" );

	levelDat:write( levelDatPath, rootName );
	levelDat:destroy();
end

local function movePlayerHere( worldPath, dim, x, y, z )
	local levelDatPath = worldPath .. "level.dat";
	local rootName, levelDat = eihort.loadNBT( levelDatPath );
	if not levelDat then
		eihort.errorDialog( "Move player", "Failed to open level.dat." );
		return;
	end

	local levelData = levelDat:get("Data");
	if not levelData then
		eihort.errorDialog( "Move player", "level.dat doesn't have TAG_Compound \"Data\"?" );
		return;
	end
	local player = levelData:get("Player");
	if not player then
		player = levelData:newCompound( "Player" );
	end

	local pos = player:newList( "Pos", "double" );
	pos[1] = x;
	pos[2] = y;
	pos[3] = z;

	player:set( "Dimension", dim, "int" );

	levelDat:write( levelDatPath, rootName );
	levelDat:destroy();
end

local function getPlayerPosition( worldPath )
	local levelDatPath = worldPath .. "level.dat";
	local rootName, levelDat = eihort.loadNBT( levelDatPath );
	if levelDat then
		local levelData = levelDat:get("Data");
		if levelData then
			local player = levelData:get("Player");
			if player then
				local pos = player:get("Pos") or {};
				local rot = player:get("Rotation") or {};
				local dim = player:get("Dimension");
				local x, y, z, az, pitch = pos[1] or 0, pos[2] or 128, pos[3] or 0, rot[1] or 0, rot[2] or 0;
				levelDat:destroy();
				return x, y+1.62, z, math.rad(math.fmod(-az+90,360)-180), -math.rad(pitch), dim or 0;
			end
		end
	end
	levelDat:destroy();
	return 0, 128, 0, 0, 0, 0;
end

local function updateLightModels_netherend( worldView, sky, worldTime, dark )
	local sr, sg, sb = unpack( endLightColor );
	sky:setTime( 0 );

	local function setLightModel( dir, pct )
		local model = worldView:getLightModel(dir);
		model:setSky( pct*sr, pct*sg, pct*sb, 0 );
		model:setDark( dark, dark, dark );
		model:setBlock( unpack( blockLightColor ) );
	end

	-- Z- (west), Z+ (east)
	setLightModel( 2, 0.8 );
	setLightModel( 3, 0.8 );
	-- X- (north), X+ (south)
	setLightModel( 0, 0.8^2 );
	setLightModel( 1, 0.8^2 );
	-- Y- (down)
	setLightModel( 4, 0.8^3 );
	-- Y+ (up)
	setLightModel( 5, 1 );
end

local function updateLightModels_overworld( worldView, sky, worldTime, dark )
	local mr, mg, mb = unpack( moonLightColor );
	local sr, sg, sb = unpack( sunLightColor );
	local gr, gg, gb = unpack( dawnDuskGlowColor );
	sky:setTime( worldTime );

	local function getStr( angle )
		angle = math.fmod( angle + math.pi, math.pi*2 ) - math.pi;
		local str = math.min( 1, math.max( 0, math.cos( angle ) * 0.8 + 0.2 ) ) ^ 0.8;
		local mask = str ^ 0.8;
		local wStr = math.min( 1, math.max( 0, math.cos( (angle - math.pi/2)*0.9 ) * 0.4 + 0.6 ) ) * mask;
		local eStr = math.min( 1, math.max( 0, math.cos( (angle + math.pi/2)*0.9 ) * 0.4 + 0.6 ) ) * mask;
		return str, wStr, eStr;
	end

	local sunAngle = math.pi * worldTime;
	local sunStr, wSunStr, eSunStr = getStr( sunAngle );
	local moonStr, wMoonStr, eMoonStr = getStr( sunAngle + math.pi );
	local wGlow = math.max( 0, math.sin( sunAngle ) ^ 3 - 0.05 );
	local eGlow = math.max( 0, -math.sin( sunAngle ) ^ 3 - 0.05 );
	local oGlow = (wGlow+eGlow)*0.8;

	local function setLightModel( dir, sun, moon, glow )
		local model = worldView:getLightModel(dir);
		model:setSky( sun*sr + moon*mr + glow*gr, sun*sg + moon*mg + glow*gg, sun*sb + moon*mb + glow*gb, sunStr*1.2 );
		model:setDark( dark, dark, dark );
		model:setBlock( unpack( blockLightColor ) );
	end
	-- Z- (west), Z+ (east)
	setLightModel( 2, wSunStr, wMoonStr, wGlow );
	setLightModel( 3, eSunStr, eMoonStr, eGlow );
	-- X- (north), X+ (south)
	setLightModel( 0, sunStr*0.8^2, moonStr*0.8^2, 0 );
	setLightModel( 1, sunStr*0.8^2, moonStr*0.8^2, 0 );
	-- Y- (down)
	setLightModel( 4, sunStr*0.8^3, moonStr*0.8^3, 0 );
	-- Y+ (up)
	setLightModel( 5, sunStr, moonStr, 0 );
end

local function createOverworldSky()
	local sky = eihort.newSky();
	local sun = loadTexture( "assets/minecraft/textures/environment/sun.png", 1, 1 ):uploadToGL( 'mag_nearest', 'min_nearest', 'mip_none' );
	local moonImage = loadTexture( "assets/minecraft/textures/environment/moon_phases.png", 1, 1 );
	local moon = moonImage:sub( 0, 0, moonImage.width/4, moonImage.height/2 ):uploadToGL( 'mag_nearest', 'min_nearest', 'mip_none' );
	sky:setSunMoon( sun, moon );

	local function setMoonPhase( phase )
		phase = math.floor( math.fmod( phase, 8 ) );
		phaseImg = moonImage:sub( (phase - math.floor(phase/4)*4) * moonImage.width/4, math.floor(phase/4)*moonImage.height/2, moonImage.width/4, moonImage.height/2 );
		phaseImg:uploadToGL( moon, 'mag_nearest', 'min_nearest', 'mip_none' );
		phaseImg:destroy();
	end
	return sky, setMoonPhase;
end

local function createNetherEndSky()
	local sky = eihort.newSky();
	sky:setColors( 0, 0, 0, 0, 0, 0 );
	return sky;
end

-- Main Map view stuff
function beginMapView( world, worldName )
	local worldPath = world:getRootPath();
	local blocks = loadBlockDesc();
	loadBiomeTextures( blocks, world:getRootPath() );
	local worldView = world:createView( blocks, Config.qtree_leaf_size or 7 );
	local owSky, setMoonPhase = createOverworldSky();
	local neSky = createNetherEndSky();
	setGpuAllowance( worldView );

	local mouseX, mouseY = eihort.getMousePos();
	local ignoreNextMM = false;
	local mouseLook = false;
	local controlsOn = 0;
	eihort.showCursor( true );

	local worldTime, viewDistance = Config.start_time or 0, Config.start_view_distance or 500;
	local pitch, azimuth;
	local light0, lightStr = 0.05, "Overworld";
	local showUI, uiRoot = true, ui.newRoot();
	local infoDisplay = ui.newLabel( "", 0.03, 0.02, 0, 0, 1, 1 );
	local takeScreenshot = false;
	local speedModifier = 1;
	local eyeX, eyeY, eyeZ, fwdX, fwdY, fwdZ, upX, upY, upZ, rightX, rightY, rightZ;
	local inDim = -666;
	local fov = (Config.fov or 75) * math.pi / 180;
	local nearPlane = 0.1;
	local monitorEnabled = false;

	local drawSky, updateLightModels, sky;
	local moonPhase = 0;
	local redrawNextFrame = true;

	local splinet, splinedt = 0, 0;
	local lastSplineAz = 0;
	local splines = {
		x = NewSpline();
		y = NewSpline();
		z = NewSpline();
		azimuth = NewSpline();
		pitch = NewSpline();
	};
	
	local execNoLoad = { };
	local function execWhenNothingIsLoading( what, a, b, c, d )
		table.insert( execNoLoad, function() what( a, b, c, d ); end );
		worldView:pauseLoading( true );
	end

	local function refreshPosition()
		fwdX, fwdY, fwdZ, upX, upY, upZ, rightX, rightY, rightZ = eihort.fwdUpRightFromAzPitch( azimuth, pitch );
		worldView:setPosition( eyeX, eyeY, eyeZ, fwdX, fwdY, fwdZ, upX, upY, upZ );
		redrawNextFrame = true;
	end
	local function resolveSplinePos()
		if splines.x.n == 0 then
			return;
		end
		eyeX, eyeY, eyeZ = splines.x:evaluate( splinet ), splines.y:evaluate( splinet ), splines.z:evaluate( splinet );
		azimuth = math.fmod( splines.azimuth:evaluate( splinet ) + math.pi, math.pi*2 ) - math.pi;
		pitch = splines.pitch:evaluate( splinet );
	end
	local function refreshInfoDisplay()
		local triCount, vtxMem, idxMem, texMem = worldView:getLastFrameStats();
		infoDisplay.text = string.format( "Coords: (%.0f %.0f %.0f)\n%s%s%sSee eihort.config for key bindings.\n\nLight Strength: %s\nTime: %.0f:%02.0f\nView Distance: %.0f%s",
			eyeX, eyeY, eyeZ,
			(Config.show_region_file and ("Region File: r."..math.floor(eyeX/(32*16)).."."..math.floor(eyeZ/(32*16)).."\n")) or "",
			(Config.show_triangles and ("Triangles: " .. triCount .. "\n")) or "",
			(Config.show_vram_use and string.format( "Mem: %.0f MB (%.0f free)\n", 
			    (vtxMem + idxMem + texMem) / (1024*1024),
			    worldView:getGpuAllowanceLeft() / (1024*1024) )) or "",
			lightStr,
			math.floor( worldTime * 12 + 12 ), math.floor( math.fmod( worldTime * 12 + 12, 1 ) * 60 ),
			viewDistance,
			(monitorEnabled and "\nMonitoring World Folder") or "" );
	end
	local function resetCameraParams()
		worldView:setCameraParams( fov, ScreenWidth / ScreenHeight, nearPlane, viewDistance * 1.05 );
	end
	local function moveToDim( dim )
		-- Nether/Overworld/End switching
		if inDim ~= dim then
			if dim == -1 then
				-- To nether
				eyeX = eyeX / 8;
				eyeZ = eyeZ / 8;
			elseif inDim == -1 then
				-- From nether
				eyeX = eyeX * 8;
				eyeZ = eyeZ * 8;
			end
			refreshPosition();
			if dim == 0 then
				world:changeRootPath( worldPath );
				light0 = 0.8^15;
				lightStr = "Overworld";
				updateLightModels = updateLightModels_overworld;
				sky = owSky;
				blocks:setDefAirSkylight( 15 );
			else
				world:changeRootPath( worldPath .. "DIM" .. dim .. "/" );
				light0 = 0.9^15;
				lightStr = "Nether/End";
				updateLightModels = updateLightModels_netherend;
				sky = neSky;
				if dim == 1 then
					blocks:setDefAirSkylight( 15, true );
				else
					blocks:setDefAirSkylight( 0 );
				end
			end
			updateLightModels( worldView, sky, worldTime, light0 );

			worldView:reloadAll();
			inDim = dim;
		end
	end
	local function resetPositionToPlayer()
		local x, y, z, yaw, pit, dim = getPlayerPosition( worldPath );
		azimuth, pitch = yaw, pit;
		eyeX, eyeY, eyeZ = x, y, z;
		moveToDim( dim );
		eyeX, eyeY, eyeZ = x, y, z;
		refreshPosition();
	end
	local function resetPosition()
		eyeX, eyeY, eyeZ = Config.origin_x or 0, Config.origin_y or 0, Config.origin_z or 0;
		azimuth = Config.origin_azimuth or 0;
		pitch = Config.origin_pitch or 0;
		refreshPosition();
	end

	resetPositionToPlayer();
	refreshInfoDisplay();
	resetCameraParams();
	worldView:setViewDistance( viewDistance );
	updateLightModels( worldView, sky, worldTime, light0 );
	EventDownAction = { };
	EventUpAction = { };

	-------------------------------------------------------------------
	-- Available impulse actions

	local impulseActions = {
		reloadworld = function()
			worldView:reloadAll();
		end;
		togglemonitor = function()
			monitorEnabled = not monitorEnabled;
			world:setMonitorState( monitorEnabled );
			redrawNextFrame = true;
		end;
		changelightmode = function()
			if light0 < 0.1 then
				light0 = 0.9^15;
				lightStr = "Nether/End";
			elseif light0 < 0.95 then
				light0 = 1.0;
				lightStr = "Full Bright";
			else
				light0 = 0.8^15;
				lightStr = "Overworld";
			end
			updateLightModels( worldView, sky, worldTime, light0 );
			redrawNextFrame = true;
		end;
		changemoonphase = function()
			moonPhase = moonPhase + 1;
			setMoonPhase( moonPhase );
			redrawNextFrame = true;
		end;
		resetposition = resetPosition;
		playerposition = resetPositionToPlayer;
		toggleui = function()
			showUI = not showUI;
			if showUI then
				uiRoot:mouseEnter( mouseX / ScreenWidth, mouseY / ScreenHeight );
			else
				uiRoot:mouseLeave();
			end
		end;
		screenshot = function()
			takeScreenshot = true;
			redrawNextFrame = true;
		end;
		freemouse = function()
			if mouseLook then
				mouseLook = false;
				eihort.showCursor( true );
				if showUI then
					uiRoot:mouseEnter( mouseX / ScreenWidth, mouseY / ScreenHeight );
				end
			end
		end;
		speedup = { function() speedModifier = Config.speedup_modifier or 8; end;
		            function() speedModifier = 1; end };
		slowdown = { function() speedModifier = Config.slowdown_modifier or 1/8; end;
		             function() speedModifier = 1; end };
	    splineadd = function()
			splines.x:addPt( eyeX );
			splines.y:addPt( eyeY );
			splines.z:addPt( eyeZ );
			local tau = 2 * math.pi;
			local taubase = tau * math.floor( lastSplineAz / tau );
			local az = taubase + azimuth;
			if math.abs( az - lastSplineAz ) > math.pi then
				if az < lastSplineAz then
					az = az + tau;
				else
					az = az - tau;
				end
			end
			lastSplineAz = az;
			splines.azimuth:addPt( az );
			splines.pitch:addPt( pitch );
		end;
		splinestart = function()
			splinet = 0;
			resolveSplinePos();
			refreshPosition();
		end;
		splineend = function()
			splinet = splines.x.n-1;
			resolveSplinePos();
			refreshPosition();
		end;
		splineerase = function()
			splinet = 0;
			for k, _ in pairs( splines ) do
				splines[k] = NewSpline();
			end
		end;
		splinepop = function()
			for _, v in pairs( splines ) do
				v:pop();
			end
			splinet = splines.x.n-1;
		end;
	};

	-------------------------------------------------------------------
	-- Key binding setup
	local controls = {
		moveforward = 0;
		moveback = 0;
		moveright = 0;
		moveleft = 0;
		moveup = 0;
		movedown = 0;
		['viewdist-'] = 0;
		['viewdist+'] = 0;
		['time-'] = 0;
		['time+'] = 0;
		['splinet+'] = 0;
		['splinet-'] = 0;
	};
	for k, v in pairs( Config.keys ) do
		local imp = impulseActions[v];
		k = string.lower( k );
		if imp then
			if type(imp) == "table" then
				EventDownAction[k] = imp[1];
				EventUpAction[k] = imp[2];
			else
				EventDownAction[k] = imp;
				EventUpAction[k] = nil;
			end
		elseif controls[v] then
			EventDownAction[k] = function()
				controlsOn = controlsOn + 1;
				controls[v] = controls[v] + 1;
			end;
			EventUpAction[k] = function()
				controlsOn = controlsOn - 1;
				controls[v] = controls[v] - 1;
			end;
		else
			error( "Unknown key binding: " .. v );
		end
	end

	-------------------------------------------------------------------
	-- Mouse stuff
	Event.mousemove = function( x, y )
		if ignoreNextMM then
			ignoreNextMM = false;
			return;
		end

		if mouseLook then
			local dx = x - mouseX;
			local dy = y - mouseY;

			azimuth = azimuth - dx * (Config.mouse_sensitivity_x or 0.008);
			pitch = pitch - dy * (Config.mouse_sensitivity_y or 0.01);
			if azimuth > math.pi then
				azimuth = azimuth - 2 * math.pi;
			elseif azimuth < -math.pi then
				azimuth = azimuth + 2 * math.pi;
			end
			if pitch > math.pi / 2 then
				pitch = math.pi / 2;
			elseif pitch < -math.pi / 2 then
				pitch = -math.pi / 2;
			end

			refreshPosition();
			ignoreNextMM = true;
			eihort.warpMouse( mouseX, mouseY );
			return;
		end

		mouseX = x;
		mouseY = y;

		-- Update the UI
		if showUI then
			uiRoot:mouseMove( x / ScreenWidth, y / ScreenHeight );
		end
	end;
	EventDownAction.mouse1 = function()
		if not mouseLook then
			if not showUI or uiRoot:mouseDown( mouseX / ScreenWidth, mouseY / ScreenHeight ) == uiRoot then
				-- Clicked in the window, not on a UI element
				mouseLook = true;
				ignoreNextMM = true;
				mouseX, mouseY = math.floor( ScreenWidth / 2 ), math.floor( ScreenHeight / 2 );
				eihort.warpMouse( mouseX, mouseY );
				eihort.showCursor( false );
				if showUI then
					uiRoot:mouseLeave();
				end
			end
		end
	end;
	EventUpAction.mouse1 = function()
		if not mouseLook and showUI then
			uiRoot:mouseUp( mouseX / ScreenWidth, mouseY / ScreenHeight );
		end
	end;

	-------------------------------------------------------------------
	-- Main UI
	do
		local panelW = 0.28;
		local panelR = 0.98;
		local panelX = panelR - panelW;
		local panelY = 0.1;
		local extraOptionsPanel = uiRoot:addComposite( 1, 0, 1, 1 );
		local fontHt = 0.03;
		local buttonHt = 0.035;
		local buttonSpace = buttonHt + 0.01;
		local y = panelY;

		-- Show Extra Options toggle
		uiRoot:addToggle(
			function() extraOptionsPanel.invisible = false; end, 
			function() extraOptionsPanel.invisible = true; end,
			"Extra Options", fontHt*1.2, 0, panelX-0.1, 0.02, panelW+0.1, 0.04 );
		extraOptionsPanel.invisible = true;

		-- Select other world button
		local moveSpawnButton = extraOptionsPanel:addButton(
			function() execWhenNothingIsLoading( function()
				worldView:destroy();
				world:destroy();
				blocks:destroy();
				beginWorldMenu();
			end ); end,
			"Select another world", fontHt, 0, panelX, y, panelW, buttonHt );
		y = y + buttonSpace;

		-- Move spawn button
		local moveSpawnButton = extraOptionsPanel:addButton(
			function() moveSpawnHere( worldPath, inDim, eyeX, eyeY, eyeZ ); end,
			"Move spawn here", fontHt, 0, panelX, y, panelW, buttonHt );
		y = y + buttonSpace;

		-- Move player button
		local movePlayerButton = extraOptionsPanel:addButton(
			function() movePlayerHere( worldPath, inDim, eyeX, eyeY, eyeZ ); end,
			"Move player here", fontHt, 0, panelX, y, panelW, buttonHt );
		y = y + buttonSpace;

		local dimButtonW = (panelW - 2*(buttonSpace - buttonHt)) / 3;
		extraOptionsPanel:addButton(
			function() execWhenNothingIsLoading( moveToDim, -1 ); end,
			"Nether", fontHt, 0, panelX, y, dimButtonW, buttonHt );
		extraOptionsPanel:addButton(
			function() execWhenNothingIsLoading( moveToDim, 0 ); end,
			"Overworld", fontHt*0.95, 0, panelX+dimButtonW+buttonSpace-buttonHt, y, dimButtonW, buttonHt );
		extraOptionsPanel:addButton(
			function() execWhenNothingIsLoading( moveToDim, 1 ); end,
			"The End", fontHt, 0, panelX+2*(dimButtonW+buttonSpace-buttonHt), y, dimButtonW, buttonHt );
		y = y + buttonSpace;

		-- No block lighting
		local function setNoBlockLighting( on )
			blocks:noBlockLighting( on );
			worldView:reloadAll();
		end
		local noBlockLighting = extraOptionsPanel:addToggle(
			function() execWhenNothingIsLoading( setNoBlockLighting, true ); end,
			function() execWhenNothingIsLoading( setNoBlockLighting, false ); end,
			"No block lighting", fontHt, 0, panelX, y, panelW, buttonHt );
		noBlockLighting.alt = "Disables light from torches and other light-emitting blocks.\nThis makes block highlights more visible";
		y = y + buttonSpace;

		local hiliteW = (panelW + buttonHt - buttonSpace) / 2;
		local hiliteX2 = panelX + hiliteW + buttonSpace - buttonHt;
		local function addBlockHighlight( i, name, ... )
			local ids = { ... };
			local function changeHighlight( on )
				for _, id in ipairs( ids ) do
					blocks:setHighlight( id, on );
				end
				worldView:reloadAll();
			end
			local x = panelX;
			if math.fmod( i, 2 ) == 1 then
				x = hiliteX2;
			end

			extraOptionsPanel:addToggle(
				function() execWhenNothingIsLoading( changeHighlight, true ); end,
				function() execWhenNothingIsLoading( changeHighlight, false ); end,
				name, fontHt, 0, x, y, hiliteW, buttonHt );

			if math.fmod( i, 2 ) == 0 then
				y = y + buttonSpace;
			end
		end
		for i, v in ipairs( blockHighlights ) do
			addBlockHighlight( i, unpack( v ) );
		end
		if math.fmod( #blockHighlights, 2 ) == 1 then
			y = y + buttonSpace;
		end

		extraOptionsPanel.rect:setRect( panelX-0.01, panelY-0.01, panelW+0.02, y+buttonHt-buttonSpace-panelY+0.02 );
	end

	-------------------------------------------------------------------
	-- Screen stuff
	EventDownAction.mfocus = nil;
	EventUpAction.mfocus = function()
		if mouseLook then
			mouseLook = false;
			eihort.showCursor( true );
			if showUI then
				uiRoot:mouseEnter( mouseX / ScreenWidth, mouseY / ScreenHeight );
			end
		end
	end;
	Event.redraw = function()
		eihort.beginRender();

		sky:render( worldView, 0, (eyeY - 16) / viewDistance, 0 );
		if eyeY > 16 then
			worldView:setFog( viewDistance * 0.7, viewDistance, sky:getHorizFogColor() );
		else
			worldView:setFog( viewDistance * 0.7, viewDistance, 0, 0, 0 );
		end
		worldView:render( true );

		if takeScreenshot then
			takeScreenshot = false;
			takeScreenshotNow( worldName );
		end

		if showUI then
			local ctx = eihort.newUIContext();
			uiRoot:render( ctx );
			refreshInfoDisplay();
			infoDisplay:render( ctx );
			ctx:destroy();
		end

		local good, err = eihort.endRender();
		assert( good or Config.ignore_gl_errors, err );
	end;
	EventDownAction.resize = function()
		resetCameraParams();
		uiRoot:changedAspect();
		infoDisplay:changedAspect();
	end;

	-------------------------------------------------------------------
	-- Movement controls
	local function moveEye( dx, dy, dz, scale )
		eyeX = eyeX + dx * scale;
		eyeY = eyeY + dy * scale;
		eyeZ = eyeZ + dz * scale;
	end;
	local function frameMove( dt )
		local ret = false;
		if execNoLoad[1] and not worldView:isLoading() then
			worldView:pauseLoading( false );
			local e = execNoLoad;
			execNoLoad = { };
			for _, v in ipairs( e ) do
				v();
			end
		end
		if controlsOn > 0 or math.abs( splinedt ) > 0.01 then
			local speed = dt * speedModifier;
			local mvSpeed = (Config.movement_speed or 50) * speed;
			if controls.moveforward > 0 then
				moveEye( fwdX, fwdY, fwdZ, mvSpeed );
			end
			if controls.moveback > 0 then
				moveEye( fwdX, fwdY, fwdZ, -mvSpeed );
			end
			if controls.moveright > 0 then
				moveEye( rightX, rightY, rightZ, mvSpeed );
			end
			if controls.moveleft > 0 then
				moveEye( rightX, rightY, rightZ, -mvSpeed );
			end
			if controls.moveup > 0 then
				moveEye( upX, upY, upZ, mvSpeed );
			end
			if controls.movedown > 0 then
				moveEye( upX, upY, upZ, -mvSpeed );
			end
			if controls['viewdist+'] > 0 or controls['viewdist-'] > 0 then
				local vdSpeed = speed * (Config.view_distance_speed or 50);
				if controls['viewdist+'] > 0 then
					viewDistance = viewDistance + vdSpeed;
					if viewDistance > 10000 then
						viewDistance = 10000;
					end
				end
				if controls['viewdist-'] > 0 then
					viewDistance = viewDistance - vdSpeed;
					if viewDistance < 50 then
						viewDistance = 50;
					end
				end
				resetCameraParams();
				worldView:setViewDistance( viewDistance );
			end
			if controls['time+'] > 0 or controls['time-'] > 0 then
				local tSpeed = speed * (Config.time_speed or 6) / 12;
				if controls['time+'] > 0 then
					worldTime = worldTime + tSpeed;
					if worldTime > 1 then
						worldTime = worldTime - 2;
					end
				end
				if controls['time-'] > 0 then
					worldTime = worldTime - tSpeed;
					if worldTime < -1 then
						worldTime = worldTime + 2;
					end
				end
				updateLightModels( worldView, sky, worldTime, light0 );
			end
			if controls['splinet+'] > 0 or controls['splinet-'] > 0 or math.abs( splinedt ) > 0.01 then
				local tSpeed = speedModifier * (Config.spline_speed or 0.5);
				local targdt = 0;
				if controls['splinet+'] > 0 then
					targdt = targdt + tSpeed;
				end
				if controls['splinet-'] > 0 then
					targdt = targdt - tSpeed;
				end
				splinedt = targdt + (splinedt - targdt) * (0.001^dt);
				splinet = splinet + splinedt * dt;
				if splinet < 0 then
					splinet = 0;
				elseif splinet > splines.x.n - 1 then
					splinet = splines.x.n - 1;
				end
				resolveSplinePos();
			end
			refreshPosition();
			ret = true;
		end
		if redrawNextFrame then
			redrawNextFrame = false;
			eihort.shouldRedraw( true );
		end
		return ret;
	end;

	-------------------------------------------------------------------
	-- Idle controller
	local minFrameTime = 1 / (Config.fps_limit or 60);
	local lastFrameTime = eihort.getTime();

	Event.idle = function()
		local t = eihort.getTime();
		local dt = t - lastFrameTime;
		if dt > minFrameTime then
			if dt > 0.1 then
				dt = 0.1;
				lastFrameTime = t;
			else
				lastFrameTime = lastFrameTime + dt;
			end
			return not frameMove( dt );
		end
		return true;
	end;
end

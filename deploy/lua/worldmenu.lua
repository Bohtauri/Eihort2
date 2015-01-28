
require "ui"
require "mapview"

local function scanForWorlds( rootPath, cb )
	local ff = eihort.findFile( rootPath .. "/*" );
	if ff then
		repeat
			local fn, isdir = ff:filename();
			if isdir then
				local levelDatPath = rootPath .. fn .. "/level.dat";
				local f = io.open( levelDatPath, "rb" );
				if f then
					f:close();
					local oName, dat = eihort.loadNBT( levelDatPath );
					if dat then
						local levelData = dat:get("Data");
						if levelData then
							local levelName = levelData:get("LevelName");
							if levelName then
								cb( levelName, rootPath .. fn .. "/", levelData );
							end
						end
						dat:destroy();
					end
				end
			end
		until not ff:next()
		ff:close();
	end
end

-- Main world menu stuff
function beginWorldMenu()
	local mouseX, mouseY = eihort.getMousePos();
	local uiRoot = ui.newRoot();
	eihort.showCursor( true );
	uiRoot:mouseEnter( mouseX / ScreenWidth, mouseY / ScreenWidth );

	EventDownAction = { };
	EventUpAction = { };

	local selectLabel = uiRoot:addLabel( "Select a world:", 0.045, 0, 0.3, 0.05, 0.4, 0.05 );
	selectLabel.align = eihort.TextAlignCenter + eihort.TextAlignVCenter;
	local assetPathsLabel = uiRoot:addLabel( "Texture search locations:" .. ActiveAssetPaths, 0.03, 0, 0.02, 0, 0.3, 0.97 );
	assetPathsLabel.align = eihort.TextAlignLeft + eihort.TextAlignBottom;
	local maxButtons = math.floor( (1 - 0.3) / 0.05 );
	local listRoot = uiRoot:addComposite( 0, 0.17, 1, 0.05*maxButtons );
	local listPages = { listRoot };
	local curPage = 1;
	local n = 0;
	local pageRect = { listPages[1].rect:getRect() };
	local lastSelectWorld, buttonTheme;

	local function addWorldToList( name, path, levelData )
		local version = levelData:get("version");
		local versionStr, isAnvil = "Unknown (Assuming MCRegion)", false;
		if version then
			if version == 0x4abd then
				versionStr = "Anvil";
				isAnvil = true;
			elseif version == 0x4abc then
				versionStr = "MCRegion";
			end
		end
		if n == maxButtons then
			listRoot = uiRoot:addComposite( unpack( pageRect ) );
			table.insert( listPages, listRoot );
			listRoot.invisible = true;
			n = 0;
		end
		local function selectWorld()
			beginMapView( eihort.loadWorld( path, isAnvil ), name );
		end
		lastSelectWorld = selectWorld;
		local button = listRoot:addButton( selectWorld, name, 0.035, 0, 0.3, pageRect[2]+0.05*n, 0.4, 0.04 );
		button:setTheme( buttonTheme );
		button.tooltip = path .. "\nMap Version: " .. versionStr;
		n = n + 1;
	end
	for _, path in ipairs( Config.world_paths or { "." } ) do
		local function colors( sc1, add1, sc2, add2 )
			return { true, path[2]*sc1+add1, path[3]*sc1+add1, path[4]*sc1+add1,   path[2]*sc2+add2, path[3]*sc2+add2, path[4]*sc2+add2 };
		end
		local OFF = { false };
		buttonTheme = {
			bkg =       colors( 1, 0, 0.8, 0 );
			bkg_mo =    colors( 0.7, 0.2, 0.5, 0.4 );
			bkg_md =    colors( -1, 1, -0.8, 1 );
			border =    OFF;
			border_mo = OFF;
			border_md = OFF;
		};
		scanForWorlds( path[1], addWorldToList );
	end

	if #listPages > 1 then
		uiRoot:addButton( function()
			listPages[curPage].invisible = true;
			curPage = curPage - 1;
			if curPage < 1 then
				curPage = #listPages;
			end
			listPages[curPage].invisible = false;
		end, "Previous Page", 0.03, 0, 0.35, pageRect[2]-0.05, 0.3, 0.035 );
		uiRoot:addButton( function()
			listPages[curPage].invisible = true;
			curPage = curPage + 1;
			if curPage > #listPages then
				curPage = 1;
			end
			listPages[curPage].invisible = false;
		end, "Next Page", 0.03, 0, 0.35, pageRect[2]+pageRect[4]+0.04-0.035, 0.3, 0.035 );
	elseif n <= 1 then
		if n == 1 then
			lastSelectWorld();
			return;
		else
			error( "No Minecraft worlds found.\n\nYou may:\n   - Drag and drop a world onto Eihort\n   - Specify the world's path after the command line\n   - Edit the world_paths option in eihort.config to point to a folder containing Minecraft worlds." );
		end
	end

	eihort.setClearColor( 0, 0, 0 );

	-------------------------------------------------------------------
	-- Mouse stuff
	Event.mousemove = function( x, y )
		mouseX = x;
		mouseY = y;

		-- Update the UI
		uiRoot:mouseMove( x / ScreenWidth, y / ScreenHeight );
	end;
	EventDownAction.mouse1 = function()
		uiRoot:mouseDown( mouseX / ScreenWidth, mouseY / ScreenHeight );
	end;
	EventUpAction.mouse1 = function()
		uiRoot:mouseUp( mouseX / ScreenWidth, mouseY / ScreenHeight );
	end;

	-------------------------------------------------------------------
	-- Screen stuff
	Event.redraw = function()
		eihort.beginRender();
		eihort.clearScreen();

		local uiCtx = eihort.newUIContext();
		uiRoot:render( uiCtx );
		uiCtx:destroy();

		local good, err = eihort.endRender();
		assert( good or Config.ignore_gl_errors, err );
	end;
	EventDownAction.resize = function()
		uiRoot:changedAspect();
	end;

	-------------------------------------------------------------------
	-- Idle controller
	Event.idle = function()
		return true;
	end;
end


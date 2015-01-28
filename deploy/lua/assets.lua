
local assetArchives = { };
local assetIndex = { };
ActiveAssetPaths = "";

local function indexDirectory( root, assetPath, ff )
	repeat
		local fn, isdir = ff:filename();
		local fullfn = root .. fn;
		local fullasset = assetPath .. fn;
		if isdir then
			if fn ~= "." and fn ~= ".." then
				local ff2 = eihort.findFile( fullfn .. "/*" );
				if ff2 then
					indexDirectory( fullfn .. "/", fullasset .. "/", ff2 );
					ff2:close();
				end
			end
		else
			assetIndex[fullasset] = fullfn;
		end
	until not ff:next();
end

function indexAssets( searchPaths )
	for i = #searchPaths, 1, -1 do
		path = searchPaths[i];

		if type(path) == "table" then
			-- This is another list of asset paths - probably from listFiles
			indexAssets( path );
		else
			-- Maybe this is an archive?
			local archiveIndex = eihort.indexZip( path );
			if archiveIndex then
				-- Yup.. add all files in the archive to the index
				local aid = #assetArchives + 1;
				assetArchives[aid] = path;
				for filename, directAccess in pairs( archiveIndex ) do
					assetIndex[filename] = aid .. "?" .. directAccess;
				end
				ActiveAssetPaths = "\n" .. path .. ActiveAssetPaths;
			else
				-- Nope.. maybe it's a folder?
				local ff = eihort.findFile( path .. "/*" );
				if ff then
					-- Yup.. add all files to the asset index
					indexDirectory( path .. "/", "", ff );
					ff:close();
					ActiveAssetPaths = "\n" .. path .. ActiveAssetPaths;
				else
					-- It's neither.. silently ignore it so we can put
					-- filenames with ??? in the default config
				end
			end
		end
	end

	--[[ Uncomment to see the loaded asset list
	local f = io.open( "assets.txt", "w" );
	for k, v in pairs( assetIndex ) do
		f:write( k .. " -> " .. v .. "\n" );
	end
	f:close();
	--]]
end

function loadTexture( path, failw, failh )
	local assetpath = assetIndex[path];
	local asset;
	if assetpath then
		local archive, start, comp, ucomp = string.match( assetpath, "^([^?]+)%?(%d+):(%d+)>?(%d*)$" );
		if archive then
			-- Asset is stored in a zip
			local zip = assetArchives[tonumber(archive)];
			asset = eihort.loadImageZipDirect( zip, tonumber(start), tonumber(comp), tonumber(ucomp) );
		else
			-- Load the file directly
			asset = eihort.loadImage( assetpath );
		end
	end
	if not asset and failw then
		if Config.silent_fail_texture_load then
			-- Return a white image for unknown but necessary textures
			return eihort.newImage( failw, failh, 1, 1, 1, 1 );
		else
			error( "Failed to locate " .. path .. ".\n\nMake sure this file is accessible from the asset search paths listed in eihort.config." );
		end
	end
	return asset;
end

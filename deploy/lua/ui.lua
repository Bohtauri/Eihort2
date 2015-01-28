
ui = { };

local function returnThis( this )
	return this;
end
local OFF = { false };

ui.TooltipTheme = {
	bkg =       { true, 0x0b, 0x1e, 0x2e,   0x05, 0x12, 0x1e };
	border =    { true, 0x1f, 0x68, 0xa6,   0x19, 0x31, 0x98 };
	textHt =    0.025;
	borderSpace=3;
};
ui.DefaultButtonTheme = {
	bkg =       { true, 0x1f, 0x68, 0xa6,   0x19, 0x31, 0x98 };
	bkg_mo =    { true, 0x3f, 0x88, 0xc6,   0x90, 0xc0, 0xa0 };
	bkg_md =    { true, 0xc6, 0x88, 0x3f,   0xb8, 0x51, 0x39 };
	border =    OFF;
	border_mo = OFF;
	border_md = OFF;
};
ui.DefaultToggleTheme = {
	set_bkg =         { true, 0x1f, 0x68, 0xa6,   0x19, 0x31, 0x98 };
	set_bkg_mo =      { true, 0x3f, 0x88, 0xc6,   0x90, 0xc0, 0xa0 };
	set_bkg_md =      { true, 0xc6, 0x88, 0x3f,   0xb8, 0x51, 0x39 };
	set_border =      OFF;
	set_border_mo =   OFF;
	set_border_md =   OFF;
	unset_bkg =       OFF;
	unset_bkg_mo =    OFF;
	unset_bkg_md =    OFF;
	unset_border =    { true, 0x1f, 0x68, 0xa6,   0x19, 0x31, 0x98 };
	unset_border_mo = { true, 0x3f, 0x88, 0xc6,   0x90, 0xc0, 0xa0 };
	unset_border_md = { true, 0xc6, 0x88, 0x3f,   0xb8, 0x51, 0x39 };
};

local function scaleTheme( theme )
	for _, t in pairs( theme ) do
		if type(t) == "table" then
			for i = 1, #t do
				if tonumber( t[i] ) then
					t[i] = t[i] / 255;
				end
			end
		end
	end
end
scaleTheme( ui.TooltipTheme );
scaleTheme( ui.DefaultButtonTheme );
scaleTheme( ui.DefaultToggleTheme );

------------------------------------------------------------------------------
-- Composites (collections of other UI elements)

local composite_meta = {
	-- Sub-element control
	add = function( this, what )
		table.insert( this, what );
	end;
	remove = function( this, what )
		for i, v in ipairs( this ) do
			if what == v then
				table.remove( this, i );
				return true;
			end
		end
		return false;
	end;
	findElementAt = function( this, x, y )
		for _, el in ipairs( this ) do
			if el.rect:contains( x, y ) and not el.invisible then
				return el;
			end
		end
	end;

	-- Mouse events
	mouseEnter = function( this, x, y )
		local el = this:findElementAt( x, y );
		if el and not el.invisible then
			this.mouseIn = el;
			return el:mouseEnter( x, y );
		end
		return this;
	end;
	mouseMove = function( this, x, y )
		if this.mouseIn then
			if this.mouseIn.rect:contains( x, y ) then
				return this.mouseIn:mouseMove( x, y );
			else
				this.mouseIn:mouseLeave();
				this.mouseIn = nil;
			end
		end
		return this:mouseEnter( x, y );
	end;
	mouseDown = function( this, x, y )
		if this.mouseIn then
			return this.mouseIn:mouseDown( x, y );
		end
		return this;
	end;
	mouseUp = function( this, x, y )
		if this.mouseIn then
			return this.mouseIn:mouseUp( x, y );
		end
		return this;
	end;
	mouseLeave = function( this )
		if this.mouseIn then
			this.mouseIn:mouseLeave();
			this.mouseIn = nil;
		end
	end;

	-- Rendering
	render = function( this, ctx )
		for _, el in ipairs( this ) do
			if not el.invisible then
				el:render( ctx );
			end
		end
	end;
	changedAspect = function( this )
		for _, el in ipairs( this ) do
			el:changedAspect();
		end
	end;

	-- Helpers to create new objects inside this composite
	addComposite = function( this, ... )
		local comp = ui.newComposite( ... );
		this:add( comp );
		return comp;
	end;
	addLabel = function( this, ... )
		local label = ui.newLabel( ... );
		this:add( label );
		return label;
	end;
	addButton = function( this, ... )
		local button = ui.newButton( ... );
		this:add( button );
		return button;
	end;
	addToggle = function( this, ... )
		local toggle = ui.newToggle( ... );
		this:add( toggle );
		return toggle;
	end;
};
composite_meta.__index = composite_meta;

function ui.newComposite( x, y, w, h )
	local c = {
		rect = eihort.newUIRect( x, y, w, h );
	};
	setmetatable( c, composite_meta );
	return c;
end

function ui.newRoot()
	local root = ui.newComposite( 0, 0, 1, 1 );
	local ttRect = eihort.newUIRect( 0, 0, 0.25, 1 );
	local tooltip;
	ttRect:setBkg( unpack( ui.TooltipTheme.bkg ) );
	ttRect:setBorder( unpack( ui.TooltipTheme.border ) );
	function root.mouseMove( this, x, y )
		local ret = composite_meta.mouseMove( this, x, y );
		local min = this;
		while min.mouseIn do
			min = min.mouseIn;
		end
		tooltip = min.tooltip;
		if tooltip then
			local uiCtx = eihort.newUIContext();
			local vScale = ui.TooltipTheme.textHt;
			local hScale = vScale / ScreenAspect;
			local border = ui.TooltipTheme.borderSpace * 2 / ScreenHeight;
			local w, h = uiCtx:textSize( tooltip, 0.25 / hScale );
			ttRect:setRect( x + 12 / ScreenWidth, y + 12 / ScreenHeight, w * hScale + border / ScreenAspect + 0.00001, h * vScale + border );
			uiCtx:destroy();
		end
	end
	function root.render( this, ctx )
		composite_meta.render( this, ctx );
		if tooltip then
			local textScale = ui.TooltipTheme.textHt;
			local border = ui.TooltipTheme.borderSpace / ScreenHeight;
			ttRect:draw( ctx );
			ttRect:drawTextIn( ctx, tooltip, textScale, textScale / ScreenAspect, 0, border, border / ScreenAspect );
		end
	end
	return root;
end

------------------------------------------------------------------------------
-- Labels

local label_meta = {
	mouseEnter = returnThis;
	mouseMove = returnThis;
	mouseDown = returnThis;
	mouseUp = returnThis;
	mouseLeave = returnThis;

	render = function( this, ctx )
		this.rect:draw( ctx );
		if this.text then
			this.rect:drawTextIn( ctx, this.text, this.fontHeight, this.fontWidth, this.align, this.vBorder, this.hBorder );
		end
	end;
	changedAspect = function( this )
		this.fontWidth = this.fontHeight / ScreenAspect;
		this.hBorder = this.vBorder / ScreenAspect;
	end;
};
label_meta.__index = label_meta;

function ui.newLabel( text, fontHeight, border, x, y, w, h )
	local c = {
		rect = eihort.newUIRect( x, y, w, h );
		text = text;
		align = 0;
	};
	c.fontHeight = fontHeight or 0.01;
	c.vBorder = border or 0;
	setmetatable( c, label_meta );
	c:changedAspect();
	return c;
end

------------------------------------------------------------------------------
-- Buttons
local button_meta = {
	mouseEnter = function( this )
		this.rect:setBkg( unpack( this.theme.bkg_mo ) );
		this.rect:setBorder( unpack( this.theme.border_mo ) );
		eihort.shouldRedraw( true );
		return this;
	end;
	mouseMove = returnThis;
	mouseDown = function( this )
		this.rect:setBkg( unpack( this.theme.bkg_md ) );
		this.rect:setBorder( unpack( this.theme.border_md ) );
		this.mousePressed = true;
		eihort.shouldRedraw( true );
		return this;
	end;
	mouseUp = function( this )
		if this.mousePressed then
			this.rect:setBkg( unpack( this.theme.bkg_mo ) );
			this.rect:setBorder( unpack( this.theme.border_mo ) );
			this.mousePressed = false;
			eihort.shouldRedraw( true );
			this.action();
		end
		return this;
	end;
	mouseLeave = function( this )
		if this.mousePressed then
			this.mousePressed = false;
		end
		this.rect:setBkg( unpack( this.theme.bkg ) );
		this.rect:setBorder( unpack( this.theme.border ) );
		eihort.shouldRedraw( true );
	end;
	setTheme = function( this, newTheme )
		this.theme = newTheme;
		if this.mousePressed then
			this:mouseDown();
		else
			this:mouseLeave();
		end
	end;

	render = label_meta.render;
	changedAspect = label_meta.changedAspect;
};
button_meta.__index = button_meta;

function ui.newButton( action, ... )
	local b = ui.newLabel( ... );
	b.action = action or function() end;
	b.theme = ui.DefaultButtonTheme;
	b.align = eihort.TextAlignCenter + eihort.TextAlignVCenter;
	b.rect:setBkg( unpack( b.theme.bkg ) );
	b.rect:setBorder( unpack( b.theme.border ) );
	setmetatable( b, button_meta );
	return b;
end

------------------------------------------------------------------------------
-- Toggles

local toggle_meta = {
	mouseEnter = function( this )
		if this.state then
			this.rect:setBkg( unpack( this.theme.set_bkg_mo ) );
			this.rect:setBorder( unpack( this.theme.set_border_mo ) );
		else
			this.rect:setBkg( unpack( this.theme.unset_bkg_mo ) );
			this.rect:setBorder( unpack( this.theme.unset_border_mo ) );
		end
		eihort.shouldRedraw( true );
		return this;
	end;
	mouseMove = returnThis;
	mouseDown = function( this )
		if this.state then
			this.rect:setBkg( unpack( this.theme.set_bkg_md ) );
			this.rect:setBorder( unpack( this.theme.set_border_md ) );
		else
			this.rect:setBkg( unpack( this.theme.unset_bkg_md ) );
			this.rect:setBorder( unpack( this.theme.unset_border_md ) );
		end
		this.mousePressed = true;
		eihort.shouldRedraw( true );
		return this;
	end;
	mouseUp = function( this )
		if this.mousePressed then
			this.state = not this.state;
			this.mousePressed = false;
			eihort.shouldRedraw( true );
			if this.state then
				this.rect:setBkg( unpack( this.theme.set_bkg_mo ) );
				this.rect:setBorder( unpack( this.theme.set_border_mo ) );
				this.setAction();
			else
				this.rect:setBkg( unpack( this.theme.unset_bkg_mo ) );
				this.rect:setBorder( unpack( this.theme.unset_border_mo ) );
				this.unsetAction();
			end
		end
		return this;
	end;
	mouseLeave = function( this )
		if this.mousePressed then
			this.mousePressed = false;
		end
		if this.state then
			this.rect:setBkg( unpack( this.theme.set_bkg ) );
			this.rect:setBorder( unpack( this.theme.set_border ) );
		else
			this.rect:setBkg( unpack( this.theme.unset_bkg ) );
			this.rect:setBorder( unpack( this.theme.unset_border ) );
		end
		eihort.shouldRedraw( true );
	end;

	render = label_meta.render;
	changedAspect = label_meta.changedAspect;

	setState = function( this, state )
		this.state = state;
		if this.state then
			this.rect:setBkg( unpack( this.theme.set_bkg ) );
			this.rect:setBorder( unpack( this.theme.set_border ) );
		else
			this.rect:setBkg( unpack( this.theme.unset_bkg ) );
			this.rect:setBorder( unpack( this.theme.unset_border ) );
		end
	end;
};
toggle_meta.__index = toggle_meta;

function ui.newToggle( setAction, unsetAction, ... )
	local t = ui.newLabel( ... );
	t.setAction = setAction or function() end;
	t.unsetAction = unsetAction or function() end;
	t.theme = ui.DefaultToggleTheme;
	t.align = eihort.TextAlignCenter + eihort.TextAlignVCenter;
	setmetatable( t, toggle_meta );
	t:setState( false );
	return t;
end


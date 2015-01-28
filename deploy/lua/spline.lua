
-- Very simple and inefficient cubic hermite spline class for Eihort

local spline_meta = {
	addPt = function( self, x )
		table.insert( self.x, x );
		if self.n > 1 then
			-- Catmull-Rom for now....
			self.dxdt[self.n] = 0.5 * (x - self.x[self.n-1]);
		end
		table.insert( self.dxdt, 0 );
		self.n = self.n + 1;
	end;
	evaluate = function( self, t )
		if t < 0 then
			t = 0;
		elseif t >= self.n-1 then
			if self.n <= 1 then
				assert( self.n > 0 );
				return self.x[1];
			end
			t = self.n - 1.0001;
		end
		local it, ft = math.modf( t );
		local x0, dx0 = self.x[it+1], self.dxdt[it+1];
		local x1, dx1 = self.x[it+2], self.dxdt[it+2];
		local pi = (3 - 2 * ft) * ft * ft;
		return x0*(1-pi) + x1*pi + dx0*ft*((ft-2)*ft+1) + dx1*ft*ft*(ft-1);
	end;
	pop = function( self )
		if self.n > 0 then
			table.remove( self.x );
			table.remove( self.dxdt );
			self.n = self.n - 1;
			if self.n > 0 then
				self.dxdt[self.n] = 0;
			end
		end
	end;
};
spline_meta.__index = spline_meta;

function NewSpline()
	local t = {
		n = 0;
		x = { };
		dxdt = { };
	};
	setmetatable( t, spline_meta );
	return t;
end




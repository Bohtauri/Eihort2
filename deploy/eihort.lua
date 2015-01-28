
package.path = eihort.ProgramPath .. "lua/?.lua;;";
return assert( loadfile( eihort.ProgramPath .. "lua/eihort.lua" ) )( ... );


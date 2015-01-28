/* Copyright (c) 2012, Jason Lloyd-Price
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met: 

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer. 
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution. 

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. */


#include "eihortshader.h"
#include "sky.h"
#include "endian.h"
#include "worldqtree.h"

#define SKY_META "Sky"

static const char *vertex_program =
"#version 110\n"
"varying vec3 eyeV;\n"
"void main(void) {\n"
	"eyeV = gl_Normal;\n"
	"gl_Position = gl_Vertex;\n"
"}\n"
;

static const char *fragment_program =
"#version 110\n"
"uniform sampler2D skyTex;\n"
"varying vec3 eyeV;\n"
"uniform vec3 offsetV;\n"

"void main(void) {"
	"vec3 V = normalize( eyeV ) + offsetV;\n"
	"vec2 lt = vec2( (1.0 + normalize( V.xy ).y) / 2.0, 1.0-normalize( V ).z );\n"
	"vec4 skyLight = texture2D( skyTex, lt );\n"

	"gl_FragColor = skyLight;\n"
"}\n"
;

void onError( const char *context, const char *error );

Sky::Sky()
: sunSize(0.5f)
, topColor(0xff1f4a76u)
, horizColor(0xff659bcau)
, fogColor(horizColor)
, sunTex(0), moonTex(0)
{
	char err[1024];
	if( !skyVtxShader.makeVertexShader( vertex_program, err, 1024 ) )
		onError( "vertex shader (sky) compilation", err );
	if( !skyFragShader.makeFragmentShader( fragment_program, err, 1024 ) )
		onError( "fragment shader (sky) compilation", err );
	skyShader.attach( &skyVtxShader );
	skyShader.attach( &skyFragShader );
	if( !skyShader.link( err, 1024 ) )
		onError( "sky shader linking", err );

	glGenTextures( 1, &skyTexId );
	glBindTexture( GL_TEXTURE_2D, skyTexId );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	glBindTexture( GL_TEXTURE_2D, 0 );

	skyShader.bind();
	glUniform1i( skyShader.uniform( "skyTex" ), 0 );
	skyShader.unbind();

	offsetUniform = skyShader.uniform( "offsetV" );
	setTime( 0.0f );
}

Sky::~Sky() {
	glDeleteTextures( 1, &skyTexId );
	if( sunTex )
		glDeleteTextures( 1, &sunTex );
	if( moonTex )
		glDeleteTextures( 1, &moonTex );
}

inline unsigned interp255( unsigned bits1, unsigned bits2, unsigned shift, unsigned t ) {
	bits1 = (bits1 >> shift) & 0xff;
	bits2 = (bits2 >> shift) & 0xff;
	return ((bits1 * (0xff-t) + bits2 * t) >> 8) << shift;
}

static unsigned interpColor( unsigned c1, unsigned c2, unsigned t ) {
	return interp255( c1, c2, 0, t ) | interp255( c1, c2, 8, t ) | interp255( c1, c2, 16, t ) | interp255( c1, c2, 24, t );
}

float Sky::setTime( float time ) {
	sunAngle = (time+0.5f) * jPI;
	float daylight = cosf( time * jPI ) * 0.8f + 0.4f;
	if( daylight > 1.0f )
		daylight = 1.0f;
	if( daylight < 0.0f )
		daylight = 0.0f;
	daylight = sqrtf( daylight );
	//eshader->setDaylight( std::max( 4.0f/15.0f, daylight ) );

	float dawnGlow;
	int glowX = 0;
	//if( fabs( time ) < 1.0f/1.4f ) {
		dawnGlow = sinf( time * jPI * 1.0f );
		glowX = dawnGlow > 0.0f ? 0 : 7;
		dawnGlow = std::max( 0.0f, fabs( dawnGlow * dawnGlow * dawnGlow ) * 1.1f - 0.1f );
	//} else {
	//	dawnGlow = 0.0f;
	//}
	
	
	// Daylight colors:
	//0xff1f4a76 // Dark color at the top of the sky
	//0xff659bca // Light color at the bottom of the sky
	//unsigned topColor = 0xff1f4a76u;
	//unsigned horizColor = 0xff659bcau;
	unsigned daylightT = (unsigned)floorf( daylight * 0xff );
	unsigned top = interpColor( 0xff000000u, topColor, daylightT );
	unsigned horiz = interpColor( 0xff000000u, horizColor, daylightT );
	fogColor = horiz;

	unsigned skyTex[32*8];
	// Bottom row is always black
	for( unsigned x = 0; x < 8; x++ )
		skyTex[x+31*8] = 0xff000000;
	for( unsigned z = 0; z < 31; z++ ) {
		for( unsigned x = 0; x < 8; x++ ) {
			float xGlow = std::max( 0, -abs((int)x-glowX) + 5 ) / 5.0f;
			float hGlow = z/31.0f;
			unsigned glow = (unsigned)(dawnGlow * hGlow * hGlow * sqrtf( xGlow ) * 0xff);
			unsigned baseColor = interpColor( top, horiz, std::min( 0xffu, z*11 ) );
			//unsigned glowColor = interpColor( 0xff000000u, 0xff9b4016, glow );
			//skyTex[x+z*8] = baseColor + glowColor;
			skyTex[x+z*8] = interpColor( baseColor, 0xff9b4016, glow );
		}
	}

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
  for (unsigned i=  0; i < sizeof(skyTex) / sizeof(*skyTex); ++i)
    skyTex[i] = uint32_t(bswap(uint32_t(skyTex[i])));
#endif

	glBindTexture( GL_TEXTURE_2D, skyTexId );
	glTexImage2D( GL_TEXTURE_2D, 0, 4, 8, 32, 0, GL_BGRA, GL_UNSIGNED_BYTE, &skyTex[0] );
	glBindTexture( GL_TEXTURE_2D, 0 );

	return std::max( 4.0f/15.0f, daylight );
}

void Sky::setColors( unsigned horiz, unsigned top ) {
	horizColor = horiz;
	topColor = top;
}

void Sky::setSunMoonTex( unsigned sun, unsigned moon ) {
	sunTex = sun;
	moonTex = moon;
}

void Sky::renderSky( const jPlane *frustum, const jVec3 *nOffset ) {
	jVec3 n;
	
	glMatrixMode( GL_PROJECTION );
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	glLoadIdentity();

	glDisable( GL_DEPTH_TEST );
	glDepthMask( 0 );

	skyShader.bind();
	// Cylindrical:
	//jVec3Set( &n, viewDistance, 0.0f, from->z - 64.0f );
	//jVec3Normalize( &n );
	//glUniform3f( offsetUniform, 0.0f, 0.0f, n.z );
	// Spherical:
	//glUniform3f( offsetUniform, 0.0f, 0.0f, (from->z - 16.0f) / viewDistance );

	glUniform3f( offsetUniform, nOffset->x, nOffset->y, nOffset->z );
	glBindTexture( GL_TEXTURE_2D, skyTexId );

	// Planes are in order: lower, upper, left, right
	glBegin( GL_QUADS );

	// Upper left corner
	jVec3Cross( &n, &frustum[2].n, &frustum[1].n );
	glNormal3fv( n.v );
	glVertex3f( -1.0f, 1.0f, 0.5f );

	// Lower left corner
	jVec3Cross( &n, &frustum[0].n, &frustum[2].n );
	glNormal3fv( n.v );
	glVertex3f( -1.0f, -1.0f, 0.5f );

	// Lower right corner
	jVec3Cross( &n, &frustum[3].n, &frustum[0].n );
	glNormal3fv( n.v );
	glVertex3f( 1.0f, -1.0f, 0.5f );

	// Upper right corner
	jVec3Cross( &n, &frustum[1].n, &frustum[3].n );
	glNormal3fv( n.v );
	glVertex3f( 1.0f, 1.0f, 0.5f );
	glEnd();

	glEnable( GL_DEPTH_TEST );
	glDepthMask( 1 );

	skyShader.unbind();

	glPopMatrix();
	glMatrixMode( GL_PROJECTION );
	glPopMatrix();
	glMatrixMode( GL_MODELVIEW );
}

void Sky::renderSunMoon( WorldQTree *qtree ) {
	if( !(sunTex && moonTex) )
		return;

	qtree->initCameraNoTrans();
	glRotatef( sunAngle * 180/jPI, 1.0f, 0.0f, 0.0f );

	glUseProgram( 0 );

	glDisable( GL_DEPTH_TEST );
	glEnable( GL_TEXTURE_2D );
	glEnable( GL_BLEND );
	glBlendFunc( GL_ONE, GL_ONE );
	glDepthMask( 0 );

	// Sun
	float sunOpacity = sinf( sunAngle ) + 1.1f;
	sunOpacity *= sunOpacity*sunOpacity;
	sunOpacity = std::min( 1.0f, sunOpacity * sunOpacity );
	if( sunOpacity > 0.01f ) {
		glBindTexture( GL_TEXTURE_2D, sunTex );
		glBegin( GL_QUADS );
		glColor3f( sunOpacity, sunOpacity, sunOpacity );
		glTexCoord2f( 0.0f, 0.0f );
		glVertex3f( -sunSize, 1.0f, -sunSize );
		glTexCoord2f( 1.0f, 0.0f );
		glVertex3f( sunSize, 1.0f, -sunSize );
		glTexCoord2f( 1.0f, 1.0f );
		glVertex3f( sunSize, 1.0f, sunSize );
		glTexCoord2f( 0.0f, 1.0f );
		glVertex3f( -sunSize, 1.0f, sunSize );
		glEnd();
	}

	// Moon
	float moonOpacity = -sinf( sunAngle ) + 1.1f;
	moonOpacity *= moonOpacity*moonOpacity;
	moonOpacity = std::min( 1.0f, moonOpacity * moonOpacity );
	if( moonOpacity > 0.01f ) {
		glBindTexture( GL_TEXTURE_2D, moonTex );
		glBegin( GL_QUADS );
		glColor3f( moonOpacity, moonOpacity, moonOpacity );
		glTexCoord2f( 0.0f, 0.0f );
		glVertex3f( -sunSize, -1.0f, -sunSize );
		glTexCoord2f( 0.0f, 1.0f );
		glVertex3f( -sunSize, -1.0f, sunSize );
		glTexCoord2f( 1.0f, 1.0f );
		glVertex3f( sunSize, -1.0f, sunSize );
		glTexCoord2f( 1.0f, 0.0f );
		glVertex3f( sunSize, -1.0f, -sunSize );
		glEnd();
	}

	glEnable( GL_DEPTH_TEST );
	glDisable( GL_BLEND );
	glDisable( GL_TEXTURE_2D );
	glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
	glDepthMask( 1 );
}

int Sky::lua_create( lua_State *L ) {
	Sky *regions = new Sky;
	regions->setupLuaObject( L, SKY_META );
	regions->lua_push();
	return 1;
}

int Sky::lua_setTime( lua_State *L ) {
	Sky *sky = getLuaObjectArg<Sky>( L, 1, SKY_META );
	lua_pushnumber( L, sky->setTime( (float)luaL_checknumber( L, 2 ) ) );
	return 1;
}

int Sky::lua_setColors( lua_State *L ) {
	Sky *sky = getLuaObjectArg<Sky>( L, 1, SKY_META );
	unsigned hr = (unsigned)luaL_checknumber( L, 2 ) & 0xff;
	unsigned hg = (unsigned)luaL_checknumber( L, 3 ) & 0xff;
	unsigned hb = (unsigned)luaL_checknumber( L, 4 ) & 0xff;
	unsigned tr = (unsigned)luaL_checknumber( L, 5 ) & 0xff;
	unsigned tg = (unsigned)luaL_checknumber( L, 6 ) & 0xff;
	unsigned tb = (unsigned)luaL_checknumber( L, 7 ) & 0xff;
	sky->setColors( 0xff000000u | (hr << 16) | (hg << 8) | hb, 0xff000000u | (tr << 16) | (tg << 8) | tb );
	return 0;
}

int Sky::lua_setSunMoon( lua_State *L ) {
	Sky *sky = getLuaObjectArg<Sky>( L, 1, SKY_META );
	unsigned sun = (unsigned)luaL_checknumber( L, 2 );
	unsigned moon = (unsigned)luaL_checknumber( L, 3 );
	sky->setSunMoonTex( sun, moon );
	return 0;
}

int Sky::lua_getOptimalFogColor( lua_State *L ) {
	Sky *sky = getLuaObjectArg<Sky>( L, 1, SKY_META );
	unsigned c = sky->getOptimalFogColor();
	lua_pushnumber( L, ((c >> 16) & 0xffu) / 255.0 );
	lua_pushnumber( L, ((c >> 8) & 0xffu) / 255.0 );
	lua_pushnumber( L, (c & 0xffu) / 255.0 );
	return 3;
}

int Sky::lua_render( lua_State *L ) {
	Sky *sky = getLuaObjectArg<Sky>( L, 1, SKY_META );
	WorldQTree *qtree = getLuaObjectArg<WorldQTree>( L, 2, WORLDQTREE_META );

	jVec3 nOffset;
	jVec3Set( &nOffset, (float)luaL_checknumber( L, 5 ), (float)luaL_checknumber( L, 3 ), (float)luaL_checknumber( L, 4 ) );

	sky->renderSky( qtree->getFrustum(), &nOffset );
	sky->renderSunMoon( qtree );
	return 0;
}

int Sky::lua_destroy( lua_State *L ) {
	delete getLuaObjectArg<Sky>( L, 1, SKY_META );
	return 0;
}

static const luaL_Reg Sky_functions[] = {
	{ "setTime", &Sky::lua_setTime },
	{ "setColors", &Sky::lua_setColors },
	{ "setSunMoon", &Sky::lua_setSunMoon },
	{ "getHorizFogColor", &Sky::lua_getOptimalFogColor },
	{ "render", &Sky::lua_render },
	{ "destroy", &Sky::lua_destroy },
	{ NULL, NULL }
};

void Sky::setupLua( lua_State *L ) {
	luaL_newmetatable( L, SKY_META );
	lua_pushvalue( L, -1 );
	lua_setfield( L, -2, "__index" );
	luaL_register( L, NULL, &Sky_functions[0] );
	lua_pop( L, 1 );

	lua_pushcfunction( L, &Sky::lua_create );
	lua_setfield( L, -2, "newSky" );
}

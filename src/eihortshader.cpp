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


#include <cmath>
#include <cstdio>

#include "eihortshader.h"
#include "glshader.h"
#include "platform.h"

static const char *vertex_program =
"#version 110\n"
"varying vec3 V;\n"
"void main(void) {\n"
	"V = vec3( gl_ModelViewMatrix * gl_Vertex );\n"
	"gl_Position = ftransform();\n"
	"gl_TexCoord[0] = gl_MultiTexCoord0;\n"
	"gl_TexCoord[1] = gl_TextureMatrix[1] * (gl_Vertex + 8.0 * vec4( gl_Normal, 0.0 ));\n"
"}\n"
;

static const char *vertex_program_texGen0 =
"#version 110\n"
"varying vec3 V;\n"
"void main(void) {\n"
	"V = vec3( gl_ModelViewMatrix * gl_Vertex );\n"
	"gl_Position = ftransform();\n"
	"gl_TexCoord[0] = gl_TextureMatrix[0] * gl_Vertex;\n"
	"gl_TexCoord[1] = gl_TextureMatrix[1] * (gl_Vertex + 8.0 * vec4( gl_Normal, 0.0 ));\n"
"}\n"
;

static const char *fragment_program =
"#version 110\n"
"uniform sampler2D mainTex;\n"
"uniform sampler3D lightTex;\n"
"uniform sampler2D lightShadeTex;\n"
"uniform vec2 lightOffset;\n"
"varying vec3 V;\n"

"void main(void) {"
	"vec4 diffuse = texture2D( mainTex, gl_TexCoord[0].xy );\n"
	"vec2 lighting = texture3D( lightTex, gl_TexCoord[1].xyz ).ga + lightOffset;\n"
	"vec4 light = texture2D( lightShadeTex, lighting );\n"

	//"float light = exp( lnlightbase * (1.0 - max( lighting.r, lighting.g * daylight )) );\n"
	"float fogInterp = clamp( (length(V) - gl_Fog.start) * gl_Fog.scale, 0.0, 1.0);\n"
	"gl_FragColor = vec4( mix( light.rgb * diffuse.rgb * gl_FrontMaterial.diffuse.rgb, gl_Fog.color.rgb, fogInterp*fogInterp), diffuse.a );\n"
"}\n"
;

static const char *fragment_program_foliage =
"#version 110\n"
"uniform sampler2D mainTex;\n"
"uniform sampler3D lightTex;\n"
"uniform sampler2D foliageTex;\n"
"uniform sampler2D lightShadeTex;\n"
"uniform vec2 lightOffset;\n"
"varying vec3 V;\n"

"void main(void) {"
	"vec4 diffuse = texture2D( mainTex, gl_TexCoord[0].xy );\n"
	"vec2 lighting = texture3D( lightTex, gl_TexCoord[1].xyz ).ga + lightOffset;\n"
	"vec4 foliage = texture2D( foliageTex, gl_TexCoord[1].xy );\n"
	"vec4 light = texture2D( lightShadeTex, lighting );\n"

	//"float light = exp( lnlightbase * (1.0 - max( lighting.r, lighting.g * daylight )) );\n"
	"float fogInterp = clamp( (length(V) - gl_Fog.start) * gl_Fog.scale, 0.0, 1.0);\n"
	"gl_FragColor = vec4( mix( light.rgb * foliage.rgb * diffuse.rgb * gl_FrontMaterial.diffuse.rgb, gl_Fog.color.rgb, fogInterp*fogInterp), diffuse.a );\n"
"}\n"
;

static const char *fragment_program_foliage_alpha =
"#version 110\n"
"uniform sampler2D mainTex;\n"
"uniform sampler3D lightTex;\n"
"uniform sampler2D foliageTex;\n"
"uniform sampler2D lightShadeTex;\n"
"uniform vec2 lightOffset;\n"
"varying vec3 V;\n"

"void main(void) {"
	"vec4 diffuse = texture2D( mainTex, gl_TexCoord[0].xy );\n"
	"vec2 lighting = texture3D( lightTex, gl_TexCoord[1].xyz ).ga + lightOffset;\n"
	"vec4 foliage = texture2D( foliageTex, gl_TexCoord[1].xy );\n"
	"vec4 light = texture2D( lightShadeTex, lighting );\n"

	//"float light = exp( lnlightbase * (1.0 - max( lighting.r, lighting.g * daylight )) );\n"
	"float fogInterp = clamp( (length(V) - gl_Fog.start) * gl_Fog.scale, 0.0, 1.0);\n"
	"gl_FragColor = vec4( mix( light.rgb * (diffuse.rgb + diffuse.a * foliage.rgb) * gl_FrontMaterial.diffuse.rgb, gl_Fog.color.rgb, fogInterp*fogInterp), 1.0 );\n"
"}\n"
;

void onError( const char *context, const char *error );

EihortShader::EihortShader()
{
	{
		char err[1024];
		if( !vtxObj.makeVertexShader( vertex_program, err, sizeof(err) ) )
			onError( "vertex shader compilation", err );
		if( !fragObj.makeFragmentShader( fragment_program, err, sizeof(err) ) )
			onError( "fragment shader compilation", err );
		if( !vtxObjTexGen.makeVertexShader( vertex_program_texGen0, err, sizeof(err) ) )
			onError( "vertex shader (texGen0) compilation", err );
		if( !fragObjFoliage.makeFragmentShader( fragment_program_foliage, err, sizeof(err) ) )
			onError( "fragment shader (foliage) compilation", err );
		if( !fragObjFoliageAlpha.makeFragmentShader( fragment_program_foliage_alpha, err, sizeof(err) ) )
			onError( "fragment shader (foliage in alpha) compilation", err );
	}

	normal.link( &vtxObj, &fragObj, "normal" );
	texGen.link( &vtxObjTexGen, &fragObj, "texgen" );
	foliage.link( &vtxObjTexGen, &fragObjFoliage, "foliage" );
	foliageAlpha.link( &vtxObjTexGen, &fragObjFoliageAlpha, "foliage in alpha" );

	bound = NULL;
	unbind();
}

void EihortShader::ShaderFlavour::link( GLShaderObject *vs, GLShaderObject *fs, const char *name ) {
	shader.attach( vs );
	shader.attach( fs );
	char err[1024];
	if( !shader.link( err, sizeof(err) ) ) {
		char context[64];
		snprintf( context, 64, "%s shader linking", name );
		onError( context, err );
	}
	lightOffsetUniform = shader.uniform( "lightOffset" );

	shader.bind();
	glUniform1i( shader.uniform( "mainTex" ), 0 );
	glUniform1i( shader.uniform( "lightTex" ), 1 );
	int foliageTexUniform = shader.uniform( "foliageTex" );
	if( foliageTexUniform >= 0 )
		glUniform1i( foliageTexUniform, 2 );
	glUniform1i( shader.uniform( "lightShadeTex" ), 3 );
	glUniform2f( lightOffsetUniform, 0.0f, 0.0f );
}

EihortShader::~EihortShader() {
}

void EihortShader::bindFlavour( ShaderFlavour *flv ) {
	if( bound != flv ) {
		bound = flv;
		flv->shader.bind();
	}
}

void EihortShader::unbind() {
	if( bound ) {
		bound->shader.unbind();
		bound = NULL;
	}
}

void EihortShader::setLightOffset( float blockLight, float skyLight ) {
	if( bound )
		glUniform2f( bound->lightOffsetUniform, blockLight, skyLight );
}


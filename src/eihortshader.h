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

#ifndef EIHORTSHADER_H
#define EIHORTSHADER_H

#include "glshader.h"

class EihortShader {
public:
	EihortShader();
	~EihortShader();

	void bindNormal() { bindFlavour( &normal ); }
	void bindTexGen() { bindFlavour( &texGen ); }
	void bindFoliage() { bindFlavour( &foliage ); }
	void bindFoliageAlpha() { bindFlavour( &foliageAlpha ); }
	void unbind();

	// The shader must be bound
	void setLightOffset( float blockLight, float skyLight );

private:
	struct ShaderFlavour {
		void link( GLShaderObject *vs, GLShaderObject *fs, const char *name );

		GLShader shader;
		int lightOffsetUniform;
	};

	ShaderFlavour normal, texGen, foliage, foliageAlpha;
	ShaderFlavour *bound;

	void bindFlavour( ShaderFlavour *flv );

	GLShaderObject vtxObj;
	GLShaderObject vtxObjTexGen;
	GLShaderObject fragObj;
	GLShaderObject fragObjFoliage;
	GLShaderObject fragObjFoliageAlpha;
};


#endif

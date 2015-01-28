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

#ifndef UITHEME_H
#define UITHEME_H

// Whole window
const unsigned BACKGROUND_COL = 0xff000000u;

// Default label appearance
const float    LABEL_FONT_HT = 20.0f;
const unsigned LABEL_FONT_COL = 0xffeeeeffu;

// Default Button appearance
const unsigned BUTTON_COL[] = { 0xff1f68a6u, 0xff193198u, 0xff1f68a6u, 0xff193198u };
const unsigned BUTTON_MO_COL[] = { 0xff3f88c6u, 0xff90c0a0u, 0xff3f88c6u, 0xff90c0a0u };
const unsigned BUTTON_MD_COL[] = { 0xffc6883fu, 0xffb85139u, 0xffc6883fu, 0xffb85139u };
const float    BUTTON_FONT_HT = 22.0f;
const unsigned BUTTON_FONT_COL = 0xffeeeeffu;

// Default Toggle appearance
const unsigned TOGGLE_MD_COL[] = { 0xffc6883fu, 0xffb85139u, 0xffc6883fu, 0xffb85139u };
const unsigned TOGGLE_SET_COL[] = { 0xff1f68a6u, 0xff193198u, 0xff193198u, 0xff1f68a6u };
const unsigned TOGGLE_BORDER_COL[] = { 0xff1f68a6u, 0xff193198u, 0xff1f68a6u, 0xff193198u };
const unsigned TOGGLE_MO_BORDER_COL[] = { 0xff3f88c6u, 0xff90c0a0u, 0xff3f88c6u, 0xff90c0a0u };

// Tooltip appearance
const float    TOOLTIP_FONT_HT = 18.0f;
const unsigned TOOLTIP_BG_COL[] = { 0xff0b1e2eu, 0xff05121eu, 0xff0b1e2eu, 0xff05121eu };
const unsigned TOOLTIP_BORDER_COL[] = { 0xff1f68a6u, 0xff193198u, 0xff1f68a6u, 0xff193198u };
const float    TOOLTIP_BORDER_BUFFER = 0.003f;
const float    TOOLTIP_DX = 10.0f;
const float    TOOLTIP_DY = 10.0f;
const unsigned TOOLTIP_FONT_COL = 0xffffffffu;

#endif

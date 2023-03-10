/*-----------------------------------------------------------------------

Matt Marchant 2021 - 2023
http://trederia.blogspot.com

Super Video Golf - zlib licence.

This software is provided 'as-is', without any express or
implied warranty.In no event will the authors be held
liable for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute
it freely, subject to the following restrictions :

1. The origin of this software must not be misrepresented;
you must not claim that you wrote the original software.
If you use this software in a product, an acknowledgment
in the product documentation would be appreciated but
is not required.

2. Altered source versions must be plainly marked as such,
and must not be misrepresented as being the original software.

3. This notice may not be removed or altered from any
source distribution.

-----------------------------------------------------------------------*/

#pragma once

#include <string>

static const std::string MinimapVertex = R"(
        uniform mat4 u_worldMatrix;
        uniform mat4 u_viewProjectionMatrix;

        ATTRIBUTE vec2 a_position;
        ATTRIBUTE MED vec2 a_texCoord0;
        ATTRIBUTE LOW vec4 a_colour;

        VARYING_OUT LOW vec4 v_colour;
        VARYING_OUT MED vec2 v_texCoord;

        void main()
        {
            gl_Position = u_viewProjectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
            v_colour = a_colour;
            v_texCoord = a_texCoord0;
        })";

//minimap as in top down view of green
static const std::string MinimapFragment = R"(
        uniform sampler2D u_texture;

        VARYING_IN LOW vec4 v_colour;
        VARYING_IN MED vec2 v_texCoord;
        OUTPUT
        
        const float stepPos = (0.49 * 0.49);
        const float borderPos = (0.46 * 0.46);
        const vec4 borderColour = vec4(vec3(0.0), 0.25);

//these ought to be uniforms for texture
//res and screen scale
const float res = 100.0;// 66.0;
const float scale = 2.0;

        void main()
        {
            vec2 pos = (round(floor(v_texCoord * res) * scale) / scale) / res;

            vec2 dir = pos - vec2(0.5);
            float length2 = dot(dir,dir);

            FRAG_OUT = TEXTURE(u_texture, v_texCoord) * v_colour;

            FRAG_OUT = mix(FRAG_OUT, borderColour, step(borderPos, length2));
            FRAG_OUT.a *= 1.0 - step(stepPos, length2);
        })";

//minimap as in mini course view
//well, this is a silly inconsistency...
static const std::string MinimapViewVertex = R"(
        ATTRIBUTE vec2 a_position;
        ATTRIBUTE vec2 a_texCoord0;
        ATTRIBUTE vec4 a_colour;

        uniform mat4 u_worldMatrix;
        uniform mat4 u_viewProjectionMatrix;

        uniform mat4 u_coordMatrix = mat4(1.0);

        VARYING_OUT vec2 v_texCoord0;
        VARYING_OUT vec2 v_texCoord1;
        VARYING_OUT vec4 v_colour;

        void main()
        {
            gl_Position = u_viewProjectionMatrix * u_worldMatrix * vec4(a_position, 0.0, 1.0);
            v_texCoord0 = (u_coordMatrix * vec4(a_texCoord0, 0.0, 1.0)).xy;
            v_texCoord1 = a_texCoord0;
            v_colour = a_colour;
        })";

static const std::string MinimapViewFragment = R"(
        uniform sampler2D u_texture;

        VARYING_IN vec2 v_texCoord0;
        VARYING_IN vec2 v_texCoord1;
        VARYING_IN vec4 v_colour;

        OUTPUT

        const float RadiusOuter = (0.4995 * 0.4995);
        const float RadiusInner = (0.47 * 0.47);

//these ought to be uniforms for texture
//res and screen scale
const vec2 res = vec2(180.0, 100.0);
const float scale = 2.0;

        void main()
        {
            FRAG_OUT = TEXTURE(u_texture, v_texCoord0) * v_colour;

            //vec2 pos = v_texCoord1 - vec2(0.5);

            vec2 pos = (round(floor(v_texCoord1 * res) * scale) / scale) / res;
            pos -= vec2(0.5);
            float len2 = dot(pos, pos);

            FRAG_OUT.a *= 1.0 - step(RadiusInner, len2);

            vec4 bgColour = mix(vec4(vec3(0.0), 0.25), vec4(0.0), step(RadiusOuter, len2));
            FRAG_OUT = mix(bgColour, FRAG_OUT, FRAG_OUT.a);
        })";
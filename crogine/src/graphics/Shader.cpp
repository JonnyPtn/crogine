/*-----------------------------------------------------------------------

Matt Marchant 2017 - 2020
http://trederia.blogspot.com

crogine - Zlib license.

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

#include <crogine/graphics/Shader.hpp>
#include <crogine/core/FileSystem.hpp>

#include <crogine/detail/Types.hpp>

#include "../detail/GLCheck.hpp"

#include <vector>
#include <cstring>

using namespace cro;

namespace
{
    const std::string precision = R"(
        #if defined(MOBILE)
        #define HIGH highp
        #define MED mediump
        #define LOW lowp

        #define ATTRIBUTE attribute
        #define VARYING_OUT varying
        #define VARYING_IN varying
        #define FRAG_OUT gl_FragColor
        #define OUTPUT

        #define TEXTURE texture2D
        #define TEXTURE_CUBE textureCube

        #else

        #define HIGH
        #define MED
        #define LOW

        #define ATTRIBUTE in
        #define VARYING_OUT out
        #define VARYING_IN in
        #define FRAG_OUT fragOut
        #define OUTPUT out vec4 FRAG_OUT;

        #define TEXTURE texture
        #define TEXTURE_CUBE texture

        #endif
        )";
}

Shader::Shader()
    : m_handle  (0),
    m_attribMap ({})
{
    resetAttribMap();
}

Shader::Shader(Shader&& other) noexcept
{
    m_handle = other.m_handle;
    m_attribMap = other.m_attribMap;
    m_uniformMap = other.m_uniformMap;

    other.m_handle = 0;
    other.m_attribMap = {};
    other.m_uniformMap.clear();
}

Shader& Shader::operator=(Shader&& other) noexcept
{
    if (this != &other)
    {
        Shader temp;
        std::swap(m_handle, temp.m_handle);
        std::swap(m_attribMap, temp.m_attribMap);
        std::swap(m_uniformMap, temp.m_uniformMap);

        m_handle = other.m_handle;
        m_attribMap = other.m_attribMap;
        m_uniformMap = other.m_uniformMap;

        other.m_handle = 0;
        other.m_attribMap = {};
        other.m_uniformMap.clear();
    }
    return *this;
}

Shader::~Shader()
{
    if (m_handle)
    {
        glCheck(glDeleteProgram(m_handle));
    }
}

//public
bool Shader::loadFromFile(const std::string& vertex, const std::string& fragment)
{
    auto vertPath = FileSystem::getResourcePath() + vertex;
    auto fragPath = FileSystem::getResourcePath() + fragment;

    if (!FileSystem::fileExists(vertPath))
    {
        LogE << "Failed opening " << vertex << ": file not found" << std::endl;
        return false;
    }

    if (!FileSystem::fileExists(fragPath))
    {
        LogE << "Failed opening " << fragment << ": file not found" << std::endl;
        return false;
    }

    return loadFromString(parseFile(vertPath), parseFile(fragPath));
}

bool Shader::loadFromString(const std::string& vertex, const std::string& fragment, const std::string& defines)
{
    if (m_handle)
    {
        //remove existing program
        glCheck(glDeleteProgram(m_handle));
        m_handle = 0;
        resetAttribMap();
        resetUniformMap();
    }    
    
    //compile vert shader
    GLuint vertID = glCreateShader(GL_VERTEX_SHADER);
    
#ifdef __ANDROID__
    const char* src[] = { "#version 100\n#define MOBILE\n", precision.c_str(), defines.c_str(), vertex.c_str() };
#else
    const char* src[] = { "#version 150 core\n", precision.c_str(), defines.c_str(), vertex.c_str() };
#endif //__ANDROID__

    glCheck(glShaderSource(vertID, 4, src, nullptr));
    glCheck(glCompileShader(vertID));

    GLint result = GL_FALSE;
    int resultLength = 0;

    glCheck(glGetShaderiv(vertID, GL_COMPILE_STATUS, &result));
    glCheck(glGetShaderiv(vertID, GL_INFO_LOG_LENGTH, &resultLength));
    if (result == GL_FALSE)
    {
        //failed compilation
        std::string str;
        str.resize(resultLength + 1);
        glCheck(glGetShaderInfoLog(vertID, resultLength, nullptr, &str[0]));
        Logger::log("Failed compiling vertex shader: " + std::to_string(result) + ", " + str, Logger::Type::Error);

        glCheck(glDeleteShader(vertID));
        return false;
    }
    
    //compile frag shader
    GLuint fragID = glCreateShader(GL_FRAGMENT_SHADER);
    src[3] = fragment.c_str();
    glCheck(glShaderSource(fragID, 4, src, nullptr));
    glCheck(glCompileShader(fragID));

    result = GL_FALSE;
    resultLength = 0;

    glCheck(glGetShaderiv(fragID, GL_COMPILE_STATUS, &result));
    glCheck(glGetShaderiv(fragID, GL_INFO_LOG_LENGTH, &resultLength));
    if (result == GL_FALSE)
    {
        std::string str;
        str.resize(resultLength + 1);
        glCheck(glGetShaderInfoLog(fragID, resultLength, nullptr, &str[0]));
        Logger::log("Failed compiling fragment shader: " + std::to_string(result) + ", " + str, Logger::Type::Error);

        glCheck(glDeleteShader(vertID));
        glCheck(glDeleteShader(fragID));
        return false;
    }

    //link shaders to program
    m_handle = glCreateProgram();
    if (m_handle)
    {
        glCheck(glAttachShader(m_handle, vertID));
        glCheck(glAttachShader(m_handle, fragID));
        glCheck(glLinkProgram(m_handle));

        result = GL_FALSE;
        resultLength = 0;
        glCheck(glGetProgramiv(m_handle, GL_LINK_STATUS, &result));
        glCheck(glGetProgramiv(m_handle, GL_INFO_LOG_LENGTH, &resultLength));
        if (result == GL_FALSE)
        {
            std::string str;
            str.resize(resultLength + 1);
            glCheck(glGetProgramInfoLog(m_handle, resultLength, nullptr, &str[0]));
            Logger::log("Failed to link shader program: " + std::to_string(result) + ", " + str, Logger::Type::Error);

            glCheck(glDetachShader(m_handle, vertID));
            glCheck(glDetachShader(m_handle, fragID));

            glCheck(glDeleteShader(vertID));
            glCheck(glDeleteShader(fragID));
            glCheck(glDeleteProgram(m_handle));
            m_handle = 0;
            return false;
        }
        else
        {
            //tidy
            glCheck(glDetachShader(m_handle, vertID));
            glCheck(glDetachShader(m_handle, fragID));

            glCheck(glDeleteShader(vertID));
            glCheck(glDeleteShader(fragID));

            //grab attributes
            if (!fillAttribMap())
            {
                glCheck(glDeleteProgram(m_handle));
                m_handle = 0;
                return false;
            }

            fillUniformMap();

            return true;
        }
    }

    return false;
}

uint32 Shader::getGLHandle() const
{
    return m_handle;
}

const std::array<int32, Mesh::Attribute::Total>& Shader::getAttribMap() const
{
    return m_attribMap;
}

const std::unordered_map<std::string, int32>& Shader::getUniformMap() const
{
    return m_uniformMap;
}

//private
bool Shader::fillAttribMap()
{
    GLint activeAttribs;
    glCheck(glGetProgramiv(m_handle, GL_ACTIVE_ATTRIBUTES, &activeAttribs));
    if (activeAttribs > 0)
    {
        GLint length;
        glCheck(glGetProgramiv(m_handle, GL_ACTIVE_ATTRIBUTE_MAX_LENGTH, &length));
        if (length > 0)
        {
            std::vector<GLchar> attribName(length + 1);
            GLint attribSize;
            GLenum attribType;
            GLint attribLocation;

            for (auto i = 0; i < activeAttribs; ++i)
            {
                glCheck(glGetActiveAttrib(m_handle, i, length, nullptr, &attribSize, &attribType, attribName.data()));
                attribName[length] = '\0';

                glCheck(attribLocation = glGetAttribLocation(m_handle, attribName.data()));
                std::string name(attribName.data());
                if (name == "a_position")
                {
                    m_attribMap[Mesh::Position] = attribLocation;
                }
                else if (name == "a_colour")
                {
                    m_attribMap[Mesh::Colour] = attribLocation;
                }
                else if (name == "a_normal")
                {
                    m_attribMap[Mesh::Normal] = attribLocation;
                }
                else if (name == "a_tangent")
                {
                    m_attribMap[Mesh::Tangent] = attribLocation;
                }
                else if (name == "a_bitangent")
                {
                    m_attribMap[Mesh::Bitangent] = attribLocation;
                }
                else if (name == "a_texCoord0")
                {
                    m_attribMap[Mesh::UV0] = attribLocation;
                }
                else if (name == "a_texCoord1")
                {
                    m_attribMap[Mesh::UV1] = attribLocation;
                }
                else if (name == "a_boneIndices")
                {
                    m_attribMap[Mesh::BlendIndices] = attribLocation;
                }
                else if (name == "a_boneWeights")
                {
                    m_attribMap[Mesh::BlendWeights] = attribLocation;
                }
                else
                {
                    Logger::log(name + ": unknown vertex attribute. Shader compilation failed.", Logger::Type::Error);
                    return false;
                }              
            }
            return true;
        }
        Logger::log("Failed loading shader attributes for some reason...", Logger::Type::Error);
        return false;
    }
    Logger::log("No vertex attributes were found in shader - compilation failed.", Logger::Type::Error);
    return false;
}

void Shader::resetAttribMap()
{
    std::memset(m_attribMap.data(), -1, m_attribMap.size() * sizeof(int32));
}

void Shader::fillUniformMap()
{
    int32 uniformCount;
    glCheck(glGetProgramiv(m_handle, GL_ACTIVE_UNIFORMS, &uniformCount));

    for (auto i = 0; i < uniformCount; ++i)
    {
        int32 nameLength;
        int32 size;
        GLenum type;
        static const int32 maxChar = 100;
        char str[maxChar];
        glCheck(glGetActiveUniform(m_handle, i, maxChar -1, &nameLength, &size, &type, str));
        str[nameLength] = 0;

        GLuint location = 0;
        glCheck(location = glGetUniformLocation(m_handle, str));
        m_uniformMap.insert(std::make_pair(std::string(str), location));
    }
}

void Shader::resetUniformMap()
{
    m_uniformMap.clear();
}

std::string Shader::parseFile(const std::string& path)
{
    std::string retVal;
    retVal.reserve(1000);

    //open file and verify
    RaiiRWops file;
    file.file = SDL_RWFromFile(path.c_str(), "r");
    if (!file.file)
    {
        Logger::log("Failed opening " + path, Logger::Type::Error);
        return {};
    }

    char buf;
    while (SDL_RWread(file.file, &buf, 1, 1))
    {
        retVal.push_back(buf);
    }

    //TODO we could do some primitive parsing on the file such as removing
    //version directives or parsing our own includes, but for now
    //just assume the user knows what they're doing and let the shader
    //compilation fail if it finds two version directives.

    return retVal;
}
# Install script for directory: D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/Assimp")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "libassimp6.0.2-dev" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Build/lib/Debug/assimp-vc143-mtd.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Build/lib/Release/assimp-vc143-mt.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Build/lib/MinSizeRel/assimp-vc143-mt.lib")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Build/lib/RelWithDebInfo/assimp-vc143-mt.lib")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "libassimp6.0.2" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Build/bin/Debug/assimp-vc143-mtd.dll")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Build/bin/Release/assimp-vc143-mt.dll")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Build/bin/MinSizeRel/assimp-vc143-mt.dll")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Build/bin/RelWithDebInfo/assimp-vc143-mt.dll")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "assimp-dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/assimp" TYPE FILE FILES
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/anim.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/aabb.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/ai_assert.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/camera.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/color4.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/color4.inl"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Build/code/../include/assimp/config.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/ColladaMetaData.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/commonMetaData.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/defs.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/cfileio.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/light.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/material.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/material.inl"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/matrix3x3.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/matrix3x3.inl"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/matrix4x4.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/matrix4x4.inl"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/mesh.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/ObjMaterial.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/pbrmaterial.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/GltfMaterial.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/postprocess.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/quaternion.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/quaternion.inl"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/scene.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/metadata.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/texture.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/types.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/vector2.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/vector2.inl"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/vector3.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/vector3.inl"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/version.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/cimport.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/AssertHandler.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/importerdesc.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/Importer.hpp"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/DefaultLogger.hpp"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/ProgressHandler.hpp"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/IOStream.hpp"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/IOSystem.hpp"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/Logger.hpp"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/LogStream.hpp"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/NullLogger.hpp"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/cexport.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/Exporter.hpp"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/DefaultIOStream.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/DefaultIOSystem.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/ZipArchiveIOSystem.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/SceneCombiner.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/fast_atof.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/qnan.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/BaseImporter.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/Hash.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/MemoryIOWrapper.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/ParsingUtils.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/StreamReader.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/StreamWriter.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/StringComparison.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/StringUtils.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/SGSpatialSort.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/GenericProperty.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/SpatialSort.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/SkeletonMeshBuilder.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/SmallVector.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/SmoothingGroups.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/SmoothingGroups.inl"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/StandardShapes.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/RemoveComments.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/Subdivision.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/Vertex.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/LineSplitter.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/TinyFormatter.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/Profiler.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/LogAux.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/Bitmap.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/XMLTools.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/IOStreamBuffer.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/CreateAnimMesh.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/XmlParser.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/BlobIOSystem.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/MathFunctions.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/Exceptional.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/ByteSwapper.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/Base64.hpp"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "assimp-dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/assimp/Compiler" TYPE FILE FILES
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/Compiler/pushpack1.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/Compiler/poppack1.h"
    "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Release/code/../include/assimp/Compiler/pstdint.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE FILE FILES "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Build/bin/Debug/assimp-vc143-mtd.pdb")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE FILE FILES "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Build/bin/Release/assimp-vc143-mt.pdb")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE FILE FILES "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Build/bin/MinSizeRel/assimp-vc143-mt.pdb")
  elseif(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE FILE FILES "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Build/bin/RelWithDebInfo/assimp-vc143-mt.pdb")
  endif()
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "D:/DX12/ElysiaRenderer/ElysiaRenderer/Assimp/Build/code/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()

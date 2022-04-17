#include "mdlparser.h"
#include "utility.h"

#include <cstdio>
#include <stdexcept>
#include <fmt/core.h>
#include <array>
#include <fstream>
#include <algorithm>

Model parseMDL(MemorySpan data) {
    struct ModelFileHeader {
        uint32_t version;
        uint32_t stackSize;
        uint32_t runtimeSize;
        unsigned short vertexDeclarationCount;
        unsigned short materialCount;
        uint32_t vertexOffsets[3];
        uint32_t indexOffsets[3];
        uint32_t vertexBufferSize[3];
        uint32_t indexBufferSize[3];
        uint8_t lodCount;
        bool indexBufferStreamingEnabled;
        bool hasEdgeGeometry;
        uint8_t padding;
    } modelFileHeader;

    data.read(&modelFileHeader);

    struct VertexElement {
        uint8_t stream, offset, type, usage, usageIndex;
        uint8_t padding[3];
    };

    struct VertexDeclaration {
        std::vector<VertexElement> elements;
    };

    std::vector<VertexDeclaration> vertexDecls(modelFileHeader.vertexDeclarationCount);
    for(int i = 0; i < modelFileHeader.vertexDeclarationCount; i++) {
        VertexElement element {};
        data.read(&element);

        do {
            vertexDecls[i].elements.push_back(element);
            data.read(&element);
        } while (element.stream != 255);

        int toSeek = 17 * 8 - (vertexDecls[i].elements.size() + 1) * 8;
        data.seek(toSeek, Seek::Current);
    }

    uint16_t stringCount;
    data.read(&stringCount);

    // dummy
    data.seek(sizeof(uint16_t), Seek::Current);

    uint32_t stringSize;
    data.read(&stringSize);

    std::vector<uint8_t> strings;
    data.read_structures(&strings, stringSize);

    enum ModelFlags1 : uint8_t
    {
        DustOcclusionEnabled = 0x80,
        SnowOcclusionEnabled = 0x40,
        RainOcclusionEnabled = 0x20,
        Unknown1 = 0x10,
        LightingReflectionEnabled = 0x08,
        WavingAnimationDisabled = 0x04,
        LightShadowDisabled = 0x02,
        ShadowDisabled = 0x01,
    };

    enum ModelFlags2 : uint8_t
    {
        Unknown2 = 0x80,
        BgUvScrollEnabled = 0x40,
        EnableForceNonResident = 0x20,
        ExtraLodEnabled = 0x10,
        ShadowMaskEnabled = 0x08,
        ForceLodRangeEnabled = 0x04,
        EdgeGeometryEnabled = 0x02,
        Unknown3 = 0x01
    };

    struct ModelHeader {
        float radius;
        unsigned short meshCount;
        unsigned short attributeCount;
        unsigned short submeshCount;
        unsigned short materialCount;
        unsigned short boneCount;
        unsigned short boneTableCount;
        unsigned short shapeCount;
        unsigned short shapeMeshCount;
        unsigned short shapeValueCount;
        uint8_t lodCount;

        ModelFlags1 flags1;

        unsigned short elementIdCount;
        uint8_t terrainShadowMeshCount;

        ModelFlags2 flags2;

        float modelClipOutDistance;
        float shadowClipOutDistance;
        unsigned short unknown4;
        unsigned short terrainShadowSubmeshCount;

        uint8_t unknown5;

        uint8_t bgChangeMaterialIndex;
        uint8_t bgCrestChangeMaterialIndex;
        uint8_t unknown6;
        unsigned short unknown7, unknown8, unknown9;
        uint8_t padding[6];
    } modelHeader;

    data.read(&modelHeader);

    struct ElementId {
        uint32_t elementId;
        uint32_t  parentBoneName;
        std::vector<float> translate;
        std::vector<float> rotate;
    };

    std::vector<ElementId> elementIds(modelHeader.elementIdCount);
    for(int i = 0; i < modelHeader.elementIdCount; i++) {
        data.read(&elementIds[i].elementId);
        data.read(&elementIds[i].parentBoneName);

        // FIXME: these always seem to be 3, convert to static array? then we could probably read this all in one go!
        data.read_structures(&elementIds[i].translate, 3);
        data.read_structures(&elementIds[i].rotate, 3);
    }

    struct Lod {
        unsigned short meshIndex;
        unsigned short meshCount;
        float modelLodRange;
        float textureLodRange;
        unsigned short waterMeshIndex;
        unsigned short waterMeshCount;
        unsigned short shadowMeshIndex;
        unsigned short shadowMeshCount;
        unsigned short terrainShadowMeshIndex;
        unsigned short terrainShadowMeshCount;
        unsigned short verticalFogMeshIndex;
        unsigned short verticalFogMeshCount;

        // unused on win32 according to lumina devs
        unsigned int edgeGeometrySize;
        unsigned int edgeGeometryDataOffset;
        unsigned int polygonCount;
        unsigned int unknown1;
        unsigned int vertexBufferSize;
        unsigned int indexBufferSize;
        unsigned int vertexDataOffset;
        unsigned int indexDataOffset;
    };

    std::vector<Lod> lods;

    // TODO: support models that support more than 3 lods
    data.read_structures(&lods, 3);

    struct Mesh {
        unsigned short vertexCount;
        unsigned short padding;
        unsigned int indexCount;
        unsigned short materialIndex;
        unsigned short subMeshIndex;
        unsigned short subMeshCount;
        unsigned short boneTableIndex;
        unsigned int startIndex;

        std::vector<uint32_t> vertexBufferOffset;
        std::vector<uint8_t> vertexBufferStride;

        uint8_t vertexStreamCount;
    };

    std::vector<Mesh> meshes(modelHeader.meshCount);
    for(int i = 0; i < modelHeader.meshCount; i++) {
        data.read(&meshes[i].vertexCount);
        data.read(&meshes[i].padding);
        data.read(&meshes[i].indexCount);
        data.read(&meshes[i].materialIndex);
        data.read(&meshes[i].subMeshIndex);
        data.read(&meshes[i].subMeshCount);
        data.read(&meshes[i].boneTableIndex);
        data.read(&meshes[i].startIndex);

        data.read_structures(&meshes[i].vertexBufferOffset, 3);
        data.read_structures(&meshes[i].vertexBufferStride, 3);

        data.read(&meshes[i].vertexStreamCount);
    }

    std::vector<uint32_t> attributeNameOffsets;
    data.read_structures(&attributeNameOffsets, modelHeader.attributeCount);

    // TODO: implement terrain shadow meshes

    struct Submesh {
        unsigned int indexOffset;
        unsigned int indexCount;
        unsigned int attributeIndexMask;
        unsigned short boneStartIndex;
        unsigned short boneCount;
    };

    std::vector<Submesh> submeshes;
    data.read_structures(&submeshes, modelHeader.submeshCount);

    // TODO: implement terrain shadow submeshes

    std::vector<uint32_t> materialNameOffsets;
    data.read_structures(&materialNameOffsets, modelHeader.materialCount);

    std::vector<uint32_t> boneNameOffsets;
    data.read_structures(&boneNameOffsets, modelHeader.boneCount);

    struct BoneTable {
        std::vector<uint16_t> boneIndex;
        uint8_t boneCount;
        std::vector<uint8_t> padding;
    };

    std::vector<BoneTable> boneTables(modelHeader.boneTableCount);
    for(int i = 0; i < modelHeader.boneTableCount; i++) {
        data.read_structures(&boneTables[i].boneIndex, 64);

        data.read(&boneTables[i].boneCount);

        data.read_structures(&boneTables[i].padding, 3);
    }

    // TODO: implement shapes

    uint32_t submeshBoneMapSize;
    data.read(&submeshBoneMapSize);

    std::vector<uint16_t> submeshBoneMap;
    data.read_structures(&submeshBoneMap, (int)submeshBoneMapSize / 2);

    uint8_t paddingAmount;
    data.read(&paddingAmount);

    data.seek(paddingAmount, Seek::Current);

    struct BoundingBox {
        std::array<float, 4> min, max;
    };

    BoundingBox boundingBoxes, modelBoundingBoxes, waterBoundingBoxes, verticalFogBoundingBoxes;
    data.read(&boundingBoxes);
    data.read(&modelBoundingBoxes);
    data.read(&waterBoundingBoxes);
    data.read(&verticalFogBoundingBoxes);

    std::vector<BoundingBox> boneBoundingBoxes;
    data.read_structures(&boneBoundingBoxes, modelHeader.boneCount);

    Model model;

    for(int i = 0; i < modelHeader.lodCount; i++) {
        ::Lod lod;

        for(int j = lods[i].meshIndex; j < (lods[i].meshIndex + lods[i].meshCount); j++) {
            Part part;

            const VertexDeclaration decl = vertexDecls[j];

            enum VertexType : uint8_t {
                Single3 = 2,
                Single4 = 3,
                UInt = 5,
                ByteFloat4 = 8,
                Half2 = 13,
                Half4 = 14
            };

            enum VertexUsage : uint8_t  {
                Position = 0,
                BlendWeights = 1,
                BlendIndices = 2,
                Normal = 3,
                UV = 4,
                Tangent2 = 5,
                Tangent1 = 6,
                Color = 7,
            };

            int vertexCount = meshes[j].vertexCount;
            std::vector<Vertex> vertices(vertexCount);

            for(int k = 0; k < vertexCount; k++) {
                for(auto& orderedElement : decl.elements) {
                    auto type = static_cast<VertexType>(orderedElement.type);
                    auto usage = static_cast<VertexUsage>(orderedElement.usage);

                    const int stream = orderedElement.stream;

                    data.seek(lods[i].vertexDataOffset + meshes[j].vertexBufferOffset[stream] + orderedElement.offset + meshes[i].vertexBufferStride[stream] * k, Seek::Set);

                    std::array<float, 4> floatData = {};

                    switch(type) {
                        case VertexType::Single3:
                            data.read_array(floatData.data(), 3);
                            break;
                        case VertexType::Single4:
                            data.read_array(floatData.data(), 4);
                            break;
                        case VertexType::UInt:
                            data.seek(sizeof(uint8_t) * 4, Seek::Current);
                            break;
                        case VertexType::ByteFloat4: {
                            uint8_t values[4];
                            data.read_array(values, 4);

                            floatData[0] = byte_to_float(values[0]);
                            floatData[1] = byte_to_float(values[1]);
                            floatData[2] = byte_to_float(values[2]);
                            floatData[3] = byte_to_float(values[3]);
                        }
                            break;
                        case VertexType::Half2: {
                            uint16_t values[2];
                            data.read_array(values, 2);

                            floatData[0] = half_to_float(values[0]);
                            floatData[1] = half_to_float(values[1]);
                        }
                            break;
                        case VertexType::Half4: {
                            uint16_t values[4];
                            data.read_array(values, 4);

                            floatData[0] = half_to_float(values[0]);
                            floatData[1] = half_to_float(values[1]);
                            floatData[2] = half_to_float(values[2]);
                            floatData[3] = half_to_float(values[3]);
                        }
                            break;
                    }

                    switch(usage) {
                        case VertexUsage::Position:
                            memcpy(vertices[k].position.data(), floatData.data(), sizeof(float) * 3);
                            break;
                        case VertexUsage::Normal:
                            memcpy(vertices[k].normal.data(), floatData.data(), sizeof(float) * 3);
                            break;
                    }
                }
            }

            data.seek(modelFileHeader.indexOffsets[i] + (meshes[j].startIndex * 2), Seek::Set);

            std::vector<uint16_t> indices;
            data.read_structures(&indices, meshes[j].indexCount);

            part.indices = indices;
            part.vertices = vertices;

            lod.parts.push_back(part);
        }

        model.lods.push_back(lod);
    }

    return model;
}
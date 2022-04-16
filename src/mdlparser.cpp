#include "mdlparser.h"

#include <cstdio>
#include <stdexcept>
#include <fmt/core.h>
#include <array>
#include <fstream>
#include <algorithm>

using ushort = unsigned short;
using uint = unsigned int;

// from lumina.halfextensions
static float Unpack(ushort value) {
    uint num3;
    if ((value & -33792) == 0) {
        if ((value & 0x3ff) != 0) {
            auto num2 = 0xfffffff2;
            auto num = (uint) (value & 0x3ff);
            while ((num & 0x400) == 0) {
                num2--;
                num <<= 1;
            }

            num &= 0xfffffbff;
            num3 = ((uint) (value & 0x8000) << 0x10) | ((num2 + 0x7f) << 0x17) | (num << 13);
        } else {
            num3 = (uint) ((value & 0x8000) << 0x10);
        }
    } else {
        num3 =
                (uint)
                        (((value & 0x8000) << 0x10) |
                         ((((value >> 10) & 0x1f) - 15 + 0x7f) << 0x17) |
                         ((value & 0x3ff) << 13));
    }

    return *(float *) &num3;
}

Model parseMDL(const std::string_view path) {
    FILE* file = fopen(path.data(), "rb");
    if(file == nullptr) {
        throw std::runtime_error("Failed to open exh file " + std::string(path));
    }

    enum class FileType : int32_t {
        Empty = 1,
        Standard = 2,
        Model = 3,
        Texture = 4
    };

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

    fread(&modelFileHeader, sizeof(ModelFileHeader), 1, file);

    fmt::print("stack size: {}\n", modelFileHeader.stackSize);

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
        fread(&element, sizeof(VertexElement), 1, file);
        do {
            vertexDecls[i].elements.push_back(element);
            fread(&element, sizeof(VertexElement), 1, file);
        } while (element.stream != 255);

        int toSeek = 17 * 8 - (vertexDecls[i].elements.size() + 1) * 8;
        fseek(file, toSeek, SEEK_CUR);
    }

    uint16_t stringCount;
    fread(&stringCount, sizeof(uint16_t), 1, file);

    fmt::print("string count: {}\n", stringCount);

    // dummy
    fseek(file, sizeof(uint16_t), SEEK_CUR);

    uint32_t stringSize;
    fread(&stringSize, sizeof(uint32_t), 1, file);

    std::vector<uint8_t> strings(stringSize);
    fread(strings.data(), stringSize, 1, file);

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

    fread(&modelHeader, sizeof(modelHeader), 1, file);

    fmt::print("mesh count: {}\n", modelHeader.meshCount);
    fmt::print("attribute count: {}\n", modelHeader.attributeCount);

    struct ElementId {
        unsigned int elementId;
        unsigned int parentBoneName;
        std::vector<float> translate;
        std::vector<float> rotate;
    };

    std::vector<ElementId> elementIds(modelHeader.elementIdCount);
    for(int i = 0; i < modelHeader.elementIdCount; i++) {
        fread(&elementIds[i].elementId, sizeof(uint32_t), 1, file);
        fread(&elementIds[i].parentBoneName, sizeof(uint32_t), 1, file);

        elementIds[i].translate.resize(3); // FIXME: these always seem to be 3, convert to static array? then we could probably fread this all in one go!
        elementIds[i].rotate.resize(3);

        fread(elementIds[i].translate.data(), sizeof(float) * 3, 1, file);
        fread(elementIds[i].rotate.data(), sizeof(float) * 3, 1, file);
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

    std::array<Lod, 3> lods;
    fread(lods.data(), sizeof(Lod) * 3, 1, file);

    // TODO: support models that support more than 3 lods

    struct Mesh {
        unsigned short vertexCount;
        unsigned short padding;
        unsigned int indexCount;
        unsigned short materialIndex;
        unsigned short subMeshIndex;
        unsigned short subMeshCount;
        unsigned short boneTableIndex;
        unsigned int startIndex;

        std::vector<unsigned int> vertexBufferOffset;
        std::vector<uint8_t> vertexBufferStride;

        uint8_t vertexStreamCount;
    };

    std::vector<Mesh> meshes(modelHeader.meshCount);
    for(int i = 0; i < modelHeader.meshCount; i++) {
        fread(&meshes[i].vertexCount, sizeof(uint16_t), 1, file);
        fread(&meshes[i].padding, sizeof(uint16_t), 1, file);
        fread(&meshes[i].indexCount, sizeof(uint32_t), 1, file);
        fread(&meshes[i].materialIndex, sizeof(uint16_t), 1, file);
        fread(&meshes[i].subMeshIndex, sizeof(uint16_t), 1, file);
        fread(&meshes[i].subMeshCount, sizeof(uint16_t), 1, file);
        fread(&meshes[i].boneTableIndex, sizeof(uint16_t), 1, file);
        fread(&meshes[i].startIndex, sizeof(uint32_t), 1, file);

        meshes[i].vertexBufferOffset.resize(3);
        fread(meshes[i].vertexBufferOffset.data(), sizeof(uint32_t) * 3, 1, file);

        meshes[i].vertexBufferStride.resize(3);
        fread(meshes[i].vertexBufferStride.data(), sizeof(uint8_t) * 3, 1, file);

        fread(&meshes[i].vertexStreamCount, sizeof(uint8_t), 1, file);
    }

    std::vector<uint32_t> attributeNameOffsets(modelHeader.attributeCount);
    fread(attributeNameOffsets.data(), sizeof(uint32_t) * modelHeader.attributeCount, 1, file);

    // TODO: implement terrain shadow meshes

    struct Submesh {
        unsigned int indexOffset;
        unsigned int indexCount;
        unsigned int attributeIndexMask;
        unsigned short boneStartIndex;
        unsigned short boneCount;
    };

    std::vector<Submesh> submeshes(modelHeader.submeshCount);
    for(int i = 0; i < modelHeader.submeshCount; i++) {
        fread(&submeshes[i], sizeof(Submesh), 1, file);
    }

    // TODO: implement terrain shadow submeshes

    std::vector<uint32_t> materialNameOffsets(modelHeader.materialCount);
    fread(materialNameOffsets.data(), sizeof(uint32_t) * modelHeader.materialCount, 1, file);

    std::vector<uint32_t> boneNameOffsets(modelHeader.boneCount);
    fread(boneNameOffsets.data(), sizeof(uint32_t) * modelHeader.boneCount, 1, file);

    struct BoneTable {
        std::vector<unsigned short> boneIndex;
        uint8_t boneCount;
        std::vector<uint8_t> padding;
    };

    std::vector<BoneTable> boneTables(modelHeader.boneTableCount);
    for(int i = 0; i < modelHeader.boneTableCount; i++) {
        boneTables[i].boneIndex.resize(64);
        fread(boneTables[i].boneIndex.data(), 64 * sizeof(uint16_t), 1, file);
        fread(&boneTables[i].boneCount, sizeof(uint8_t), 1, file);
        boneTables[i].padding.resize(3);
        fread(boneTables[i].padding.data(), sizeof(uint8_t) * 3, 1, file);

        fmt::print("bone count: {}\n", boneTables[i].boneCount);
    }

    // TODO: implement shapes

    unsigned int submeshBoneMapSize;
    fread(&submeshBoneMapSize, sizeof(uint32_t), 1, file);

    std::vector<uint16_t > submeshBoneMap((int)submeshBoneMapSize / 2);
    fread(submeshBoneMap.data(), submeshBoneMap.size() * sizeof(uint16_t), 1, file);

    uint8_t paddingAmount;
    fread(&paddingAmount, sizeof(uint8_t), 1, file);

    fseek(file, paddingAmount, SEEK_CUR);

    struct BoundingBox {
        std::array<float, 4> min, max;
    };

    BoundingBox boundingBoxes, modelBoundingBoxes, waterBoundingBoxes, verticalFogBoundingBoxes;
    fread(&boundingBoxes, sizeof(BoundingBox), 1, file);
    fread(&modelBoundingBoxes, sizeof(BoundingBox), 1, file);
    fread(&waterBoundingBoxes, sizeof(BoundingBox), 1, file);
    fread(&verticalFogBoundingBoxes, sizeof(BoundingBox), 1, file);

    std::vector<BoundingBox> boneBoundingBoxes(modelHeader.boneCount);
    fread(boneBoundingBoxes.data(), modelHeader.boneCount * sizeof(BoundingBox), 1, file);

    fmt::print("Successfully read mdl file!\n");

    fmt::print("Now exporting as test.obj...\n");

    Model model;

    // TODO: doesn't work for lod above 0
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
                for(auto & orderedElement : decl.elements) {
                    VertexType type = (VertexType)orderedElement.type;
                    VertexUsage usage = (VertexUsage)orderedElement.usage;

                    const int stream = orderedElement.stream;

                    fseek(file, lods[i].vertexDataOffset + meshes[j].vertexBufferOffset[stream] + orderedElement.offset + meshes[i].vertexBufferStride[stream] * k, SEEK_SET);

                    std::array<float, 4> floatData = {};

                    switch(type) {
                        case VertexType::Single3:
                            fread(floatData.data(), sizeof(float) * 3, 1, file);
                            break;
                        case VertexType::Single4:
                            fread(floatData.data(), sizeof(float) * 4, 1, file);
                            break;
                        case VertexType::UInt:
                            fseek(file, sizeof(uint8_t) * 4, SEEK_CUR);
                            break;
                        case VertexType::ByteFloat4:
                            fseek(file, sizeof(uint8_t) * 4, SEEK_CUR);
                            break;
                        case VertexType::Half2: {
                            uint16_t values[2];
                            fread(values, sizeof(uint16_t) * 2, 1, file);
                            floatData[0] = Unpack(values[0]);
                            floatData[1] = Unpack(values[1]);
                        }
                            break;
                        case VertexType::Half4: {
                            uint16_t values[4];
                            fread(values, sizeof(uint16_t) * 4, 1, file);
                            floatData[0] = Unpack(values[0]);
                            floatData[1] = Unpack(values[1]);
                            floatData[2] = Unpack(values[2]);
                            floatData[3] = Unpack(values[3]);
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

            fseek(file,  modelFileHeader.indexOffsets[i] + (meshes[j].startIndex * 2), SEEK_SET);
            std::vector<uint16_t> indices(meshes[j].indexCount);
            fread(indices.data(), meshes[j].indexCount * sizeof(uint16_t), 1, file);

            part.indices = indices;
            part.vertices = vertices;

            lod.parts.push_back(part);
        }

        model.lods.push_back(lod);
    }

    return model;
}
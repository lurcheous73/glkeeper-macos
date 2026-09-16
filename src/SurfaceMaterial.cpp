#include "stdafx.h"
#include "SurfaceMaterial.h"
#include "RenderDevice.h"
#include "TextureManager.h"

void SurfaceMaterial::BindMaterial(ShaderProgram& shaderProgram) const
{
    gRenderDevice.SetRenderState(mRenderStates);
    if (mDiffuseTexture)
    {
        if (std::getenv("KEEPER_FRONTEND_TRACE"))
        {
            static std::unordered_set<std::string> seenTextures;
            if (seenTextures.size() < 40 && seenTextures.insert(mDiffuseTexture->GetTextureName()).second)
                std::fprintf(stderr, "DK2MAT texture=%s format=%d dims=%d,%d loaded=%d default=%d\n",
                    mDiffuseTexture->GetTextureName().c_str(), static_cast<int>(mDiffuseTexture->GetPixelFormat()),
                    mDiffuseTexture->GetImageDimensions().x, mDiffuseTexture->GetImageDimensions().y,
                    mDiffuseTexture->IsLoadedFromFile() ? 1 : 0, mDiffuseTexture->IsDefaultTexture() ? 1 : 0);
        }
        mDiffuseTexture->BindTexture(eTextureUnit_DiffuseMap0);
    }
    shaderProgram.SetMaterialUniforms(*this);
}

void SurfaceMaterial::Clear()
{
    mDiffuseTexture = nullptr;
    mEnvMappingTexture = nullptr;

    mBaseColor = COLOR_WHITE;
    //mSpecularColor = COLOR_WHITE;
    //mAmbientColor = COLOR_WHITE;
    mEmissiveColor = COLOR_BLACK;

    mOpacity = 1.0f;
}

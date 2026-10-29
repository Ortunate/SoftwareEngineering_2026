// 本文件直接绘制调用方传入的三维网格，叠加收缩和透明度变换，不使用额外球壳。
#include "spawn_renderer.hpp"
#include "raymath.h"
#include "rlgl.h"
namespace review {
SpawnRenderer::SpawnRenderer(const std::filesystem::path&root){
 shader_=LoadShader((root/"tools/projectile_review/shaders/effects.vs").string().c_str(),(root/"tools/projectile_review/shaders/effects.fs").string().c_str());
 if(!IsShaderValid(shader_))throw std::runtime_error("Spawn shader load failed");
}
SpawnRenderer::~SpawnRenderer(){UnloadShader(shader_);}
EffectSample SpawnRenderer::Draw(const Model&model,const SpawnParams&p){
 auto sample=SampleEffect(p);
 if(model.meshCount<=0||!model.meshes||!model.materials||!model.meshMaterial)throw std::invalid_argument("Spawn model invalid");
 for(int i=0;i<model.meshCount;i++)if(model.meshMaterial[i]<0||model.meshMaterial[i]>=model.materialCount)throw std::invalid_argument("Spawn material index");
 if(!sample.visible)return sample;
 float tint[]{p.tint.r/255.F,p.tint.g/255.F,p.tint.b/255.F,p.tint.a/255.F};int mask=p.projectileMask;
 SetShaderValue(shader_,GetShaderLocation(shader_,"opacity"),&sample.alpha,SHADER_UNIFORM_FLOAT);
 SetShaderValue(shader_,GetShaderLocation(shader_,"tint"),tint,SHADER_UNIFORM_VEC4);
 SetShaderValue(shader_,GetShaderLocation(shader_,"projectileMask"),&mask,SHADER_UNIFORM_INT);
 Matrix transform=MatrixMultiply(MatrixMultiply(MatrixMultiply(model.transform,MatrixScale(sample.scale,sample.scale,sample.scale)),QuaternionToMatrix(p.rotation)),MatrixTranslate(p.position.x,p.position.y,p.position.z));
 BeginBlendMode(BLEND_ALPHA);rlDrawRenderBatchActive();rlDisableDepthMask();
 for(int i=0;i<model.meshCount;i++){Material material=model.materials[model.meshMaterial[i]];material.shader=shader_;DrawMesh(model.meshes[i],material,transform);}
 rlDrawRenderBatchActive();rlEnableDepthMask();EndBlendMode();return sample;
}
}

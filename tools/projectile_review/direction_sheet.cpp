// 本文件输出全部模型的本地方向灰模总览；仅变换预览副本，不改写资产。
#include "raylib.h"
#include "raymath.h"
#include "resources/projectile_catalog.hpp"
#include <filesystem>
#include <array>
#include <algorithm>
#include <string>
#include <iostream>

namespace {
// 箭身与箭头均为真实三维几何，长度使用当前格的归一化模型单位。
void Arrow(Vector3 origin,Vector3 direction,Color color,float length){
 DrawCylinderEx(origin,Vector3Add(origin,Vector3Scale(direction,length-.20F)),.014F,.014F,10,color);
 DrawCylinderEx(Vector3Add(origin,Vector3Scale(direction,length-.20F)),Vector3Add(origin,Vector3Scale(direction,length)),.065F,0,12,color);
}
}
// 参数为仓库根目录；输出 6×4 总览，正向来自正式模型目录，不依据文件名推断。
int main(int argc,char**argv){try{
 std::filesystem::path root=argc>1?argv[1]:".";
 SetConfigFlags(FLAG_WINDOW_HIDDEN|FLAG_MSAA_4X_HINT);InitWindow(720,540,"Model direction sheet");
 Shader shader=LoadShaderFromMemory(
 "#version 330\nin vec3 vertexPosition;in vec3 vertexNormal;uniform mat4 mvp;out vec3 n;void main(){n=vertexNormal;gl_Position=mvp*vec4(vertexPosition,1);}",
 "#version 330\nin vec3 n;out vec4 finalColor;void main(){float light=.32+.68*max(dot(normalize(n),normalize(vec3(-.3,.7,1))),0);finalColor=vec4(vec3(.62,.69,.78)*light,1);}");
 if(!IsShaderValid(shader))throw std::runtime_error("Direction shader failed");
 constexpr int W=600,H=440,HEADER=100;auto cell=LoadRenderTexture(W,H-75),sheet=LoadRenderTexture(W*6,H*4+HEADER);
 BeginTextureMode(sheet);ClearBackground({12,17,27,255});EndTextureMode();
 for(size_t index=0;index<koumakyou::ProjectileModels.size();index++){
  auto& definition=koumakyou::ProjectileModels[index];std::string name(definition.name);auto dir=root/"assets/projectiles"/name;auto file=dir/(name+".gltf");Model model=LoadModel(file.string().c_str());if(!model.meshCount)throw std::runtime_error("Cannot load "+name);
  bool capsule=name=="laser_default"||name=="laser_round"||name=="marisa_laser",spark=name=="bomb_spark";
  // 激光文件是参数模板；在副本上还原长束比例，不能直接把单位半径模板当成成品。
  if(capsule||spark)for(int m=0;m<model.meshCount;m++){auto& mesh=model.meshes[m];for(int v=0;v<mesh.vertexCount;v++){auto p=mesh.vertices+v*3;float y=p[1];float radius=spark?.15F+.38F*y:.14F;p[0]*=radius;p[2]*=radius;p[1]=capsule?(y<0?y*.14F:y>1?2.5F+(y-1)*.14F:y*2.5F):y*2.5F;}UpdateMeshBuffer(mesh,0,mesh.vertices,mesh.vertexCount*3*sizeof(float),0);}
  for(int m=0;m<model.materialCount;m++)model.materials[m].shader=shader;
  auto box=GetModelBoundingBox(model);Vector3 center=Vector3Scale(Vector3Add(box.min,box.max),.5F),extent=Vector3Subtract(box.max,box.min);float scale=1.6F/std::max({extent.x,extent.y,extent.z});Vector3 origin=Vector3Scale(center,-scale);
  Camera3D camera{{3.1F,2.2F,5.2F},{0,0,0},{0,1,0},3.6F,CAMERA_ORTHOGRAPHIC};
  BeginTextureMode(cell);ClearBackground({23,30,43,255});BeginMode3D(camera);DrawModel(model,origin,scale,WHITE);DrawSphere(origin,.035F,WHITE);
  float forwardLength=Vector3DotProduct(box.max,definition.direction.forward)*scale+.32F;
  float upLength=Vector3DotProduct(box.max,definition.direction.up)*scale+.32F;
  Arrow(origin,definition.direction.forward,GREEN,forwardLength);
  // 偏移参考上方向的箭身，避免与正向箭头共线时混在一起。
  Arrow(Vector3Add(origin,{.07F,0,0}),definition.direction.up,SKYBLUE,upLength);
  EndMode3D();EndTextureMode();
  int x=index%6*W,y=index/6*H+HEADER;BeginTextureMode(sheet);DrawRectangleLines(x,y,W,H,{58,70,88,255});DrawTextureRec(cell.texture,{0,0,float(W),float(-(H-75))},{float(x),float(y)},WHITE);DrawText(TextFormat("%02d  %s",int(index+1),name.c_str()),x+18,y+H-65,22,RAYWHITE);
  DrawText(name.ends_with("_option")?"FORWARD +Z | UP +Y | FACE +Z":"FORWARD +Y | UP +Z | FACE +Z",x+18,y+H-33,18,LIGHTGRAY);EndTextureMode();
  UnloadModel(model);
 }
 BeginTextureMode(sheet);DrawText("ALL 24 MODEL DIRECTIONS / LOCAL AXES AUDIT",25,18,32,RAYWHITE);DrawText("GREEN = FORWARD    BLUE = REFERENCE UP    WHITE DOT = MODEL ORIGIN    |    CLAY GEOMETRY / NO GLOW / NORMALIZED DISPLAY SIZE",25,65,20,LIGHTGRAY);EndTextureMode();
 BeginDrawing();ClearBackground(BLACK);EndDrawing();auto out=root/"output/projectile-review/directions";std::filesystem::create_directories(out);Image image=LoadImageFromTexture(sheet.texture);ImageFlipVertical(&image);if(!ExportImage(image,(out/"all-model-directions.png").string().c_str()))throw std::runtime_error("Cannot export sheet");UnloadImage(image);UnloadRenderTexture(cell);UnloadRenderTexture(sheet);UnloadShader(shader);CloseWindow();return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

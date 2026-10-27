// 本文件保存独立实例渲染改动的 OpenGL 状态，便于嵌入其他 raylib 渲染通道。
#pragma once
#include "external/glad.h"
namespace review {
struct GLState {
    GLint program,vao,arrayBuffer,srcRGB,dstRGB,srcAlpha,dstAlpha,eqRGB,eqAlpha,depthFunc,cullFace,frontFace;
    GLboolean depth,cull,blend,depthWrite;
    GLState(){
        glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&arrayBuffer);
        glGetIntegerv(GL_BLEND_SRC_RGB,&srcRGB);glGetIntegerv(GL_BLEND_DST_RGB,&dstRGB);glGetIntegerv(GL_BLEND_SRC_ALPHA,&srcAlpha);glGetIntegerv(GL_BLEND_DST_ALPHA,&dstAlpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB,&eqRGB);glGetIntegerv(GL_BLEND_EQUATION_ALPHA,&eqAlpha);glGetIntegerv(GL_DEPTH_FUNC,&depthFunc);
        glGetIntegerv(GL_CULL_FACE_MODE,&cullFace);glGetIntegerv(GL_FRONT_FACE,&frontFace);
        depth=glIsEnabled(GL_DEPTH_TEST);cull=glIsEnabled(GL_CULL_FACE);blend=glIsEnabled(GL_BLEND);glGetBooleanv(GL_DEPTH_WRITEMASK,&depthWrite);
    }
    void Restore()const{
        glUseProgram(program);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,arrayBuffer);
        glBlendFuncSeparate(srcRGB,dstRGB,srcAlpha,dstAlpha);glBlendEquationSeparate(eqRGB,eqAlpha);
        glDepthFunc(depthFunc);glDepthMask(depthWrite);glCullFace(cullFace);glFrontFace(frontFace);
        if(depth)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
        if(cull)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);
        if(blend)glEnable(GL_BLEND);else glDisable(GL_BLEND);
    }
    ~GLState(){Restore();}
};
}

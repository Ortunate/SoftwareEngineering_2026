// 本文件为独立压力实验提供预分配统计容器与无帧内阻塞的 GPU 时间查询。
#pragma once
#include "external/glad.h"
#include <vector>
#include <array>
#include <algorithm>
#include <numeric>
#include <ostream>
#include <stdexcept>
namespace review {
/// @brief 最多 250000 帧，构造预留空间；采样不分配，排序/汇总只在测量区间结束后调用。
struct PerformanceSamples {
    std::vector<double> values;
    PerformanceSamples(){values.reserve(250000);}
    void Add(double ms){if(values.size()==values.capacity())throw std::runtime_error("Sample capacity exceeded");values.push_back(ms);}
    void Write(std::ostream& out){if(values.empty()){out<<",NA,NA,NA,NA";return;}double mean=std::accumulate(values.begin(),values.end(),0.)/values.size();std::sort(values.begin(),values.end());out<<','<<mean;for(double q:{.5,.95,.99})out<<','<<values[std::min(values.size()-1,(size_t)(q*values.size()))];}
};
/// @brief 32 个异步查询槽。槽忙则跳过并计数；析构须在 GL 上下文关闭前完成。
struct AsyncGpuTimer {
    std::array<GLuint,32> ids{};std::array<bool,32> busy{},record{};int active=-1;bool supported=false;size_t skipped=0;
    AsyncGpuTimer(){supported=glGenQueries&&glGetQueryObjectui64v&&glBeginQuery;if(supported)glGenQueries(32,ids.data());}
    ~AsyncGpuTimer(){if(supported)glDeleteQueries(32,ids.data());}
    void Poll(PerformanceSamples& samples){if(!supported)return;for(int i=0;i<32;++i)if(busy[i]){GLint ready=0;glGetQueryObjectiv(ids[i],GL_QUERY_RESULT_AVAILABLE,&ready);if(ready){GLuint64 ns;glGetQueryObjectui64v(ids[i],GL_QUERY_RESULT,&ns);if(record[i])samples.Add(ns/1e6);busy[i]=false;}}}
    void Begin(bool measured){if(!supported)return;for(int i=0;i<32;++i)if(!busy[i]){active=i;record[i]=measured;glBeginQuery(GL_TIME_ELAPSED,ids[i]);return;}if(measured)++skipped;}
    void End(){if(active>=0){glEndQuery(GL_TIME_ELAPSED);busy[active]=true;active=-1;}}
};
} // namespace review

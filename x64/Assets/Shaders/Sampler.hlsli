#ifndef SAMPLER_COMMON
#define SAMPLER_COMMON

sampler LinearSampler : register(s0);
sampler PointSampler : register(s1);
sampler ClampSampler : register(s2);

sampler ShadowSampler : register(s2);
#endif

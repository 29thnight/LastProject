#pragma once
#include "SingletonBase.h"
enum EasingEffect
{
    // Linear
    Linear,

    // Sine
    InSine,
    OutSine,
    InOutSine,

    // Quad
    InQuad,
    OutQuad,
    InOutQuad,

    // Cubic
    InCubic,
    OutCubic,
    InOutCubic,

    // Quart
    InQuart,
    OutQuart,
    InOutQuart,

    // Quint
    InQuint,
    OutQuint,
    InOutQuint,

    // Expo
    InExpo,
    OutExpo,
    InOutExpo,

    // InCirc
    InCirc,
    OutCirc,
    InOutCirc,

    // Back
    InBack,
    OutBack,
    InOutBack,

    // Elastic
    InElastic,
    OutElastic,
    InOutElastic,

    // Bounce
    InBounce,
    OutBounce,
    InOutBounce,

    EasingEffectEnd
};
#pragma region GraphFunc
// Linear
float EaseLinear(float x);

// Sine
float EaseInSine(float x);
float EaseOutSine(float x);
float EaseInOutSine(float x);

// Quad
float EaseInQuad(float x);
float EaseOutQuad(float x);
float EaseInOutQuad(float x);

// Cubic
float EaseInCubic(float x);
float EaseOutCubic(float x);
float EaseInOutCubic(float x);

// Quart
float EaseInQuart(float x);
float EaseOutQuart(float x);
float EaseInOutQuart(float x);

// Quint
float EaseInQuint(float x);
float EaseOutQuint(float x);
float EaseInOutQuint(float x);

// Expo
float EaseInExpo(float x);
float EaseOutExpo(float x);
float EaseInOutExpo(float x);

// InCirc
float EaseInCirc(float x);
float EaseOutCirc(float x);
float EaseInOutCirc(float x);

// Back
float EaseInBack(float x);
float EaseOutBack(float x);
float EaseInOutBack(float x);

// Elastic
float EaseInElastic(float x);
float EaseOutElastic(float x);
float EaseInOutElastic(float x);

// Bounce
float EaseInBounce(float x);
float EaseOutBounce(float x);
float EaseInOutBounce(float x);
#pragma endregion GraphFunc
static std::function<float(float)> EasingFunction[EasingEffect::EasingEffectEnd] =
{
    // Linear
    EaseLinear,

    // Sine
    EaseInSine,
    EaseOutSine,
    EaseInOutSine,

    // Quad
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,

    // Cubic
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic,

    // Quart
    EaseInQuart,
    EaseOutQuart,
    EaseInOutQuart,

    // Quint
    EaseInQuint,
    EaseOutQuint,
    EaseInOutQuint,

    // Expo
    EaseInExpo,
    EaseOutExpo,
    EaseInOutExpo,

    // InCirc
    EaseInCirc,
    EaseOutCirc,
    EaseInOutCirc,

    // Back
    EaseInBack,
    EaseOutBack,
    EaseInOutBack,

    // Elastic
    EaseInElastic,
    EaseOutElastic,
    EaseInOutElastic,

    // Bounce
    EaseInBounce,
    EaseOutBounce,
    EaseInOutBounce,
};;

enum StepAnimation
{
    StepOnceForward,
    StepOnceBack,
    StepOncePingPong,
    StepLoopForward,
    StepLoopBack,
    StepLoopPingPong,
    StepAnimationEnd
};

class DOTween
{
public:
    DOTween(float& _Data, EasingEffect _EasingEffect, StepAnimation _StepAnimation = StepOnceForward, float duration = 1, float startpoint = 0, float endpoint = 1);
    DOTween(DXMath::Quaternion& _quater, EasingEffect _EasingEffect, StepAnimation _StepAnimation, float duration, DXMath::Quaternion startpoint, DXMath::Quaternion endpoint);
    ~DOTween();
    static int testCount;
    void   SetStartPoint(float _StartPoint) { StartPoint = _StartPoint; }
    void   SetEndPoint(float _EndPoint) { EndPoint = _EndPoint; }
    void   SetDuration(float   _Duration) { Duration = _Duration; }

    void Update(const float& _DeltaTime)
    {
        (this->*StepAnimationFunction[Type])(_DeltaTime);
    }

private:
 
    float& Data;         //   0 ~ 1(StartPoint ~ EndPoint)
    float                     StartPoint;
    float                     EndPoint;
    float                  Duration;      // N초 동안
    float                  CurTime = 0.f;
    std::function<float(float)>   Function;
    StepAnimation            Type;

private:
    void OnceForward(const float& _DeltaTime)
    {
        if (CurTime > Duration)
        {
            delete this; //시간이 다되면 소멸
            return;
        }

        float CurStepTime;
        float CurStep;

        CurTime += _DeltaTime;
        CurStepTime = CurTime / Duration;
        CurStep = EndPoint - StartPoint;

        Data = StartPoint + CurStep * Function(CurStepTime);
    }
    void OnceBack(const float& _DeltaTime)
    {
        if (CurTime > Duration)
        {
            delete this;
            return;
        }
        float CurStepTime;
        float CurStep;

        CurTime += _DeltaTime;
        CurStepTime = 1.f - (CurTime / Duration);
        CurStep = EndPoint - StartPoint;

        Data = StartPoint + CurStep * Function(CurStepTime);
    }
    void OncePingPong(const float& _DeltaTime)
    {
        if (CurTime > Duration)
        {
            delete this;
            return;
        }
        float CurStepTime;
        float CurStep;

        CurTime += _DeltaTime;
        CurStepTime = (CurTime < (Duration / 2))
            ? CurTime / (Duration / 2)
            : 1.f - ((CurTime - Duration / 2) / (Duration / 2));
        CurStep = EndPoint - StartPoint;

        Data = StartPoint + CurStep * Function(CurStepTime);
    }
    void LoopForward(const float& _DeltaTime)
    {
        if (CurTime > Duration)
            CurTime -= Duration;
        float CurStepTime;
        float CurStep;

        CurTime += _DeltaTime;
        CurStepTime = CurTime / Duration;
        CurStep = EndPoint - StartPoint;

        Data = StartPoint + CurStep * Function(CurStepTime);
    }
    void LoopBack(const float& _DeltaTime)
    {
        if (CurTime > Duration)
            CurTime -= Duration;
        float CurStepTime;
        float CurStep;

        CurTime += _DeltaTime;
        CurStepTime = 1.f - (CurTime / Duration);
        CurStep = EndPoint - StartPoint;

        Data = StartPoint + CurStep * Function(CurStepTime);
    }
    void LoopPingPong(const float& _DeltaTime)
    {
        if (CurTime > Duration)
            CurTime -= Duration;
        float CurStepTime;
        float CurStep;

        CurTime += _DeltaTime;
        CurStepTime = (CurTime < (Duration / 2))
            ? CurTime / (Duration / 2)
            : 1.f - ((CurTime - Duration / 2) / (Duration / 2));
        CurStep = EndPoint - StartPoint;

        Data = StartPoint + CurStep * Function(CurStepTime);
    }

    static void (DOTween::* StepAnimationFunction[StepAnimation::StepAnimationEnd])(const float&);
};




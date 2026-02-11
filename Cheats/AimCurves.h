#pragma once

#include <random>

#include "../Math/Vector.h"

namespace Cheats
{
	enum class AimCurveMode : int
	{
		SineArc = 0,
		ExpoDecay = 1,
		SmootherStep = 2,
		RandomPerTrigger = 3
	};

	struct AimCurveConfig
	{
		AimCurveMode mode = AimCurveMode::SineArc;
		float speed = 1.0f;
		float smoothing = 0.55f;
	};

	class AimCurveTracker
	{
	public:
		AimCurveTracker();

		void Reset();
		Vector Step(const Vector& targetOffset, int targetId, double deltaMs, bool triggerActive, const AimCurveConfig& config);

	private:
		static float Clamp01(float value);
		static float Length2D(const Vector& value);
		static Vector Normalize2D(const Vector& value);
		static Vector EvaluateSineArc(const Vector& targetOffset, float mappedT);
		static Vector EvaluateExpo(const Vector& targetOffset, float mappedT);
		static Vector EvaluateSmootherStep(const Vector& targetOffset, float mappedT);
		static float EaseOutExpo(float t);
		static float SmootherStep01(float t);

	private:
		std::mt19937 rng_{};
		bool active_ = false;
		int targetId_ = -1;
		float progress_ = 0.0f;
		Vector anchorOffset_{};
		Vector lastPoint_{};
		AimCurveMode activeMode_ = AimCurveMode::SineArc;
		bool triggerWasActive_ = false;
	};
}

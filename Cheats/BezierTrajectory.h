#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <random>

#include "../Math/Vector.h"

namespace Cheats
{
	struct DynamicBezierConfig
	{
		float minDurationMs = 10.0f;
		float maxDurationMs = 65.0f;
		float controlSpread = 16.0f;
		float targetShiftScaleC1 = 0.45f;
		float targetShiftScaleC2 = 0.85f;
		float catchUpGain = 1.0f;
	};

	class DynamicBezierTracker
	{
	public:
		DynamicBezierTracker();

		void Reset();
		Vector Step(const Vector& targetOffset, int targetId, double deltaMs, const DynamicBezierConfig& config);

	private:
		void StartCurve(const Vector& targetOffset, int targetId, const DynamicBezierConfig& config);
		void UpdateDynamicTarget(const Vector& targetOffset, const DynamicBezierConfig& config);
		float ComputeDurationMs(const Vector& targetOffset, const DynamicBezierConfig& config) const;
		Vector Evaluate(float t) const;

	private:
		std::mt19937 rng_{};
		bool active_ = false;
		int targetId_ = -1;
		Vector p0_{};
		Vector p1_{};
		Vector p2_{};
		Vector p3_{};
		float progress_ = 0.0f;
		float durationMs_ = 25.0f;
		Vector lastPoint_{};
	};
}

#include "BezierTrajectory.h"

#include <algorithm>
#include <cmath>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

namespace Cheats
{
	namespace
	{
		float Clamp01(const float value)
		{
			return (std::clamp)(value, 0.0f, 1.0f);
		}

		float Length2D(const Vector& value)
		{
			return std::sqrt(value.x * value.x + value.y * value.y);
		}

		Vector Lerp(const Vector& a, const Vector& b, const float t)
		{
			return a + (b - a) * Clamp01(t);
		}
	}

	DynamicBezierTracker::DynamicBezierTracker() : rng_(std::random_device{}())
	{
	}

	void DynamicBezierTracker::Reset()
	{
		active_ = false;
		targetId_ = -1;
		progress_ = 0.0f;
		durationMs_ = 25.0f;
		p0_ = {};
		p1_ = {};
		p2_ = {};
		p3_ = {};
		lastPoint_ = {};
	}

	Vector DynamicBezierTracker::Step(const Vector& targetOffset, const int targetId, const double deltaMs, const DynamicBezierConfig& config)
	{
		if (targetOffset.z <= 0.0f)
		{
			Reset();
			return {};
		}

		if (!active_ || targetId_ != targetId)
		{
			StartCurve(targetOffset, targetId, config);
		}

		UpdateDynamicTarget(targetOffset, config);

		const float safeDuration = (std::max)(config.minDurationMs, durationMs_);
		const float progressStep = static_cast<float>((std::max)(0.1, deltaMs)) / safeDuration;
		progress_ = (std::min)(1.0f, progress_ + progressStep);

		const Vector point = Evaluate(progress_);
		Vector delta = point - lastPoint_;
		lastPoint_ = point;

		delta = delta * (std::max)(0.1f, config.catchUpGain);

		if (progress_ >= 1.0f || Length2D(targetOffset) < 0.6f)
		{
			Reset();
		}

		return delta;
	}

	void DynamicBezierTracker::StartCurve(const Vector& targetOffset, const int targetId, const DynamicBezierConfig& config)
	{
		std::uniform_real_distribution<float> randomPhase(-1.0f, 1.0f);
		std::uniform_real_distribution<float> randomSpread(-config.controlSpread, config.controlSpread);

		const Vector start{};
		const Vector end{ targetOffset.x, targetOffset.y, 1.0f };
		const Vector baseDir = (end - start);

		const Vector normal{ -baseDir.y, baseDir.x, 0.0f };
		const float normalLen = (std::max)(1.0f, Length2D(normal));
		const Vector normalUnit{ normal.x / normalLen, normal.y / normalLen, 0.0f };

		const float c1Offset = randomSpread(rng_);
		const float c2Offset = randomSpread(rng_);

		p0_ = start;
		p1_ = Lerp(start, end, 0.32f) + normalUnit * c1Offset + Vector{ randomPhase(rng_) * 2.0f, randomPhase(rng_) * 2.0f, 0.0f };
		p2_ = Lerp(start, end, 0.74f) + normalUnit * c2Offset + Vector{ randomPhase(rng_) * 2.0f, randomPhase(rng_) * 2.0f, 0.0f };
		p3_ = end;

		durationMs_ = ComputeDurationMs(targetOffset, config);
		progress_ = 0.0f;
		targetId_ = targetId;
		active_ = true;
		lastPoint_ = p0_;
	}

	void DynamicBezierTracker::UpdateDynamicTarget(const Vector& targetOffset, const DynamicBezierConfig& config)
	{
		const Vector oldEnd = p3_;
		const Vector newEnd{ targetOffset.x, targetOffset.y, 1.0f };
		const Vector shift = newEnd - oldEnd;
		if (Length2D(shift) < 0.01f)
			return;

		const float t = Clamp01(progress_);
		const float smooth = t * t * (3.0f - 2.0f * t);

		p1_ = p1_ + shift * (config.targetShiftScaleC1 * (1.0f - smooth));
		p2_ = p2_ + shift * (config.targetShiftScaleC2 * (1.0f - smooth * 0.7f));
		p3_ = newEnd;

		durationMs_ = ComputeDurationMs(targetOffset, config);
	}

	float DynamicBezierTracker::ComputeDurationMs(const Vector& targetOffset, const DynamicBezierConfig& config) const
	{
		const float distance = Length2D(targetOffset);
		const float normalized = (std::clamp)(distance / 220.0f, 0.0f, 1.0f);
		const float duration = config.minDurationMs + (config.maxDurationMs - config.minDurationMs) * normalized;
		return (std::clamp)(duration, config.minDurationMs, config.maxDurationMs);
	}

	Vector DynamicBezierTracker::Evaluate(const float t) const
	{
		const float u = 1.0f - Clamp01(t);
		const float tt = t * t;
		const float uu = u * u;
		const float uuu = uu * u;
		const float ttt = tt * t;

		Vector point = p0_ * uuu;
		point = point + p1_ * (3.0f * uu * t);
		point = point + p2_ * (3.0f * u * tt);
		point = point + p3_ * ttt;
		point.z = 1.0f;
		return point;
	}
}

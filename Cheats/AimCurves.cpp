#include "AimCurves.h"

#include <algorithm>
#include <cmath>

namespace Cheats
{
	namespace
	{
		constexpr float kPi = 3.14159265358979323846f;
	}

	AimCurveTracker::AimCurveTracker() : rng_(std::random_device{}())
	{
	}

	void AimCurveTracker::Reset()
	{
		active_ = false;
		targetId_ = -1;
		progress_ = 0.0f;
		anchorOffset_ = {};
		lastPoint_ = {};
		triggerWasActive_ = false;
	}

	Vector AimCurveTracker::Step(const Vector& targetOffset, const int targetId, const double deltaMs, const bool triggerActive, const AimCurveConfig& config)
	{
		if (targetOffset.z <= 0.0f)
		{
			Reset();
			return {};
		}

		const float safeSpeed = std::clamp(config.speed, 0.4f, 2.5f);
		const float safeSmoothing = std::clamp(config.smoothing, 0.05f, 0.95f);

		const bool targetChanged = (!active_ || targetId_ != targetId);
		const bool triggerEdge = (!triggerWasActive_ && triggerActive);
		if (targetChanged || triggerEdge)
		{
			progress_ = 0.0f;
			lastPoint_ = {};
			anchorOffset_ = targetOffset;
			targetId_ = targetId;
			active_ = true;

			if (config.mode == AimCurveMode::RandomPerTrigger)
			{
				std::uniform_int_distribution<int> dist(0, 2);
				activeMode_ = static_cast<AimCurveMode>(dist(rng_));
			}
			else
			{
				activeMode_ = config.mode;
			}
		}

		triggerWasActive_ = triggerActive;

		const float distance = Length2D(anchorOffset_);
		const float baseMs = std::clamp(34.0f - safeSpeed * 10.0f, 8.0f, 40.0f);
		const float distanceBoost = std::clamp(distance / 260.0f, 0.0f, 1.0f) * 10.0f;
		const float durationMs = baseMs + distanceBoost;

		const float progressStep = static_cast<float>(std::max(0.1, deltaMs)) / durationMs;
		progress_ = std::min(1.0f, progress_ + progressStep);

		const float mappedT = Clamp01(progress_);
		Vector point{};
		switch (activeMode_)
		{
		case AimCurveMode::SineArc:
			point = EvaluateSineArc(anchorOffset_, mappedT);
			break;
		case AimCurveMode::ExpoDecay:
			point = EvaluateExpo(anchorOffset_, mappedT);
			break;
		case AimCurveMode::SmootherStep:
			point = EvaluateSmootherStep(anchorOffset_, mappedT);
			break;
		default:
			point = EvaluateSineArc(anchorOffset_, mappedT);
			break;
		}

		Vector delta = point - lastPoint_;
		const Vector correction = (targetOffset - anchorOffset_) * 0.14f;
		delta = delta + correction;
		lastPoint_ = point;

		delta = delta * (1.0f - safeSmoothing * 0.35f);

		if (progress_ >= 1.0f || Length2D(targetOffset) < 0.55f)
		{
			active_ = false;
			progress_ = 0.0f;
			anchorOffset_ = {};
			lastPoint_ = {};
		}

		return delta;
	}

	float AimCurveTracker::Clamp01(const float value)
	{
		return std::clamp(value, 0.0f, 1.0f);
	}

	float AimCurveTracker::Length2D(const Vector& value)
	{
		return std::sqrt(value.x * value.x + value.y * value.y);
	}

	Vector AimCurveTracker::Normalize2D(const Vector& value)
	{
		const float len = Length2D(value);
		if (len <= 0.0001f)
			return { 0.0f, 0.0f, 0.0f };
		return { value.x / len, value.y / len, 0.0f };
	}

	float AimCurveTracker::EaseOutExpo(const float t)
	{
		if (t >= 1.0f)
			return 1.0f;
		return 1.0f - std::pow(2.0f, -10.0f * Clamp01(t));
	}

	float AimCurveTracker::SmootherStep01(const float t)
	{
		const float x = Clamp01(t);
		return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
	}

	Vector AimCurveTracker::EvaluateSineArc(const Vector& targetOffset, const float mappedT)
	{
		const Vector linear = targetOffset * mappedT;
		const Vector direction = Normalize2D(targetOffset);
		const Vector perp{ -direction.y, direction.x, 0.0f };

		const float distance = Length2D(targetOffset);
		const float arcHeight = std::clamp(distance / 20.0f, 2.0f, 45.0f);
		const float arcOffset = std::sin(mappedT * kPi) * arcHeight;

		return { linear.x + perp.x * arcOffset, linear.y + perp.y * arcOffset, 1.0f };
	}

	Vector AimCurveTracker::EvaluateExpo(const Vector& targetOffset, const float mappedT)
	{
		const float eased = EaseOutExpo(mappedT);
		return { targetOffset.x * eased, targetOffset.y * eased, 1.0f };
	}

	Vector AimCurveTracker::EvaluateSmootherStep(const Vector& targetOffset, const float mappedT)
	{
		const float eased = SmootherStep01(mappedT);
		const Vector linear = targetOffset * eased;

		const Vector direction = Normalize2D(targetOffset);
		const Vector perp{ -direction.y, direction.x, 0.0f };

		const float distance = Length2D(targetOffset);
		const float amp = std::clamp(distance / 85.0f, 1.0f, 7.0f);
		const float distortion = std::sin(mappedT * 2.0f * kPi) * amp;

		return { linear.x + perp.x * distortion, linear.y + perp.y * distortion, 1.0f };
	}
}

#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace Cheats
{
	enum class InputBackend : int
	{
		WinAPI = 0,
		KmboxNet = 1,
		KmboxBPro = 2
	};

	struct MouseConnectParams
	{
		InputBackend backend = InputBackend::WinAPI;
		std::string endpoint{};
		std::string uuid{};
	};

	class IMouseController
	{
	public:
		virtual ~IMouseController() = default;
		virtual bool Initialize(const MouseConnectParams& params, std::string& error) = 0;
		virtual bool MoveRelative(int dx, int dy) = 0;
		virtual bool LeftDown() = 0;
		virtual bool LeftUp() = 0;
		virtual void Close() = 0;
		virtual const char* Name() const = 0;
		virtual std::string LastError() const = 0;
	};

	class MouseControllerService
	{
	public:
		MouseControllerService();
		~MouseControllerService();

		bool Connect(const MouseConnectParams& params, std::string& message);

		bool MoveRelative(int dx, int dy);
		bool LeftDown();
		bool LeftUp();
		bool LeftDownImmediate();
		bool LeftUpImmediate();
		bool TriggerClickDebug(std::string& detail);

		InputBackend ActiveBackend() const;
		std::string ActiveName() const;
		std::string ConsumeWarning();

	private:
		enum class CommandType
		{
			Move,
			LeftDown,
			LeftUp
		};

		struct QueuedCommand
		{
			CommandType type = CommandType::Move;
			int dx = 0;
			int dy = 0;
		};

		bool EnqueueCommand(const QueuedCommand& command);
		void ClearQueue();
		void ConsumerLoop();

		bool ExecuteMoveLocked(int dx, int dy);
		bool ExecuteButtonLocked(bool down);
		std::unique_ptr<IMouseController> CreateController(InputBackend backend) const;

	private:
		mutable std::mutex mutex_{};
		std::unique_ptr<IMouseController> controller_{};
		InputBackend activeBackend_ = InputBackend::WinAPI;
		std::string pendingWarning_{};

		mutable std::mutex queueMutex_{};
		std::condition_variable queueCv_{};
		std::deque<QueuedCommand> queue_{};
		std::thread consumerThread_{};
		std::atomic<bool> running_{ false };
	};
}

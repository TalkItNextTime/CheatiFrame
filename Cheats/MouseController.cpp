#include "MouseController.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <limits>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>

#include "../KmBox/kmboxNet.h"
#include "../KmBox/KmboxB.h"

namespace Cheats
{
	namespace
	{
		std::string TrimAscii(std::string value)
		{
			auto isWs = [](unsigned char ch) { return std::isspace(ch) != 0; };
			value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char ch) { return !isWs(static_cast<unsigned char>(ch)); }));
			value.erase(std::find_if(value.rbegin(), value.rend(), [&](char ch) { return !isWs(static_cast<unsigned char>(ch)); }).base(), value.end());
			return value;
		}

		bool ParseIpPort(const std::string& endpoint, std::string& outIp, std::string& outPort)
		{
			std::string value = TrimAscii(endpoint);
			const size_t sep = value.find(':');
			if (sep == std::string::npos)
				return false;
			outIp = TrimAscii(value.substr(0, sep));
			outPort = TrimAscii(value.substr(sep + 1));
			return !outIp.empty() && !outPort.empty();
		}

		int ParseComNumber(const std::string& endpoint)
		{
			std::string value = TrimAscii(endpoint);
			std::string digits{};
			for (char ch : value)
			{
				if (std::isdigit(static_cast<unsigned char>(ch)))
					digits.push_back(ch);
			}
			if (digits.empty())
				return -1;
			return std::atoi(digits.c_str());
		}

		class WinApiMouseController final : public IMouseController
		{
		public:
			bool Initialize(const MouseConnectParams&, std::string&) override
			{
				lastError_.clear();
				return true;
			}

			bool MoveRelative(int dx, int dy) override
			{
				if (dx == 0 && dy == 0)
					return true;

				const DWORD data = 0;
				const ULONG_PTR extra = 0;
				mouse_event(MOUSEEVENTF_MOVE, dx, dy, data, extra);
				return true;
			}

			bool LeftDown() override
			{
				mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
				return true;
			}

			bool LeftUp() override
			{
				mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
				return true;
			}

			void Close() override
			{
				lastError_.clear();
			}

			const char* Name() const override
			{
				return "WinAPI";
			}

			std::string LastError() const override
			{
				return lastError_;
			}

		private:
			std::string lastError_{};
		};

		class KmboxMouseController final : public IMouseController
		{
		public:
			KmboxMouseController() = default;

			bool ReconnectNet()
			{
				if (netIp_.empty() || netPort_.empty() || netUuid_.empty())
					return false;

				const int ret = kmNet_init(const_cast<char*>(netIp_.c_str()), const_cast<char*>(netPort_.c_str()), const_cast<char*>(netUuid_.c_str()));
				if (ret != success)
				{
					char buf[128]{};
					sprintf_s(buf, "kmNet reinit failed (%d)", ret);
					lastError_ = buf;
					netConnected_ = false;
					return false;
				}

				netConnected_ = true;
				lastError_.clear();
				return true;
			}

			bool Initialize(const MouseConnectParams& params, std::string& error) override
			{
				Close();
				backend_ = params.backend;

				if (backend_ == InputBackend::KmboxNet)
				{
					std::string ip{};
					std::string port{};
					if (!ParseIpPort(params.endpoint, ip, port))
					{
						error = "Kmbox Net endpoint must be in IP:Port format";
						lastError_ = error;
						return false;
					}

					const std::string uuid = TrimAscii(params.uuid);
					if (uuid.empty())
					{
						error = "Kmbox Net UUID is required";
						lastError_ = error;
						return false;
					}

					netIp_ = ip;
					netPort_ = port;
					netUuid_ = uuid;

					const int ret = kmNet_init(const_cast<char*>(netIp_.c_str()), const_cast<char*>(netPort_.c_str()), const_cast<char*>(netUuid_.c_str()));
					if (ret != success)
					{
						char buf[128]{};
						sprintf_s(buf, "kmNet_init failed (%d)", ret);
						error = buf;
						lastError_ = error;
						return false;
					}

					netConnected_ = true;
					lastError_.clear();
					return true;
				}

				if (backend_ == InputBackend::KmboxBPro)
				{
					const int comPort = ParseComNumber(params.endpoint);
					if (comPort <= 0)
					{
						error = "Kmbox B+ Pro COM port is invalid";
						lastError_ = error;
						return false;
					}

					if (!serial_.open(comPort, 115200))
					{
						error = "Failed to open KMbox B+ Pro serial port";
						lastError_ = error;
						return false;
					}

					bproConnected_ = true;
					lastError_.clear();
					return true;
				}

				error = "Unsupported KMbox backend";
				lastError_ = error;
				return false;
			}

			bool MoveRelative(int dx, int dy) override
			{
				if (dx == 0 && dy == 0)
					return true;

				if (backend_ == InputBackend::KmboxNet)
				{
					const int ret = kmNet_mouse_move(static_cast<short>(dx), static_cast<short>(dy));
					if (ret != success)
					{
						char buf[128]{};
						sprintf_s(buf, "kmNet_mouse_move failed (%d)", ret);
						lastError_ = buf;
						return false;
					}
					return true;
				}

				if (backend_ == InputBackend::KmboxBPro)
				{
					if (!bproConnected_ || !serial_.is_open())
					{
						lastError_ = "B+ Pro serial port is not open";
						return false;
					}

					char cmd[128]{};
					sprintf_s(cmd, "km.move(%d,%d)\r\n", dx, dy);
					if (serial_.write(cmd) <= 0)
					{
						lastError_ = "B+ Pro move command failed";
						return false;
					}
					return true;
				}

				lastError_ = "Kmbox backend is not initialized";
				return false;
			}

			bool LeftDown() override
			{
				if (backend_ == InputBackend::KmboxNet)
				{
					int ret = kmNet_mouse_left(1);
					if (ret == success)
						return true;

					if (ret == err_net_pts)
					{
						if (!ReconnectNet())
							return false;
						::Sleep(1);
						ret = kmNet_mouse_left(1);
						if (ret == success)
							return true;

						const int encRet = kmNet_enc_mouse_left(1);
						if (encRet == success)
						{
							lastError_.clear();
							return true;
						}
					}

					char buf[160]{};
					sprintf_s(buf, "kmNet_mouse_left down failed (%d)", ret);
					lastError_ = buf;
					return false;
				}

				if (backend_ == InputBackend::KmboxBPro)
				{
					if (!bproConnected_ || !serial_.is_open())
					{
						lastError_ = "B+ Pro serial port is not open";
						return false;
					}

					char cmd[] = "km.left(1)\r\n";
					if (serial_.write(cmd) <= 0)
					{
						lastError_ = "B+ Pro left down command failed";
						return false;
					}
					return true;
				}

				lastError_ = "Kmbox backend is not initialized";
				return false;
			}

			bool LeftUp() override
			{
				if (backend_ == InputBackend::KmboxNet)
				{
					int ret = kmNet_mouse_left(0);
					if (ret == success)
						return true;

					if (ret == err_net_pts)
					{
						if (!ReconnectNet())
							return false;
						::Sleep(1);
						ret = kmNet_mouse_left(0);
						if (ret == success)
							return true;

						const int encRet = kmNet_enc_mouse_left(0);
						if (encRet == success)
						{
							lastError_.clear();
							return true;
						}
					}

					char buf[160]{};
					sprintf_s(buf, "kmNet_mouse_left up failed (%d)", ret);
					lastError_ = buf;
					return false;
				}

				if (backend_ == InputBackend::KmboxBPro)
				{
					if (!bproConnected_ || !serial_.is_open())
					{
						lastError_ = "B+ Pro serial port is not open";
						return false;
					}

					char cmd[] = "km.left(0)\r\n";
					if (serial_.write(cmd) <= 0)
					{
						lastError_ = "B+ Pro left up command failed";
						return false;
					}
					return true;
				}

				lastError_ = "Kmbox backend is not initialized";
				return false;
			}

			void Close() override
			{
				if (bproConnected_ && serial_.is_open())
					serial_.close();

				if (netConnected_)
				{
					if (sockClientfd > 0)
					{
						closesocket(sockClientfd);
						WSACleanup();
						sockClientfd = -1;
					}
					netConnected_ = false;
				}

				bproConnected_ = false;
				backend_ = InputBackend::WinAPI;
				netIp_.clear();
				netPort_.clear();
				netUuid_.clear();
			}

			const char* Name() const override
			{
				if (backend_ == InputBackend::KmboxNet)
					return "Kmbox Net";
				if (backend_ == InputBackend::KmboxBPro)
					return "Kmbox B+ Pro";
				return "Kmbox";
			}

			std::string LastError() const override
			{
				return lastError_;
			}

		private:
			InputBackend backend_ = InputBackend::WinAPI;
			bool netConnected_ = false;
			bool bproConnected_ = false;
			_com serial_{};
			std::string lastError_{};
			std::string netIp_{};
			std::string netPort_{};
			std::string netUuid_{};
		};
	}

	MouseControllerService::MouseControllerService()
	{
		std::string error{};
		MouseConnectParams params{};
		params.backend = InputBackend::WinAPI;
		controller_ = CreateController(InputBackend::WinAPI);
		if (controller_)
			controller_->Initialize(params, error);
		activeBackend_ = InputBackend::WinAPI;
		pendingWarning_.clear();

		running_.store(true);
		consumerThread_ = std::thread(&MouseControllerService::ConsumerLoop, this);
	}

	MouseControllerService::~MouseControllerService()
	{
		running_.store(false);
		queueCv_.notify_all();
		if (consumerThread_.joinable())
			consumerThread_.join();

		{
			std::scoped_lock lock(mutex_);
			if (controller_)
				controller_->Close();
		}

		ClearQueue();
	}

	bool MouseControllerService::Connect(const MouseConnectParams& params, std::string& message)
	{
		std::scoped_lock lock(mutex_);

		auto next = CreateController(params.backend);
		if (!next)
		{
			message = "Failed to create input controller";
			return false;
		}

		std::string error{};
		if (!next->Initialize(params, error))
		{
			message = error.empty() ? "Input initialization failed" : error;
			return false;
		}

		if (controller_)
			controller_->Close();

		controller_ = std::move(next);
		activeBackend_ = params.backend;
		pendingWarning_.clear();
		ClearQueue();

		message = std::string("Connected: ") + controller_->Name();
		return true;
	}

	bool MouseControllerService::MoveRelative(int dx, int dy)
	{
		QueuedCommand command{};
		command.type = CommandType::Move;
		command.dx = dx;
		command.dy = dy;
		return EnqueueCommand(command);
	}

	bool MouseControllerService::LeftDown()
	{
		QueuedCommand command{};
		command.type = CommandType::LeftDown;
		return EnqueueCommand(command);
	}

	bool MouseControllerService::LeftUp()
	{
		QueuedCommand command{};
		command.type = CommandType::LeftUp;
		return EnqueueCommand(command);
	}

	bool MouseControllerService::LeftDownImmediate()
	{
		std::scoped_lock inputLock(mutex_);
		return ExecuteButtonLocked(true);
	}

	bool MouseControllerService::LeftUpImmediate()
	{
		std::scoped_lock inputLock(mutex_);
		return ExecuteButtonLocked(false);
	}

	bool MouseControllerService::TriggerClickDebug(std::string& detail)
	{
		std::scoped_lock inputLock(mutex_);
		if (!controller_)
		{
			detail = "controller is null";
			return false;
		}

		const std::string backendName = controller_->Name();
		const bool downOk = ExecuteButtonLocked(true);
		const std::string downErr = controller_->LastError();
		std::this_thread::sleep_for(std::chrono::milliseconds(12));
		const bool upOk = ExecuteButtonLocked(false);
		const std::string upErr = controller_->LastError();

		if (downOk && upOk)
		{
			detail = std::string("trigger ok on ") + backendName;
			return true;
		}

		detail = std::string("trigger failed on ") + backendName + ", down=" + (downOk ? "ok" : "fail") + ", up=" + (upOk ? "ok" : "fail");
		if (!downOk && !downErr.empty())
			detail += std::string(", down_err=") + downErr;
		if (!upOk && !upErr.empty())
			detail += std::string(", up_err=") + upErr;
		return false;
	}

	InputBackend MouseControllerService::ActiveBackend() const
	{
		std::scoped_lock lock(mutex_);
		return activeBackend_;
	}

	std::string MouseControllerService::ActiveName() const
	{
		std::scoped_lock lock(mutex_);
		return controller_ ? std::string(controller_->Name()) : std::string("WinAPI");
	}

	std::string MouseControllerService::ConsumeWarning()
	{
		std::scoped_lock lock(mutex_);
		std::string out = pendingWarning_;
		pendingWarning_.clear();
		return out;
	}

	bool MouseControllerService::EnqueueCommand(const QueuedCommand& command)
	{
		if (!running_.load())
			return false;

		{
			std::scoped_lock lock(queueMutex_);
			if (command.type == CommandType::Move)
			{
				if (!queue_.empty() && queue_.back().type == CommandType::Move)
				{
					long long mergedDx = static_cast<long long>(queue_.back().dx) + static_cast<long long>(command.dx);
					long long mergedDy = static_cast<long long>(queue_.back().dy) + static_cast<long long>(command.dy);
					mergedDx = std::clamp(mergedDx,
						static_cast<long long>(std::numeric_limits<int>::min()),
						static_cast<long long>(std::numeric_limits<int>::max()));
					mergedDy = std::clamp(mergedDy,
						static_cast<long long>(std::numeric_limits<int>::min()),
						static_cast<long long>(std::numeric_limits<int>::max()));
					queue_.back().dx = static_cast<int>(mergedDx);
					queue_.back().dy = static_cast<int>(mergedDy);
					return true;
				}

				if (command.dx == 0 && command.dy == 0)
					return true;
			}

			constexpr size_t kMaxQueueSize = 1024;
			if (queue_.size() >= kMaxQueueSize)
			{
				queue_.pop_front();
			}
			queue_.push_back(command);
		}

		queueCv_.notify_one();
		return true;
	}

	void MouseControllerService::ClearQueue()
	{
		std::scoped_lock lock(queueMutex_);
		queue_.clear();
	}

	void MouseControllerService::ConsumerLoop()
	{
		while (running_.load())
		{
			QueuedCommand command{};
			bool hasCommand = false;

			{
				std::unique_lock lock(queueMutex_);
				queueCv_.wait(lock, [&] { return !running_.load() || !queue_.empty(); });
				if (!running_.load())
					break;
				if (!queue_.empty())
				{
					command = queue_.front();
					queue_.pop_front();
					hasCommand = true;
				}
			}

			if (!hasCommand)
				continue;

			std::scoped_lock inputLock(mutex_);
			switch (command.type)
			{
			case CommandType::Move:
				ExecuteMoveLocked(command.dx, command.dy);
				break;
			case CommandType::LeftDown:
				ExecuteButtonLocked(true);
				break;
			case CommandType::LeftUp:
				ExecuteButtonLocked(false);
				break;
			default:
				break;
			}
		}
	}

	bool MouseControllerService::ExecuteMoveLocked(int dx, int dy)
	{
		if (!controller_)
			return false;

		if (!controller_->MoveRelative(dx, dy))
		{
			pendingWarning_ = controller_->LastError().empty() ? "Mouse move failed" : controller_->LastError();
			return false;
		}

		return true;
	}

	bool MouseControllerService::ExecuteButtonLocked(bool down)
	{
		if (!controller_)
			return false;

		const bool ok = down ? controller_->LeftDown() : controller_->LeftUp();
		if (!ok)
		{
			pendingWarning_ = controller_->LastError().empty() ? "Mouse click failed" : controller_->LastError();
			return false;
		}

		return true;
	}

	std::unique_ptr<IMouseController> MouseControllerService::CreateController(InputBackend backend) const
	{
		if (backend == InputBackend::WinAPI)
			return std::make_unique<WinApiMouseController>();
		if (backend == InputBackend::KmboxNet || backend == InputBackend::KmboxBPro)
			return std::make_unique<KmboxMouseController>();
		return nullptr;
	}
}

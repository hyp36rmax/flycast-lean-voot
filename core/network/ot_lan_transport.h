#pragma once

// Cross-PC Project OT native link. The receive worker owns no guest state.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace otlan
{
using Clock = std::chrono::steady_clock;
using Bytes = std::vector<uint8_t>;
using Session = std::array<uint8_t, 16>;

constexpr size_t Header = 40;
constexpr size_t Chunk = 1152;
constexpr size_t MaxMessage = 16388;
constexpr size_t QueueLimit = 32;
constexpr size_t AssemblyLimit = 16;
constexpr auto AssemblyLifetime = std::chrono::milliseconds(350);
constexpr auto HeartbeatInterval = std::chrono::milliseconds(250);
constexpr auto PeerTimeout = std::chrono::seconds(5);
constexpr uint8_t Version = 1;
constexpr uint32_t Magic = 0x4f544c4e; // OTLN, network byte order.

inline uint64_t read(const uint8_t* bytes, size_t size)
{
	uint64_t result = 0;
	for (size_t i = 0; i < size; ++i)
		result = (result << 8) | bytes[i];
	return result;
}

inline void write(uint8_t* bytes, size_t size, uint64_t value)
{
	while (size != 0)
	{
		bytes[--size] = static_cast<uint8_t>(value);
		value >>= 8;
	}
}

inline uint32_t crc(const uint8_t* data, size_t length, uint32_t current = 0xffffffff)
{
	static constexpr auto table = [] {
		std::array<uint32_t, 256> result{};
		for (size_t i = 0; i < result.size(); ++i)
		{
			uint32_t value = static_cast<uint32_t>(i);
			for (int bit = 0; bit < 8; ++bit)
				value = (value >> 1) ^ ((value & 1) ? 0xedb88320 : 0);
			result[i] = value;
		}
		return result;
	}();
	while (length-- != 0)
		current = table[(current ^ *data++) & 0xff] ^ (current >> 8);
	return current;
}

inline size_t fragmentCount(size_t length) { return (length + Chunk - 1) / Chunk; }

inline Bytes frame(const Session& session, uint64_t sequence, const uint8_t* data, size_t length, size_t index)
{
	const size_t amount = length == 0 ? 0 : std::min(Chunk, length - index * Chunk);
	Bytes result(Header + amount);
	write(result.data(), 4, Magic);
	result[4] = Version;
	result[5] = static_cast<uint8_t>(index);
	result[6] = static_cast<uint8_t>(fragmentCount(length));
	std::copy(session.begin(), session.end(), result.begin() + 8);
	write(result.data() + 24, 8, sequence);
	write(result.data() + 32, 2, length);
	write(result.data() + 34, 2, amount);
	if (amount != 0)
		std::memcpy(result.data() + Header, data + index * Chunk, amount);
	write(result.data() + 36, 4, crc(result.data() + Header, amount, crc(result.data(), 36)) ^ 0xffffffff);
	return result;
}

inline sockaddr_in endpoint(const std::string& ip, int port)
{
	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_port = htons(static_cast<u_short>(port));
	if (port < 1024 || port > 65535 || InetPtonA(AF_INET, ip.c_str(), &address.sin_addr) != 1
			|| address.sin_addr.s_addr == INADDR_ANY || address.sin_addr.s_addr == INADDR_BROADCAST
			|| (ntohl(address.sin_addr.s_addr) >> 28) >= 14)
		throw std::invalid_argument("Invalid LAN endpoint");
	return address;
}

inline bool sameEndpoint(const sockaddr_in& a, const sockaddr_in& b)
{
	return a.sin_family == b.sin_family && a.sin_port == b.sin_port && a.sin_addr.s_addr == b.sin_addr.s_addr;
}

inline Session parseSession(const char* hex)
{
	Session result{};
	if (hex == nullptr || std::strlen(hex) != result.size() * 2)
		throw std::invalid_argument("Missing LAN reservation");
	const auto digit = [](char ch) {
		if (ch >= '0' && ch <= '9') return ch - '0';
		if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
		if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
		throw std::invalid_argument("Invalid LAN reservation");
	};
	for (size_t i = 0; i < result.size(); ++i)
		result[i] = static_cast<uint8_t>((digit(hex[i * 2]) << 4) | digit(hex[i * 2 + 1]));
	if (std::all_of(result.begin(), result.end(), [](uint8_t b) { return b == 0; }))
		throw std::invalid_argument("Empty LAN reservation");
	return result;
}

struct Stats
{
	uint64_t sent = 0, datagrams = 0, assembled = 0, delivered = 0;
	uint64_t invalid = 0, foreign = 0, stale = 0, duplicate = 0, expired = 0, evicted = 0;
	uint64_t overflow = 0, queuePeak = 0, resets = 0, assemblyMicros = 0, assemblyMaxMicros = 0;
	uint64_t queueMicros = 0, queueMaxMicros = 0;
};

class Transport
{
public:
	Transport(sockaddr_in local, sockaddr_in peer, Session session, std::string log = {}, std::string failure = {})
		: local(local), peer(peer), session(session), log(std::move(log)), failure(std::move(failure))
	{
		if (sameEndpoint(local, peer))
			throw std::invalid_argument("LAN endpoints must differ");
		try
		{
			socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (socketHandle == INVALID_SOCKET)
				throw socketError("socket");
			const BOOL enabled = TRUE;
			if (setsockopt(socketHandle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
					reinterpret_cast<const char*>(&enabled), sizeof(enabled)) != 0)
				throw socketError("exclusive bind option");
			if (setsockopt(socketHandle, IPPROTO_IP, IP_DONTFRAGMENT,
					reinterpret_cast<const char*>(&enabled), sizeof(enabled)) != 0)
				throw socketError("do not fragment option");
			const int bufferSize = 256 * 1024;
			if (setsockopt(socketHandle, SOL_SOCKET, SO_RCVBUF,
					reinterpret_cast<const char*>(&bufferSize), sizeof(bufferSize)) != 0)
				throw socketError("receive buffer option");
			if (::bind(socketHandle, reinterpret_cast<const sockaddr*>(&local), sizeof(local)) != 0)
				throw socketError("bind");
			BOOL reportIcmpReset = FALSE;
			DWORD returned = 0;
			if (WSAIoctl(socketHandle, SIO_UDP_CONNRESET, &reportIcmpReset, sizeof(reportIcmpReset),
					nullptr, 0, &returned, nullptr, nullptr) != 0)
				throw socketError("ICMP reset option");
			u_long nonblocking = 1;
			if (ioctlsocket(socketHandle, FIONBIO, &nonblocking) != 0)
				throw socketError("nonblocking option");
			WriteSummary("Bound");
			worker = std::thread([this] { Run(); });
		}
		catch (...)
		{
			if (socketHandle != INVALID_SOCKET)
				closesocket(socketHandle);
			throw;
		}
	}

	~Transport() { Stop(); }
	Transport(const Transport&) = delete;
	Transport& operator=(const Transport&) = delete;

	const sockaddr_in& Peer() const { return peer; }

	void Send(const sockaddr_in& target, const void* data, size_t length)
	{
		if (!sameEndpoint(target, peer) || length < 2 || length > MaxMessage)
			throw std::invalid_argument("Unexpected LAN message endpoint or size");
		std::lock_guard<std::mutex> lock(sendGate);
		if (stopping.load() || failed.load())
			throw std::runtime_error("LAN transport is no longer running");
		const auto sequence = ++sentSequence;
		try
		{
			for (size_t i = 0; i < fragmentCount(length); ++i)
				SendFrame(frame(session, sequence, static_cast<const uint8_t*>(data), length, i));
		}
		catch (const std::exception& error)
		{
			Fail(error.what());
			throw;
		}
		std::lock_guard<std::mutex> guard(gate);
		++stats.sent;
	}

	bool Pop(Bytes& bytes)
	{
		std::lock_guard<std::mutex> lock(gate);
		if (queue.empty())
			return false;
		const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - queue.front().assembledAt).count();
		const auto micros = static_cast<uint64_t>(elapsed);
		stats.queueMicros += micros;
		stats.queueMaxMicros = std::max(stats.queueMaxMicros, micros);
		bytes = std::move(queue.front().bytes);
		queue.pop_front();
		++stats.delivered;
		return true;
	}

	bool Wait(std::chrono::microseconds timeout)
	{
		std::unique_lock<std::mutex> lock(gate);
		ready.wait_for(lock, timeout, [this] { return !queue.empty() || stopping.load() || failed.load(); });
		if (failed.load())
			throw std::runtime_error("LAN transport worker stopped");
		return !queue.empty();
	}

	void ResetBoard()
	{
		std::lock_guard<std::mutex> lock(gate);
		// A board reset does not touch the port or restart the transport sequence.
		lastComplete = highestObserved;
		queue.clear();
		assemblies.clear();
		++stats.resets;
	}

	Stats Snapshot() const
	{
		std::lock_guard<std::mutex> lock(gate);
		return stats;
	}

	bool Failed() const { return failed.load(); }

	void Stop()
	{
		std::lock_guard<std::mutex> stopLock(stopGate);
		if (stopping.exchange(true))
			return;
		ready.notify_all();
		if (worker.joinable())
			worker.join();
		{
			std::lock_guard<std::mutex> lock(sendGate);
			closesocket(socketHandle);
		}
		WriteSummary("Stopped");
	}

private:
	struct Assembly
	{
		Clock::time_point created;
		Bytes bytes;
		uint32_t seen = 0;
	};
	struct Message
	{
		Bytes bytes;
		Clock::time_point assembledAt;
	};

	static std::runtime_error socketError(const char* action)
	{
		return std::runtime_error(std::string("LAN ") + action + " (Winsock " + std::to_string(WSAGetLastError()) + ")");
	}

	void SendFrame(const Bytes& bytes)
	{
		if (sendto(socketHandle, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), 0,
				reinterpret_cast<const sockaddr*>(&peer), sizeof(peer)) != static_cast<int>(bytes.size()))
			throw socketError("send");
	}

	void Receive(const uint8_t* bytes, size_t size, const sockaddr_in& source, Clock::time_point now)
	{
		std::lock_guard<std::mutex> lock(gate);
		++stats.datagrams;
		if (!sameEndpoint(source, peer))
		{
			++stats.foreign;
			return;
		}
		if (size < Header || read(bytes, 4) != Magic || bytes[4] != Version || bytes[7] != 0
				|| !std::equal(session.begin(), session.end(), bytes + 8))
		{
			++stats.invalid;
			return;
		}
		const auto sequence = read(bytes + 24, 8);
		const size_t length = read(bytes + 32, 2);
		const size_t amount = read(bytes + 34, 2);
		const size_t index = bytes[5], count = bytes[6];
		const bool heartbeat = sequence == 0 && length == 0 && amount == 0 && count == 0 && index == 0;
		if (size != Header + amount || amount > Chunk ||
				(!heartbeat && (sequence == 0 || length < 2 || length > MaxMessage || count != fragmentCount(length)
					|| index >= count || amount != std::min(Chunk, length - index * Chunk))) ||
				read(bytes + 36, 4) != (crc(bytes + Header, amount, crc(bytes, 36)) ^ 0xffffffff))
		{
			++stats.invalid;
			return;
		}
		lastPeer = now;
		if (heartbeat)
			return;
		highestObserved = std::max(highestObserved, sequence);
		if (sequence <= lastComplete)
		{
			++stats.stale;
			return;
		}
		auto position = assemblies.find(sequence);
		if (position == assemblies.end())
		{
			if (assemblies.size() >= AssemblyLimit)
			{
				++stats.evicted;
				assemblies.erase(assemblies.begin());
			}
			position = assemblies.emplace(sequence, Assembly{now, Bytes(length)}).first;
		}
		auto& assembly = position->second;
		if (assembly.bytes.size() != length)
		{
			++stats.invalid;
			return;
		}
		const auto bit = uint32_t{1} << index;
		if ((assembly.seen & bit) != 0)
		{
			++stats.duplicate;
			return;
		}
		std::memcpy(assembly.bytes.data() + index * Chunk, bytes + Header, amount);
		assembly.seen |= bit;
		if (assembly.seen != ((uint32_t{1} << count) - 1))
			return;

		// Complete-message order is monotonic. Never replay a late older state.
		lastComplete = sequence;
		++stats.assembled;
		const auto micros = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now - assembly.created).count());
		stats.assemblyMicros += micros;
		stats.assemblyMaxMicros = std::max(stats.assemblyMaxMicros, micros);
		if (queue.size() == QueueLimit)
		{
			queue.pop_front();
			++stats.overflow;
		}
		queue.push_back({std::move(assembly.bytes), now});
		stats.queuePeak = std::max<uint64_t>(stats.queuePeak, queue.size());
		stats.evicted += static_cast<uint64_t>(std::distance(assemblies.begin(), position));
		assemblies.erase(assemblies.begin(), std::next(position));
		ready.notify_one();
	}

	void Run() noexcept
	{
		try
		{
			auto nextHeartbeat = Clock::now();
			auto nextSummary = nextHeartbeat + std::chrono::seconds(5);
			while (!stopping.load() && !failed.load())
			{
				auto now = Clock::now();
				if (now >= nextHeartbeat)
				{
					std::lock_guard<std::mutex> lock(sendGate);
					SendFrame(frame(session, 0, nullptr, 0, 0));
					nextHeartbeat = now + HeartbeatInterval;
				}
				fd_set set;
				FD_ZERO(&set);
				FD_SET(socketHandle, &set);
				timeval timeout{0, 25000};
				const auto result = select(0, &set, nullptr, nullptr, &timeout);
				if (result == SOCKET_ERROR)
					throw socketError("select");
				if (result > 0)
				{
					// Yield periodically to expiry, health and stop even under a flood.
					for (int received = 0; received < 64 && !stopping.load(); ++received)
					{
						std::array<uint8_t, Header + Chunk> data;
						sockaddr_in source{};
						int size = sizeof(source);
						const int count = recvfrom(socketHandle, reinterpret_cast<char*>(data.data()),
							static_cast<int>(data.size()), 0, reinterpret_cast<sockaddr*>(&source), &size);
						if (count == SOCKET_ERROR)
						{
							const int error = WSAGetLastError();
							if (error == WSAEWOULDBLOCK || error == WSAECONNRESET)
								break;
							if (error == WSAEMSGSIZE)
							{
								std::lock_guard<std::mutex> lock(gate);
								++stats.invalid;
								continue;
							}
							throw socketError("receive");
						}
						Receive(data.data(), static_cast<size_t>(count), source, Clock::now());
					}
				}
				now = Clock::now();
				{
					std::lock_guard<std::mutex> lock(gate);
					for (auto i = assemblies.begin(); i != assemblies.end();)
					{
						if (now - i->second.created >= AssemblyLifetime)
						{
							++stats.expired;
							i = assemblies.erase(i);
						}
						else
							++i;
					}
				}
				if (lastPeer != Clock::time_point{} && now - lastPeer >= PeerTimeout)
					throw std::runtime_error("LAN gameplay peer stopped responding");
				if (now >= nextSummary)
				{
					WriteSummary("Running");
					nextSummary = now + std::chrono::seconds(5);
				}
			}
		}
		catch (const std::exception& error) { Fail(error.what()); }
	}

	void Fail(const char* reason)
	{
		if (failed.exchange(true))
			return;
		ready.notify_all();
		if (!failure.empty())
		{
			if (auto file = std::fopen(failure.c_str(), "wb"))
			{
				std::fputs(reason, file);
				std::fclose(file);
			}
		}
		WriteSummary("Failed");
	}

	void WriteSummary(const char* state) const
	{
		if (log.empty())
			return;
		if (auto file = std::fopen(log.c_str(), "ab"))
		{
			const Stats counts = Snapshot();
			char localIp[INET_ADDRSTRLEN]{}, peerIp[INET_ADDRSTRLEN]{};
			InetNtopA(AF_INET, &local.sin_addr, localIp, sizeof(localIp));
			InetNtopA(AF_INET, &peer.sin_addr, peerIp, sizeof(peerIp));
			std::fprintf(file, "{\"state\":\"%s\",\"local\":\"%s:%u\",\"peer\":\"%s:%u\","
				"\"sent\":%llu,\"datagrams\":%llu,\"assembled\":%llu,\"delivered\":%llu,"
				"\"invalid\":%llu,\"foreign\":%llu,\"stale\":%llu,\"duplicate\":%llu,"
				"\"expired\":%llu,\"evicted\":%llu,\"overflow\":%llu,\"queue_peak\":%llu,"
				"\"resets\":%llu,\"assembly_us\":%llu,\"assembly_max_us\":%llu,\"queue_us\":%llu,\"queue_max_us\":%llu}\n",
				state, localIp, static_cast<unsigned>(ntohs(local.sin_port)), peerIp, static_cast<unsigned>(ntohs(peer.sin_port)),
				counts.sent, counts.datagrams, counts.assembled, counts.delivered, counts.invalid, counts.foreign, counts.stale,
				counts.duplicate, counts.expired, counts.evicted, counts.overflow, counts.queuePeak, counts.resets,
				counts.assemblyMicros, counts.assemblyMaxMicros, counts.queueMicros, counts.queueMaxMicros);
			std::fclose(file);
		}
	}

	const sockaddr_in local, peer;
	const Session session;
	const std::string log, failure;
	SOCKET socketHandle = INVALID_SOCKET;
	std::thread worker;
	std::atomic<bool> stopping{false}, failed{false};
	mutable std::mutex gate;
	std::mutex sendGate, stopGate;
	std::condition_variable ready;
	std::map<uint64_t, Assembly> assemblies;
	std::deque<Message> queue;
	Clock::time_point lastPeer{};
	uint64_t lastComplete = 0, highestObserved = 0, sentSequence = 0;
	Stats stats;
};
}


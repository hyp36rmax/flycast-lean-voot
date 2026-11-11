/*
	Copyright 2022 flyinghead

	This file is part of Flycast.

    Flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once
#include "types.h"
#include "net_platform.h"
#if defined(_WIN32) && defined(FLYCAST_HOSTED_STARTUP)
#include "ot_lan_transport.h"
#include <memory>
#endif
#include "naomi_network_trace.h"
#include "miniupnp.h"
#include "cfg/option.h"
#include "emulator.h"
#include "oslib/oslib.h"

#include <algorithm>
#include <chrono>
#include <atomic>
#include <future>
#include <vector>

class NaomiNetwork
{
public:
	class Exception : public FlycastException
	{
	public:
		Exception(const std::string& reason) : FlycastException(reason) {}
	};

	~NaomiNetwork() { shutdown(); }

	std::future<bool> startNetworkAsync()
	{
		networkStopping = false;
		_startNow = false;
		return std::async(std::launch::async, [this] {
			ThreadName _("NaomiNetwork-start");
			bool res = startNetwork();
			emu.setNetworkState(res);
			return res;
		});
	}

	void shutdown()
	{
		enableNetworkBroadcast(false);
		emu.setNetworkState(false);
#if defined(_WIN32) && defined(FLYCAST_HOSTED_STARTUP)
		if (std::getenv("PROJECTOT_LAN08_SESSION") != nullptr)
			networkStopping = true;
		if (auto link = std::atomic_exchange(&lan, std::shared_ptr<otlan::Transport>{}))
			link->Stop();
#endif
		if (sock != INVALID_SOCKET)
		{
			naomi_network_trace::event("shutdown",sock,nullptr,-1,-1,0,0,0,slotId,slotCount);
			closesocket(sock);
			sock = INVALID_SOCKET;
		}
	}

	bool receive(u8 *data, u32 size, u16 *packetNumber)
	{
		poll();
		if (receivedData.empty())
			return false;

		if(receivedData.size()!=size)naomi_network_trace::event("guest_receive_size_mismatch",sock,nullptr,Data,this->packetNumber,(int)receivedData.size(),0,0,slotId,slotCount,(int)size);
		size = std::min(size, (u32)receivedData.size());
		memcpy(data, receivedData.data(), size);
		receivedData.erase(receivedData.begin(), receivedData.begin() + size);
		*packetNumber = this->packetNumber;

		return true;
	}

#ifdef _WIN32
	bool waitForData(std::chrono::microseconds timeout);
#endif

	void resetLanBoard()
	{
#if defined(_WIN32) && defined(FLYCAST_HOSTED_STARTUP)
		if (auto link = std::atomic_load(&lan))
		{
			link->ResetBoard();
			receivedData.clear();
		}
#endif
	}

	void send(u8 *data, u32 size, u16 packetNumber)
	{
		verify(size < sizeof(Packet::data.payload));
		Packet packet(Data);
		memcpy(packet.data.payload, data, size);
		packet.data.packetNumber = packetNumber;
		send(&nextPeer, &packet, packet.size(size));
	}

	int getSlotCount() const { return slotCount; }
	int getSlotId() const { return slotId; }
	void startNow() {
		if (config::ActAsServer)
			_startNow = true;
	}
	void setMaxSlots(int maxSlots, bool satelliteSupported) {
		this->maxSlots = maxSlots;
		this->satelliteSupported = satelliteSupported;
	}

private:
	enum PacketType : u16 {
		SyncReq,
		SyncReply,
		Start,
		Data,
		Ack,
		NAck
	};

	#pragma pack(push, 1)
	struct Packet
	{
		Packet(PacketType type = SyncReq) : type(type) {}

		PacketType type;
		union {
			struct {
				u16 nodeId;
				u16 nextNodePort;
				u32 nextNodeIp;
			} sync;
			struct {
				u16 nodeCount;
			} start;
			struct {
				u16 packetNumber;
				u8 payload[0x4000];
			} data;
		};

		size_t size(size_t dataSize = 0) const
		{
			size_t sz = sizeof(type);
			switch (type) {
			case SyncReq:
			case SyncReply:
				sz += sizeof(sync);
				break;
			case Start:
				sz += sizeof(start);
				break;
			case Data:
				sz += sizeof(data.packetNumber) + dataSize;
				break;
			default:
				break;
			}
			return sz;
		}
	};
	#pragma pack(pop)

	bool init();

	void createSocket();

	bool startNetwork();

	void poll()
	{
#if defined(_WIN32) && defined(FLYCAST_HOSTED_STARTUP)
		if (auto link = std::atomic_load(&lan))
		{
			if (link->Failed())
				throw Exception("LAN receive worker stopped");
			otlan::Bytes bytes;
			for (size_t i = 0; i < otlan::QueueLimit && link->Pop(bytes); ++i)
			{
				Packet packet;
				if (bytes.size() < sizeof(packet.type) || bytes.size() > sizeof(packet))
					throw Exception("Invalid framed NAOMI packet size");
				std::memcpy(&packet, bytes.data(), bytes.size());
				if (bytes.size() < packet.size(0))
					throw Exception("Truncated framed NAOMI packet");
				naomi_network_trace::event("rx",INVALID_SOCKET,&link->Peer(),packet.type,
					packet.type==Data?packet.data.packetNumber:-1,(int)bytes.size(),(int)bytes.size(),0,slotId,slotCount);
				receive(&link->Peer(), &packet, static_cast<u32>(bytes.size()));
			}
			return;
		}
		if (networkStopping && std::getenv("PROJECTOT_LAN08_SESSION") != nullptr)
			throw Exception("LAN transport was cancelled");
#endif
		Packet packet;
		sockaddr_in addr;
		while (true)
		{
			socklen_t len = sizeof(addr);
			int rc = recvfrom(sock, (char *)&packet, sizeof(packet), 0, (sockaddr *)&addr, &len);
			if (rc == -1)
			{
				int error = get_last_error();
				if(error!=L_EWOULDBLOCK&&error!=L_EAGAIN)naomi_network_trace::event("receive_socket_error",sock,nullptr,-1,-1,0,rc,error,slotId,slotCount);
				if (error == L_EWOULDBLOCK || error == L_EAGAIN)
					break;
#ifdef _WIN32
				if (error == WSAECONNRESET)
					// Happens if the previous send resulted in an ICMP Port Unreachable message
					break;
#endif
				throw Exception("Receive error: errno " + std::to_string(error));
			}
			naomi_network_trace::event("rx",sock,&addr,rc>=2?packet.type:-1,rc>=4&&packet.type==Data?packet.data.packetNumber:-1,rc,rc,0,slotId,slotCount);
			if (rc < (int)packet.size(0))
				throw Exception("Receive error: truncated packet");
			receive(&addr, &packet, rc);
		}
	}

	bool receive(const sockaddr_in *addr, const Packet *packet, u32 size);

	void sendAck(const sockaddr_in *addr, bool ack = true)
	{
		Packet packet(ack ? Ack : NAck);
		send(addr, &packet, packet.size());
	}

	void send(const sockaddr_in *addr, const Packet *packet, u32 size)
	{
#if defined(_WIN32) && defined(FLYCAST_HOSTED_STARTUP)
		if (auto link = std::atomic_load(&lan))
		{
			try { link->Send(*addr, packet, size); }
			catch (const std::exception& error) { throw Exception(error.what()); }
			naomi_network_trace::event("tx",INVALID_SOCKET,addr,packet->type,
				packet->type==Data?packet->data.packetNumber:-1,(int)size,(int)size,0,slotId,slotCount);
			return;
		}
		if (networkStopping && std::getenv("PROJECTOT_LAN08_SESSION") != nullptr)
			throw Exception("LAN transport was cancelled");
#endif
		if (loopbackOnly && addr->sin_addr.s_addr != htonl(INADDR_LOOPBACK))
			throw Exception("Loopback cabinet rejected an external destination");
		int rc = sendto(sock, (const char *)packet, size, 0,
				(sockaddr *)addr, sizeof(*addr));
		const int error=rc!=(int)size?get_last_error():0;
		naomi_network_trace::event("tx",sock,addr,packet->type,packet->type==Data?packet->data.packetNumber:-1,(int)size,rc,error,slotId,slotCount);
		if (rc != (int)size)
			throw Exception("Send failed: errno " + std::to_string(error));
		DEBUG_LOG(NETWORK, "Sent port %d pckt %d size %x", ntohs(addr->sin_port), packet->type, size - (u32)packet->size(0));
	}

#if defined(_WIN32) && defined(FLYCAST_HOSTED_STARTUP)
	std::shared_ptr<otlan::Transport> lan;
#endif
	sock_t sock = INVALID_SOCKET;
	bool loopbackOnly = false;
	int slotCount = 0;
	int slotId = 0;
	std::atomic<bool> networkStopping{ false };
	MiniUPnP miniupnp;

	sockaddr_in nextPeer;
	std::vector<u8> receivedData;
	u16 packetNumber = 0;
	bool _startNow = false;
	int maxSlots = 0;
	bool satelliteSupported = false;

	// Server stuff
	struct Slave
	{
		int state;
		sockaddr_in addr;
	};
	std::vector<Slave> slaves;

	// Client stuff
	u32 serverIp;

public:
	static constexpr u16 SERVER_PORT = 37391;
};
extern NaomiNetwork naomiNetwork;

void setNaomiNetworkConfig(int node, int nodeCount, bool satellite = false);
std::pair<int, bool> naomiNetworkMaxNodes();

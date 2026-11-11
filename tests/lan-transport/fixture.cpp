// SPDX-License-Identifier: GPL-2.0-or-later
#include "ot_lan_transport.h"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>

using namespace std::chrono_literals;
using otlan::Bytes;
using otlan::Clock;
using otlan::Transport;
namespace fs = std::filesystem;

static const auto session = otlan::parseSession("0123456789ABCDEF0123456789ABCDEF");
static void check(bool result, const char* message)
{
	if (!result) throw std::runtime_error(message);
}
static void until(const std::function<bool()>& predicate, Clock::duration timeout)
{
	const auto end = Clock::now() + timeout;
	while (!predicate())
	{
		check(Clock::now() < end, "fixture milestone timed out");
		std::this_thread::sleep_for(2ms);
	}
}
static void mark(const fs::path& folder, const char* name) { std::ofstream(folder / name) << "ready"; }
static Bytes receive(Transport& transport, std::chrono::milliseconds timeout)
{
	Bytes bytes;
	check(transport.Wait(timeout) && transport.Pop(bytes), "no complete message before deadline");
	return bytes;
}
static Bytes data(size_t sequence, uint8_t role)
{
	Bytes bytes(2820);
	bytes[0] = 3;
	bytes[1] = 0;
	bytes[2] = static_cast<uint8_t>(sequence);
	bytes[3] = static_cast<uint8_t>(sequence >> 8);
	for (size_t i = 4; i < bytes.size(); ++i)
		bytes[i] = static_cast<uint8_t>((i * 37 + sequence * 13 + role) & 0xff);
	return bytes;
}
static void send(Transport& transport, const Bytes& bytes)
{
	transport.Send(transport.Peer(), bytes.data(), bytes.size());
}
static void pair(bool master, const fs::path& folder, size_t count)
{
	const char* role = master ? "MASTER" : "SLAVE";
	Transport transport(otlan::endpoint("127.0.0.1", master ? 38901 : 38902),
		otlan::endpoint("127.0.0.1", master ? 38902 : 38901), session,
		(folder / (std::string(role) + ".jsonl")).string());
	mark(folder, role);
	until([&] { return fs::exists(folder / "go"); }, 10s);
	const Bytes request{0,0,0,0,0,0,0,0,0,0}, reply{1,0,1,0,0x97,0xf5,0,0,0,0};
	const Bytes start{2,0,2,0}, ack{4,0};
	if (master)
	{
		check(receive(transport, 2s) == request, "bad SyncReq");
		send(transport, reply);
		send(transport, start);
		check(receive(transport, 2s) == ack, "bad ACK");
	}
	else
	{
		send(transport, request);
		check(receive(transport, 2s) == reply, "bad SyncReply");
		check(receive(transport, 2s) == start, "bad two-node Start");
		send(transport, ack);
	}
	auto next = Clock::now();
	long long maxWait = 0;
	const auto started = next;
	for (size_t i = 0; i < count; ++i)
	{
		next += 16667us;
		send(transport, data(i, master ? 0 : 1));
		const auto before = Clock::now();
		check(receive(transport, 250ms) == data(i, master ? 1 : 0), "native frame corrupted or reordered");
		const auto wait = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - before).count();
		maxWait = std::max(maxWait, wait);
		std::this_thread::sleep_until(next);
	}
	check(maxWait < 100000, "normal transport wait reached 100 ms");
	mark(folder, master ? "master-done" : "slave-done");
	until([&] { return fs::exists(folder / "master-done") && fs::exists(folder / "slave-done"); }, 5s);
	const auto stats = transport.Snapshot();
	check(stats.overflow == 0 && stats.invalid == 0 && stats.expired == 0, "unexpected normal receive failure");
	std::cout << role << " sent=" << stats.sent << " delivered=" << stats.delivered << " max_wait_us=" << maxWait
		<< " elapsed_ms=" << std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-started).count() << '\n';
}
static void faults(const fs::path& folder)
{
	const auto local=otlan::endpoint("127.0.0.1",38901), peer=otlan::endpoint("127.0.0.1",38902);
	Transport transport(local, peer, session,(folder/"faults.jsonl").string());
	bool conflict = false;
	try { Transport duplicate(local, peer, session); }
	catch (const std::runtime_error&) { conflict = true; }
	check(conflict, "exclusive bind did not reject conflict");
	SOCKET sender = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
	check(sender!=INVALID_SOCKET && bind(sender,reinterpret_cast<const sockaddr*>(&peer),sizeof(peer))==0,"fixture raw peer bind failed");
	auto emit = [&](const Bytes& bytes) {
		check(sendto(sender,reinterpret_cast<const char*>(bytes.data()),static_cast<int>(bytes.size()),0,
			reinterpret_cast<const sockaddr*>(&local),sizeof(local))==static_cast<int>(bytes.size()),"fixture send failed");
	};
	const auto payload = data(42,1);
	const auto part = [&](uint64_t seq,size_t index) { return otlan::frame(session,seq,payload.data(),payload.size(),index); };
	const auto all = [&](uint64_t seq) { for(size_t i=0;i<3;++i) emit(part(seq,i)); };
	emit(part(1,2)); emit(part(1,0)); emit(part(1,0));
	auto broken=part(1,1); broken.back()^=1; emit(broken);
	auto unrelated=part(1,1); unrelated[8]^=1; emit(unrelated);
	emit(part(1,1));
	check(receive(transport,1s)==payload,"reordered fragments did not reassemble");
	emit(part(2,0)); emit(part(2,2));
	until([&] {return transport.Snapshot().expired==1;},1s);
	emit(part(2,1)); all(3); all(2);
	check(receive(transport,1s)==payload,"following valid message did not pass expired packet");
	Bytes empty;
	check(!transport.Pop(empty),"stale message replayed");
	for(uint64_t i=4;i<62;++i) all(i);
	until([&] {return transport.Snapshot().queuePeak==otlan::QueueLimit&&transport.Snapshot().overflow>0;},2s);
	for(int i=0;i<4;++i) transport.ResetBoard();
	check(!transport.Pop(empty),"reset did not clear pending queue");
	all(4); all(63);
	check(receive(transport,1s)==payload,"reset blocked newer traffic");
	auto stats=transport.Snapshot();
	check(stats.duplicate>=1 && stats.invalid>=2 && stats.stale>=4 && stats.resets==4 && stats.overflow>=1,
		"receive anomaly counters incomplete");
	closesocket(sender);
	until([&] {return transport.Failed();},6s);
	bool notified=false;
	try { transport.Wait(1ms); } catch(const std::runtime_error&) { notified=true; }
	check(notified,"worker loss did not wake consumer");
	transport.Stop();
	Transport restarted(local,peer,session);
	restarted.Stop();
	std::cout << "PASS exclusive bind, fragment integrity/reorder/expiry, stale suppression, queue pressure, reset, peer loss, restart\n";
}
int main(int argc,char** argv)
{
	WSADATA winsock{};
	if(WSAStartup(MAKEWORD(2,2),&winsock)!=0) return 1;
	try
	{
		check(argc>=3,"missing arguments");
		std::string mode=argv[1];fs::path folder=argv[2];fs::create_directories(folder);
		if(mode=="faults") faults(folder);
		else if(mode=="peer-exit" || mode=="wait-exit")
		{
			const bool sender=mode=="peer-exit";
			Transport transport(otlan::endpoint("127.0.0.1",sender?38902:38901),
				otlan::endpoint("127.0.0.1",sender?38901:38902),session);
			if(sender)
			{
				send(transport,data(7,1)); mark(folder,"peer-sent");
				std::this_thread::sleep_for(30s);
				throw std::runtime_error("parent did not terminate the peer");
			}
			mark(folder,"waiting");
			check(receive(transport,5s)==data(7,1),"peer-loss setup failed");
			until([&] {return transport.Failed();},8s);
			transport.Stop();
			std::cout<<"PASS abrupt process exit detected; owned worker joined\n";
		}
		else pair(mode=="master",folder,argc>3?std::stoul(argv[3]):3600);
		WSACleanup();
		return 0;
	}
	catch(const std::exception& error)
	{
		std::cerr<<error.what()<<'\n';
		WSACleanup();
		return 1;
	}
}


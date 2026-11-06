// SPDX-License-Identifier: GPL-2.0-or-later

#include <winsock2.h>
#include <windows.h>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <stdexcept>
#include <cstdio>
using namespace std::chrono_literals;
int get_last_error(){return WSAGetLastError();}
struct NaomiNetwork {struct Exception:std::runtime_error{using std::runtime_error::runtime_error;};std::vector<unsigned char> receivedData;SOCKET sock=INVALID_SOCKET;bool waitForData(std::chrono::microseconds);};
bool NaomiNetwork::waitForData(std::chrono::microseconds timeout)
{
	if (!receivedData.empty())
		return true;
	if (timeout.count() <= 0)
		return false;
	fd_set readable;
	FD_ZERO(&readable);
	FD_SET(sock, &readable);
	timeval limit { static_cast<long>(timeout.count() / 1000000),
		static_cast<long>(timeout.count() % 1000000) };
	const int ready = select(0, &readable, nullptr, nullptr, &limit);
	if (ready < 0)
		throw Exception("Receive wait failed: errno " + std::to_string(get_last_error()));
	return ready != 0;
}
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
int main(){try{
 WSADATA data;require(WSAStartup(MAKEWORD(2,2),&data)==0,"WSAStartup");
 NaomiNetwork n;n.sock=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);require(n.sock!=INVALID_SOCKET,"receiver socket");
 sockaddr_in address{};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);require(bind(n.sock,(sockaddr*)&address,sizeof(address))==0,"bind");int len=sizeof(address);require(getsockname(n.sock,(sockaddr*)&address,&len)==0,"getsockname");
 SOCKET sender=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);require(sender!=INVALID_SOCKET,"sender");
 n.receivedData.push_back(7);require(n.waitForData(0us),"buffered data readiness");n.receivedData.clear();require(!n.waitForData(0us),"zero budget");require(!n.waitForData(-1us),"negative budget");
 ULONG64 before=0,after=0;QueryThreadCycleTime(GetCurrentThread(),&before);auto start=std::chrono::steady_clock::now();require(!n.waitForData(20000us),"empty socket timeout");auto elapsed=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();QueryThreadCycleTime(GetCurrentThread(),&after);require(elapsed>=10000&&elapsed<300000,"timeout wall bound");printf("Idle timeout wall_us=%lld active_cycles=%llu\n",(long long)elapsed,(unsigned long long)(after-before));
 std::thread delayed([&]{std::this_thread::sleep_for(5ms);sendto(sender,"AB",2,0,(sockaddr*)&address,sizeof(address));});bool ready=n.waitForData(100000us);delayed.join();require(ready,"delayed packet wakes wait");char buf[8]{};require(recv(n.sock,buf,sizeof(buf),0)==2&&buf[0]=='A'&&buf[1]=='B',"readiness leaves payload untouched");
 for(const char value:{'C','D'})require(sendto(sender,&value,1,0,(sockaddr*)&address,sizeof(address))==1,"send ordered datagram");for(const char value:{'C','D'}){require(n.waitForData(100000us),"queued readiness");require(recv(n.sock,buf,sizeof(buf),0)==1&&buf[0]==value,"FIFO packet content retained");}
 closesocket(n.sock);n.sock=INVALID_SOCKET;bool failed=false;try{n.waitForData(1000us);}catch(const NaomiNetwork::Exception&){failed=true;}require(failed,"invalid socket uses original exception channel");closesocket(sender);WSACleanup();puts("PASS: buffered/zero/negative budget, real UDP timeout, delayed arrival, non-consuming FIFO readiness and invalid-socket exception.");return 0;
 }catch(const std::exception& e){printf("FAIL: %s\n",e.what());return 1;}}

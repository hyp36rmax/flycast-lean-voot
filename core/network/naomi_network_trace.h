// Optional Project OT diagnostic metadata only. No payloads or protocol changes.
#pragma once
#include "net_platform.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <mutex>
#include <string>
#ifdef _WIN32
#include <process.h>
#endif
namespace naomi_network_trace {
inline bool& retired(){static bool r=false;return r;}
struct Writer {
 std::mutex mutex; FILE *file=nullptr; uint64_t tx=0,rx=0,records=0; const char *role="UNKNOWN";
 std::chrono::steady_clock::time_point begin=std::chrono::steady_clock::now();
 Writer(){const char *path=std::getenv("PROJECTOT_NAOMI_TRACE");const char *r=std::getenv("PROJECTOT_CABINET_ROLE");if(r&&std::string(r)=="MASTER")role="MASTER";else if(r&&std::string(r)=="SLAVE")role="SLAVE";if(path&&*path)file=std::fopen(path,"wb");}
 ~Writer(){retired()=true;if(file)std::fclose(file);}
};
inline Writer& writer(){static Writer w;return w;}
inline void endpoint(const sockaddr_in *a,char *out,size_t size){if(!a){std::snprintf(out,size,"none");return;}char ip[INET_ADDRSTRLEN]{};inet_ntop(AF_INET,&a->sin_addr,ip,sizeof(ip));std::snprintf(out,size,"%s:%u",ip,ntohs(a->sin_port));}
inline void event(const char *name,sock_t socket=INVALID_SOCKET,const sockaddr_in *peer=nullptr,int type=-1,int sequence=-1,int bytes=0,int result=0,int error=0,int slot=-1,int nodes=0,int detail=0,uint64_t waitUs=0,uint64_t intervalUs=0,unsigned timeoutRun=0){
#ifdef _WIN32
 const int saved=WSAGetLastError();struct Restore{int value;~Restore(){WSASetLastError(value);}}restore{saved};
#else
 const int saved=errno;struct Restore{int value;~Restore(){errno=value;}}restore{saved};
#endif
 if(retired())return;auto& w=writer();if(!w.file)return;std::lock_guard<std::mutex> lock(w.mutex);
 if(std::string(name)=="tx")++w.tx;else if(std::string(name)=="rx")++w.rx;
 const bool packet=std::string(name)=="tx"||std::string(name)=="rx";
 const uint64_t n=std::string(name)=="tx"?w.tx:w.rx;
 if(packet&&n>64&&n%300!=0&&error==0)return;
 if(w.records++>=8192)return;
 sockaddr_in local{};socklen_t size=sizeof(local);const bool have=socket!=INVALID_SOCKET&&getsockname(socket,(sockaddr*)&local,&size)==0;
 char a[64],b[64];endpoint(have?&local:nullptr,a,sizeof(a));endpoint(peer,b,sizeof(b));
 const auto utc=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
 const auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-w.begin).count();
#ifdef _WIN32
 const int pid=_getpid();
#else
 const int pid=getpid();
#endif
 std::fprintf(w.file,"{\"utc_ms\":%lld,\"monotonic_us\":%lld,\"pid\":%d,\"role\":\"%s\",\"event\":\"%s\",\"local_bind\":\"%s\",\"remote\":\"%s\",\"packet_type\":%d,\"sequence\":%d,\"bytes\":%d,\"result\":%d,\"error\":%d,\"slot\":%d,\"nodes\":%d,\"detail\":%d,\"tx_count\":%llu,\"rx_count\":%llu,\"wait_us\":%llu,\"vblank_interval_us\":%llu,\"timeout_run\":%u}\n",(long long)utc,(long long)us,pid,w.role,name,a,b,type,sequence,bytes,result,error,slot,nodes,detail,(unsigned long long)w.tx,(unsigned long long)w.rx,(unsigned long long)waitUs,(unsigned long long)intervalUs,timeoutRun);std::fflush(w.file);
}
}

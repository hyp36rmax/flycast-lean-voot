#include "rend/frame_delivery_trace.h"
#if defined(FLYCAST_VOOT_RENDER_DIAGNOSTICS) && defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#endif
/*
	Created on: Mar 15, 2020

	Copyright 2020 flyinghead

	This file is part of flycast.

    flycast is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    flycast is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with flycast.  If not, see <https://www.gnu.org/licenses/>.
 */
//
// Optical communication board (837-13691)
// Ring topology
// 10 Mbps
// Max packet size 0x4000
//
#include "naomi_m3comm.h"
#include "naomi_regs.h"
#include "hw/holly/sb.h"
#include "hw/sh4/sh4_mem.h"
#include "network/naomi_network.h"
#include "emulator.h"
#include "oslib/oslib.h"
#include "oslib/i18n.h"

#include <chrono>
#include <memory>

constexpr u16 COMM_CTRL_CPU_RAM = 1 << 0;
constexpr u16 COMM_CTRL_RESET = 1 << 5;		// rising edge
constexpr u16 COMM_CTRL_G1DMA = 1 << 14;	// active low

struct CommBoardStat
{
	u16 transmode;		// communication mode (0: master, positive value: slave)
	u16 totalnode;		// Total number of nodes (same value is entered in upper and lower 8 bits)
	u16 nodeID;			// Local node ID (the same value is entered in the upper and lower 8 bits)
	u16 transcnt;		// counter (value increases by 1 per frame)
	u16 cts;			// CTS timer value (for debugging)
	u16 dma_rx_addr;	// DMA receive address (for debugging)
	u16 dma_rx_size;	// DMA receive size (for debugging)
	u16 dma_tx_addr;	// DMA transmit address (for debugging)
	u16 dma_tx_size;	// DMA transmission size (for debugging)
	u16 dummy[7];
};

#if !defined(__OpenBSD__)
static inline u16 swap16(u16 w)
{
	return (w >> 8) | (w << 8);
}
#endif

static void vblankCallback(Event event, void *param) {
	((NaomiM3Comm *)param)->vblank();
}

void NaomiM3Comm::closeNetwork()
{
	EventManager::unlisten(Event::VBlank, vblankCallback, this);
	naomiNetwork.shutdown();

	comm_ctrl = 0xC000;
	comm_offset = 0;
	comm_status1 = 0;
	comm_status2 = 0;
	memset(m68k_ram, 0, 128_KB);
	memset(comm_ram, 0, 128_KB);
	naomi_network_trace::event("comm_closed",INVALID_SOCKET,nullptr,-1,packet_number,0,0,0,slot_id,slot_count);
	packet_number = 0;
	slot_count = 0;
	slot_id = 0;
}

void NaomiM3Comm::connectNetwork()
{
	os_notify(i18n::T("Network started"), 5000);
	if (slot_count >= 2)
		naomiNetwork.resetLanBoard();
	packet_number = 0;
	slot_count = naomiNetwork.getSlotCount();
	slot_id = naomiNetwork.getSlotId();
	if (slot_count >= 2)
	{
		connectedState();
		EventManager::listen(Event::VBlank, vblankCallback, this);
	}
}

bool NaomiM3Comm::receiveNetwork()
{
	const u32 slot_size = swap16(*(u16*)&m68k_ram[0x204]);
	const u32 packet_size = slot_size * slot_count;

	std::unique_ptr<u8[]> buf = std::make_unique<u8[]>(packet_size);

	u16 packetNumber;
	if (!naomiNetwork.receive(buf.get(), packet_size, &packetNumber))
		return false;

	*(u16*)&comm_ram[6] = swap16(packetNumber);
	memcpy(&comm_ram[0x100 + slot_size], buf.get(), packet_size);
	if(packetNumber<8||packetNumber%300==0)naomi_network_trace::event("comm_rx_delivered",INVALID_SOCKET,nullptr,-1,packetNumber,(int)packet_size,1,0,slot_id,slot_count,(int)slot_size);

	return true;
}

void NaomiM3Comm::sendNetwork()
{
	const u32 packet_size = swap16(*(u16*)&m68k_ram[0x204]) * slot_count;
	if(packet_number<8||packet_number%300==0)naomi_network_trace::event("comm_tx",INVALID_SOCKET,nullptr,-1,packet_number,(int)packet_size,0,0,slot_id,slot_count);
	naomiNetwork.send(&comm_ram[0x100], packet_size, packet_number);
	packet_number++;
}

u32 NaomiM3Comm::ReadMem(u32 address, u32 size)
{
	switch (address)
	{
	case NAOMI_COMM_CTRL_addr:
		//DEBUG_LOG(NAOMI, "NAOMI_COMM_CTRL read");
		return comm_ctrl;

	case NAOMI_COMM_OFFSET_addr:
		//DEBUG_LOG(NAOMI, "NAOMI_COMM_OFFSET read");
		return comm_offset;

	case NAOMI_COMM_DATA_addr:
	{
		u16 value;
		if (comm_ctrl & COMM_CTRL_CPU_RAM)
			value = *(u16*)&m68k_ram[comm_offset];
		else
			// TODO u16 *commram = (u16*)membank("comm_ram")->base();
			value = *(u16*)&comm_ram[comm_offset];
		value = swap16(value);
		if (value == 0xb002 && trace_b002_reads++ < 8)
			naomi_network_trace::event("comm_b002_data_read",INVALID_SOCKET,nullptr,-1,packet_number,0,value,0,slot_id,slot_count,comm_offset);
		DEBUG_LOG(NAOMI, "COMM_DATA %s read @ %04x: %x", (comm_ctrl & COMM_CTRL_CPU_RAM) ? "m68k ram" : "comm ram", comm_offset, value);
		comm_offset += 2;
		return value;
	}

	case NAOMI_COMM_STATUS1_addr:
		DEBUG_LOG(NAOMI, "COMM_STATUS1 read %x", comm_status1);
		return comm_status1;

	case NAOMI_COMM_STATUS2_addr:
		DEBUG_LOG(NAOMI, "COMM_STATUS2 read %x", comm_status2);
		return comm_status2;

	default:
		DEBUG_LOG(NAOMI, "NaomiM3Comm::ReadMem unmapped: %08x sz %d", address, size);
		return 0xffffffff;
	}
}

void NaomiM3Comm::connectedState()
{
	memset(&comm_ram[0xf000], 0, 16);
	comm_ram[0xf000] = 1;
	comm_ram[0xf001] = 1;
	comm_ram[0xf002] = m68k_ram[0x204];
	comm_ram[0xf003] = m68k_ram[0x205];

	u32 slot_size = swap16(*(u16*)&m68k_ram[0x204]);

	int slotCount = slot_count;
	if (settings.content.gameId == " DERBY OWNERS CLUB WE ---------"
			|| settings.content.gameId == " DERBY OWNERS CLUB ------------")
		// DOC [WE] adds 1 to the slot count in its code, so it needs the number of slaves instead
		slotCount--;
	CommBoardStat& stat = *(CommBoardStat *)&comm_ram[0];
	memset(&stat, 0, sizeof(stat));
	stat.transmode = swap16(slot_id == 0 ? 0 : 1);
	stat.totalnode = slotCount | (slotCount << 8);
	stat.nodeID = slot_id | (slot_id << 8);
	stat.cts = swap16(slot_id == 0 ? 0x7830 : 0x73a2);
	stat.dma_rx_addr = swap16(0x100 + slot_size);
	stat.dma_rx_size = swap16(slot_size * slot_count);
	stat.dma_tx_addr = swap16(0x100);
	stat.dma_tx_size = swap16(slot_size * slot_count);

	comm_status1 = 0xff01;	// But 1 at connect time before f000 is read
	comm_status2 = (slotCount << 8) | slot_id;
	naomi_network_trace::event("comm_connected",INVALID_SOCKET,nullptr,-1,packet_number,(int)(slot_size*slot_count),1,0,slot_id,slot_count,(int)slot_size);
}

void NaomiM3Comm::WriteMem(u32 address, u32 data, u32 size)
{
	switch (address)
	{
	case NAOMI_COMM_CTRL_addr:
		// bit 0: access RAM is 0 - communication RAM / 1 - M68K RAM
		// bit 1: comm RAM bank (seems R/O for SH4)
		// bit 5: M68K Reset
		// bit 6: ???
		// bit 7: might be M68K IRQ 5 or 2 - set to 0 by nlCbIntr() (on vblank)
		// bit 14: G1 DMA bus master 0 - active / 1 - disabled
		// bit 15: 0 - enable / 1 - disable this device ???
		if ((comm_ctrl & COMM_CTRL_RESET) == 0 && (data & COMM_CTRL_RESET) != 0)
		{
			naomi_network_trace::event("comm_reset",INVALID_SOCKET,nullptr,-1,packet_number,0,0,0,slot_id,slot_count,(int)data);
			DEBUG_LOG(NAOMI, "COMM_CTRL m68k reset");
			memset(&comm_ram[0], 0, 32);
			comm_status1 = 0; // varies...
			comm_status2 = 0;
			connectNetwork();
		}
		comm_ctrl = (u16)data;
		DEBUG_LOG(NAOMI, "COMM_CTRL = %x", comm_ctrl);
		return;

	case NAOMI_COMM_OFFSET_addr:
		comm_offset = (u16)data;
		//DEBUG_LOG(NAOMI, "NAOMI_COMM_OFFSET set to %x", comm_offset);
		return;

	case NAOMI_COMM_DATA_addr:
		data = swap16(data);
		if (comm_ctrl & COMM_CTRL_CPU_RAM) {
			*(u16*)&m68k_ram[comm_offset] = (u16)data;
			DEBUG_LOG(NAOMI, "COMM_DATA (cpu) written @ %04x %04x", comm_offset, (u16)data);
		}
		else {
			*(u16*)&comm_ram[comm_offset] = (u16)data;
			DEBUG_LOG(NAOMI, "COMM_DATA (comm) written @ %04x %04x", comm_offset, (u16)data);
		}
		comm_offset += 2;
		return;

	case NAOMI_COMM_STATUS1_addr:
		if (comm_status1 != (u16)data && (++trace_status_changes <= 16 || trace_status_changes % 300 == 0 || ((u16)data == 0xb002 && trace_b002_reads++ < 8))) naomi_network_trace::event("comm_status1_guest_write",INVALID_SOCKET,nullptr,-1,packet_number,0,(u16)data,0,slot_id,slot_count,comm_status1);
		comm_status1 = (u16)data;
		//DEBUG_LOG(NAOMI, "COMM_STATUS1 set to %x", comm_status0);
		return;

	case NAOMI_COMM_STATUS2_addr:
		if (comm_status2 != (u16)data && (++trace_status_changes <= 16 || trace_status_changes % 300 == 0 || ((u16)data == 0xb002 && trace_b002_reads++ < 8))) naomi_network_trace::event("comm_status2_guest_write",INVALID_SOCKET,nullptr,-1,packet_number,0,(u16)data,0,slot_id,slot_count,comm_status2);
		comm_status2 = (u16)data;
		//DEBUG_LOG(NAOMI, "COMM_STATUS2 set to %x", comm_status1);
		return;

	default:
		break;
	}
	DEBUG_LOG(NAOMI, "NaomiM3Comm::WriteMem: %x <= %x sz %d", address, data, size);
}

bool NaomiM3Comm::DmaStart(u32 addr, u32 data)
{
	if (comm_ctrl & COMM_CTRL_G1DMA)
		return false;

	DEBUG_LOG(NAOMI, "NaomiM3Comm: DMA addr %08X <-> %04x len %d %s", SB_GDSTAR, comm_offset, SB_GDLEN, SB_GDDIR == 0 ? "OUT" : "IN");
	if (SB_GDDIR == 0)
	{
		// Network write
		for (u32 i = 0; i < SB_GDLEN; i++)
			comm_ram[comm_offset++] = ReadMem8_nommu(SB_GDSTAR + i);
	}
	else
	{
		// Network read
		/*
		if (SB_GDLEN == 32 && (comm_ctrl & COMM_CTRL_CPU_RAM) == 0)
		{
			char buf[32 * 5 + 1];
			buf[0] = 0;
			for (u32 i = 0; i < SB_GDLEN; i++)
			{
				u8 value = comm_ram[comm_offset + i];
				size_t len = strlen(buf);
				snprintf(buf + len, sizeof(buf) - len, "%02x ", value);
			}
			DEBUG_LOG(NAOMI, "Comm RAM read @%x: %s", comm_offset, buf);
		}
		*/
		for (u32 i = 0; i < SB_GDLEN; i++)
			WriteMem8_nommu(SB_GDSTAR + i, comm_ram[comm_offset++]);
	}
	return true;
}

void NaomiM3Comm::vblank()
{
	if ((comm_ctrl & COMM_CTRL_RESET) == 0 || comm_status2 == 0)
		return;

	using the_clock = std::chrono::steady_clock;
	the_clock::time_point start = the_clock::now();
	const auto intervalUs = trace_last_vblank.time_since_epoch().count() == 0 ? 0 :
		std::chrono::duration_cast<std::chrono::microseconds>(start - trace_last_vblank).count();
	trace_last_vblank = start;
#if defined(FLYCAST_VOOT_RENDER_DIAGNOSTICS) && defined(_WIN32)
	uint64_t polls=0, timeouts=0;
	ULONG64 cyclesStart=0;
	QueryThreadCycleTime(GetCurrentThread(), &cyclesStart);
#endif
	try {
		bool received = false;
		do {
#if defined(FLYCAST_VOOT_RENDER_DIAGNOSTICS) && defined(_WIN32)
			++polls;
#endif
			received = receiveNetwork();
#ifdef _WIN32
			if (!received)
			{
				const auto remaining = std::chrono::milliseconds(100) - (the_clock::now() - start);
				if (remaining <= decltype(remaining)::zero()
						|| !naomiNetwork.waitForData(std::chrono::ceil<std::chrono::microseconds>(remaining)))
					break;
			}
		} while (!received);
#else
		} while (!received && the_clock::now() - start < std::chrono::milliseconds(100));
#endif
		if (!received) {
#if defined(FLYCAST_VOOT_RENDER_DIAGNOSTICS) && defined(_WIN32)
			++timeouts;
#endif
			static unsigned missing=0;if(++missing<=8||missing%300==0)naomi_network_trace::event("comm_receive_timeout",INVALID_SOCKET,nullptr,-1,packet_number,0,0,0,slot_id,slot_count,(int)missing);
			INFO_LOG(NETWORK, "No data received");
		}
#if defined(FLYCAST_VOOT_RENDER_DIAGNOSTICS) && defined(_WIN32)
		ULONG64 cyclesEnd=0;
		QueryThreadCycleTime(GetCurrentThread(), &cyclesEnd);
		frame_delivery_trace::linkWaitNs.fetch_add(std::chrono::duration_cast<std::chrono::nanoseconds>(the_clock::now()-start).count(), std::memory_order_relaxed);
		frame_delivery_trace::linkWaitCycles.fetch_add(cyclesEnd-cyclesStart, std::memory_order_relaxed);
		frame_delivery_trace::linkPolls.fetch_add(polls,std::memory_order_relaxed);
		frame_delivery_trace::linkTimeouts.fetch_add(timeouts,std::memory_order_relaxed);
#endif
		const auto waitUs = std::chrono::duration_cast<std::chrono::microseconds>(the_clock::now()-start).count();
		const auto previousTimeoutRun = trace_timeout_run;
		trace_timeout_run = received ? 0 : trace_timeout_run + 1;
		if (++trace_vblanks <= 8 || trace_vblanks % 300 == 0 ||
			(!received && trace_timeout_run <= 8) || (received && previousTimeoutRun != 0))
			naomi_network_trace::event(received ? "comm_wait_received" : "comm_wait_timeout",
				INVALID_SOCKET,nullptr,-1,packet_number,0,received ? 1 : 0,0,slot_id,slot_count,
				previousTimeoutRun,waitUs,intervalUs,trace_timeout_run);
		sendNetwork();
	} catch (const FlycastException& e) {
		naomi_network_trace::event("comm_network_exception",INVALID_SOCKET,nullptr,-1,packet_number,0,-1,0,slot_id,slot_count);
		comm_status1 = 0;
		comm_status2 = 0;
	}
}

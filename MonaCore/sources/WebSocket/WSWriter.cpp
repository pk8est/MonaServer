/*
Copyright 2014 Mona
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License received along this program for more
details (or else see http://www.gnu.org/licenses/).

This file is a part of Mona.
*/

#include "Mona/WebSocket/WSWriter.h"
#include "Mona/Logs.h"


using namespace std;


namespace Mona {

WSWriter::WSWriter(TCPSession& session) : _session(session),Writer(session.peer.connected ? OPENED : OPENING) {
	
}

void WSWriter::close(Int32 code) {
	if (code < 0)
		return;
	if (code == 0)
		code = WS::CODE_NORMAL_CLOSE;
	write(WS::TYPE_CLOSE, NULL, (UInt32)code);
	Writer::close(code);
	_session.kill(code);
}


JSONWriter& WSWriter::newDataWriter(bool modeRaw) {
	pack();
	WSSender* pSender = new WSSender(_session.invoker.poolBuffers,modeRaw);
	_senders.emplace_back(pSender);
	pSender->writer.packet.next(10); // header
	return pSender->writer;
}

void WSWriter::pack() {
	if(_senders.empty())
		return;
	WSSender& sender = *_senders.back();
	if(sender.packaged)
		return;
	JSONWriter& writer = sender.writer;
	PacketWriter& packet = writer.packet;
	UInt32 size = packet.size()-10;
	packet.clip(10-WS::HeaderSize(size));
	BinaryWriter headerWriter(packet);
	packet.clear(WS::WriteHeader(WS::TYPE_TEXT,size,headerWriter)+size);
	sender.packaged = true;
}

void WSWriter::flush(bool full) {
	if(_senders.empty())
		return;
	pack();
	Exception ex;
	for (shared_ptr<WSSender>& pSender : _senders) {
		_session.dumpResponse(pSender->data(),pSender->size());
		EXCEPTION_TO_LOG(_session.send<WSSender>(ex, qos(), pSender), "WSSender flush");
	}
	_senders.clear();
}

void WSWriter::write(UInt8 type,const UInt8* data,UInt32 size) {
	if(state()==CLOSED)
		return;
	pack();
	WSSender* pSender = new WSSender(_session.invoker.poolBuffers);
	pSender->packaged = true;
	_senders.emplace_back(pSender);
	BinaryWriter& writer = pSender->writer.packet;
	if(type==WS::TYPE_CLOSE) {
		// here size is the code!
		if(size>0) {
			WS::WriteHeader((WS::MessageType)type,2,writer);
			writer.write16(size);
		} else
			WS::WriteHeader((WS::MessageType)type,0,writer);
		return;
	}
	WS::WriteHeader((WS::MessageType)type,size,writer);
	writer.writeRaw(data,size);
}



DataWriter& WSWriter::writeInvocation(const std::string& name) {
	if(state()==CLOSED)
        return DataWriter::Null;
	JSONWriter& invocation = newDataWriter();
	invocation.writeString(name);
	return invocation;
}

DataWriter& WSWriter::writeResponse(UInt8 type) {
	 if (type == 0)
		 return writeMessage(); 
	if(state()==CLOSED)
        return DataWriter::Null;
	return newDataWriter(true);
}

DataWriter& WSWriter::writeMessage() {
	if(state()==CLOSED)
        return DataWriter::Null;
	return newDataWriter();
}



bool WSWriter::writeMedia(MediaType type,UInt32 time,PacketReader& packet,Parameters& properties) {
	if(state()==CLOSED)
		return true;
	switch(type) {
		case START:
			writeInvocation("__publishing").writeString(string((const char*)packet.current(),packet.available()));
			break;
		case STOP:
			writeInvocation("__unpublishing").writeString(string((const char*)packet.current(),packet.available()));
			break;
		case DATA: {
			JSONWriter& writer = newDataWriter();
			writer.packet.write8('[');
			writer.packet.writeRaw(packet.current(),packet.available());
			writer.packet.write8(']');
			break;
		}
		case INIT:
			break;
		default:
			return Writer::writeMedia(type,time,packet,properties);
	}
	return true;
}


} // namespace Mona
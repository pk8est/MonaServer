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

#include "Mona/Sessions.h"
#include "Mona/Session.h"
#include "Mona/Logs.h"

using namespace std;

namespace Mona {


Sessions::Sessions():_nextId(1) {
	onAddressChange = [this](Session& session, const SocketAddress& oldAddress) {
		INFO("Session ",session.name()," has changed its address, ",oldAddress.toString()," -> ",session.peer.address.toString());
		if (!(session._sessionsOptions&BYADDRESS))
			return;
		if (_sessionsByAddress.erase(oldAddress) == 0)
			WARN("Session ",session.name()," unfound with the address key ",oldAddress.toString());
		auto it = _sessionsByAddress.lower_bound(session.peer.address);
		if (it != _sessionsByAddress.end() && it->first == session.peer.address) {
			it->second->expire();
			it->second->kill();
			removeByPeer(*it->second);
			if (_sessions.erase(it->second->id()) == 0) {
				ERROR("Session ", session.name(), " unfound with the id key ", it->second->id());
				for (auto it2 = _sessions.begin(); it2 != _sessions.end();++it2) {
					if (it2->second == &session) {
						INFO("The correct key was ",it2->first);
						_sessions.erase(it2);
						break;
					}
				}
			}
			delete it->second;
			_sessionsByAddress.erase(it);
		}
		_sessionsByAddress.emplace_hint(it,session.peer.address,&session);
	};
}

Sessions::~Sessions() {
	// delete sessions
	_sessionsByAddress.clear();
	_sessionsByPeerId.clear();
	if (!_sessions.empty())
		WARN("sessions are deleting");
	Iterator it;
	for (it = begin(); it != end(); ++it) {
		it->second->expire();
		it->second->kill(Session::SERVER_DEATH);
		delete it->second;
	}
	_sessions.clear();
}

void Sessions::removeByPeer(Session& session) {
	if (session._sessionsOptions&BYPEER) {
		if (_sessionsByPeerId.erase(session.peer.id)==0) {
			string buffer;
 			ERROR("Session ",session.name()," deletion unfound in peer sessions collection with key ",Util::FormatHex(session.peer.id,ID_SIZE,buffer));
 			for (auto it = _sessionsByPeerId.begin(); it != _sessionsByPeerId.end();++it) {
 				if (it->second == &session) {
					INFO("The correct key was ",Util::FormatHex(it->first,ID_SIZE,buffer));
 					_sessionsByPeerId.erase(it);
					break;
				}
			}
		}
	}
}

void Sessions::remove(map<UInt32,Session*>::iterator it) {
	Session& session(*it->second);
	DEBUG("Session ",session.name()," died");
	removeByPeer(session);
	if (session._sessionsOptions&BYADDRESS) {
		if (_sessionsByAddress.erase(session.peer.address)==0) {
			ERROR("Session ",session.name()," deletion unfound in address sessions collection with key ",session.peer.address.toString());
			for (auto it = _sessionsByAddress.begin(); it != _sessionsByAddress.end();++it) {
				if (it->second == &session) {
					INFO("The correct key was ",it->first.toString());
					_sessionsByAddress.erase(it);
					break;
				}
			}
		}
	}
	session.expire();
	session.kill();
	delete &session;
	_sessions.erase(it);
}


void Sessions::manage() {
	auto it= _sessions.begin();
	while(it!=_sessions.end()) {
		if(!it->second->died)
			it->second->manage();
		if(!it->second->died)
			it->second->flush();
		if(it->second->died) {
			remove(it++);
			continue;
		}
		++it;
	}
}




} // namespace Mona
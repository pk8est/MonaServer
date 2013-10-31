/* 
	Copyright 2013 Mona - mathieu.poux[a]gmail.com
 
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

#pragma once

#include "Mona/Mona.h"
#include <istream>

namespace Mona {


class BinaryReader : virtual Object {
public:
	enum ByteOrder {
		BIG_ENDIAN,
		LITTLE_ENDIAN
	};
	BinaryReader(std::istream& istr,ByteOrder byteOrder=BIG_ENDIAN); // BIG_ENDIAN==NETWORK_ENDIAN

	UInt32			read7BitValue();
	UInt64			read7BitLongValue();
	UInt32			read7BitEncoded();
	UInt8*			readRaw(UInt8* value, UInt32 size) { _istr.read((char*)value, size); return value; }
	char*			readRaw(char* value, UInt32 size) { _istr.read(value, size); return value; }
	std::string&	readRaw(UInt32 size, std::string& value);
	std::string&	readString8(std::string& value) { return readRaw(read8(), value);}
	std::string&	readString16(std::string& value) { return readRaw(read16(), value); }
	UInt8			read8() { return (UInt8)_istr.get(); }
	UInt16			read16();
	UInt32			read24();
	UInt32			read32();
	UInt64			read64();

private:

	bool			_flipBytes;
	std::istream&   _istr;
};


} // namespace Mona

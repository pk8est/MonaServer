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
*/


#pragma once

#include "Mona/Mona.h"
#include "Mona/Buffer.h"
#include <openssl/dh.h>


namespace Mona {

#define DH_KEY_SIZE				0x80

class DiffieHellman : virtual Object {
public:
	DiffieHellman();
	virtual ~DiffieHellman();

	bool	initialize(Exception& ex,bool reset=false);
	int		publicKeySize(Exception& ex) { initialize(ex); return BN_num_bytes(_pDH->pub_key); }
	int		privateKeySize(Exception& ex) { initialize(ex);  return BN_num_bytes(_pDH->priv_key); }
	UInt8*	readPublicKey(Exception& ex, UInt8* pubKey) { initialize(ex); readKey(_pDH->pub_key, pubKey); return pubKey; }
	UInt8*	readPrivateKey(Exception& ex, UInt8* privKey) { initialize(ex);  readKey(_pDH->priv_key, privKey); return privKey; }
	Buffer<UInt8>&	computeSecret(Exception& ex, const Buffer<UInt8>& farPubKey, Buffer<UInt8>& sharedSecret);

private:
	void	readKey(BIGNUM *pKey, UInt8* key) { BN_bn2bin(pKey, key); }

	DH*			_pDH;
};


} // namespace Mona

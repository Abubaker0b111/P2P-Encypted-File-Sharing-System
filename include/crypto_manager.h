#ifndef CRYPTO_MANAGER_H
#define CRYPTO_MANAGER_H

#include <sodium.h>
#include "rudp_socket.h"

class CryptoManager {
private:
    // The keys live strictly in private memory
    unsigned char my_pk[crypto_kx_PUBLICKEYBYTES];//public key
    unsigned char my_sk[crypto_kx_SECRETKEYBYTES];//secret key
    unsigned char peer_pk[crypto_kx_PUBLICKEYBYTES];//peer's public key
    
    unsigned char rx_key[crypto_kx_SESSIONKEYBYTES];//receiver key
    unsigned char tx_key[crypto_kx_SESSIONKEYBYTES];//transmit key

public:
    CryptoManager();
    ~CryptoManager();

    // Boots up libsodium and generates the ephemeral keypair
    bool Initialize();

    // Uses the RUDP socket to securely swap keys and derive the session secrets
    bool PerformKeyExchange(RUDPSocket& reliable_channel, bool is_caller);

    // Accessors for the RUDP socket to grab the final session keys
    const unsigned char* GetTxKey() const;
    const unsigned char* GetRxKey() const;
};

#endif
#include "crypto_manager.h"
#include <iostream>
#include <cstring>

CryptoManager::CryptoManager() {
    // Ensure all memory is zeroed out upon creation
    memset(my_pk, 0, sizeof(my_pk));
    memset(my_sk, 0, sizeof(my_sk));
    memset(peer_pk, 0, sizeof(peer_pk));
    memset(rx_key, 0, sizeof(rx_key));
    memset(tx_key, 0, sizeof(tx_key));
}

CryptoManager::~CryptoManager() {
    //Wipe all secret keys from RAM before the object is destroyed
    sodium_memzero(my_sk, sizeof(my_sk));
    sodium_memzero(rx_key, sizeof(rx_key));
    sodium_memzero(tx_key, sizeof(tx_key));
}


//Initializing Libsodium
bool CryptoManager::Initialize() {
    if (sodium_init() < 0) {
        std::cerr << "[Crypto] Panic! Libsodium couldn't be initialized. Not safe to proceed." << std::endl;
        return false;
    }
    //std::cout << "[Crypto] Libsodium initialized successfully." << std::endl;
    
    // Generate the ephemeral keypair for this session right away
    crypto_kx_keypair(my_pk, my_sk);
    return true;
}

bool CryptoManager::PerformKeyExchange(RUDPSocket& reliable_channel, bool is_caller) {
    //std::cout << "\n[Crypto] Initiating Elliptic-Curve Key Exchange..." << std::endl;

    if (is_caller) {
        // Caller sends first, receives second
        //std::cout << "[Crypto] Sending my Public Key..." << std::endl;
        reliable_channel.Send((const char*)my_pk, crypto_kx_PUBLICKEYBYTES);//Sending our public key
        
        //std::cout << "[Crypto] Waiting for Peer's Public Key..." << std::endl;
        reliable_channel.Receive((char*)peer_pk, crypto_kx_PUBLICKEYBYTES);//Receiving the peers public key
        
        // Derive keys as Caller
        if (crypto_kx_client_session_keys(rx_key, tx_key, my_pk, my_sk, peer_pk) != 0) {
            std::cerr << "[FATAL] Suspicious peer public key. Math failed." << std::endl;
            return false;
        }
    } 
    else {
        // Listener receives first, sends second
        //std::cout << "[Crypto] Waiting for Peer's Public Key..." << std::endl;
        reliable_channel.Receive((char*)peer_pk, crypto_kx_PUBLICKEYBYTES);//Receive the senders public key
        
        //std::cout << "[Crypto] Sending my Public Key..." << std::endl;
        reliable_channel.Send((const char*)my_pk, crypto_kx_PUBLICKEYBYTES);//Send our own public key
        
        // Derive keys as Callee
        if (crypto_kx_server_session_keys(rx_key, tx_key, my_pk, my_sk, peer_pk) != 0) {
            std::cerr << "[FATAL] Suspicious peer public key. Math failed." << std::endl;
            return false;
        }
    }

    //std::cout << "[Crypto] SUCCESS! Secure Session Keys derived." << std::endl;
    
    
    // Wiping our secret key from the memory
    sodium_memzero(my_sk, sizeof(my_sk));
    
    return true;
}

const unsigned char* CryptoManager::GetTxKey() const {//Get transmit key
    return tx_key;
}

const unsigned char* CryptoManager::GetRxKey() const {//Get receive Key
    return rx_key;
}
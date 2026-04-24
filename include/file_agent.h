#ifndef FILE_AGENT_H
#define FILE_AGENT_H

#include <string>
#include "rudp_socket.h"

class FileAgent {
private:
    const int CHUNK_SIZE = 900; // Safe limit for our encrypted payloads

public:
    FileAgent();
    ~FileAgent();

    // Reads a file from disk and streams it over the encrypted channel
    bool SendFile(RUDPSocket& reliable_channel, const std::string& filepath);
    
    // Listens for incoming metadata and continuously writes chunks to disk
    bool ReceiveFile(RUDPSocket& reliable_channel);
};

#endif
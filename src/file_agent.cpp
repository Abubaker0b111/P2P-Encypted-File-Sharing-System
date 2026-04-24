#include "file_agent.h"
#include <iostream>
#include <fstream>
#include <string>

FileAgent::FileAgent() {}
FileAgent::~FileAgent() {}

bool FileAgent::SendFile(RUDPSocket& reliable_channel, const std::string& filepath) {
    //Open the file in binary mode
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    
    if (!file.is_open()) {
        std::cerr << "[FileAgent] Error: Could not open file: " << filepath << std::endl;
        return false;
    }

    //Extract the file size
    std::streamsize file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::cout << "\n[FileAgent] Initiating transfer of: " << filepath << std::endl;
    std::cout << "[FileAgent] Size: " << file_size << " bytes" << std::endl;

    //Send Metadata Packet ("META:filename:filesize")
    std::string metadata = "META:" + filepath + ":" + std::to_string(file_size);
    reliable_channel.Send(metadata.c_str(), metadata.length());

    //The Chunking Loop
    char chunk_buffer[CHUNK_SIZE];
    long bytes_sent = 0;
    int chunk_count = 1;

    std::cout << "[FileAgent] Streaming encrypted chunks to peer..." << std::endl;

    //Reads the file 1 chunk at a time and transmits it
    //Waits for acknowledgement before proceeding to next chunk 
    while (file.read(chunk_buffer, CHUNK_SIZE) || file.gcount() > 0) {
        size_t bytes_to_send = file.gcount(); 
        
        reliable_channel.Send(chunk_buffer, bytes_to_send);
        bytes_sent += bytes_to_send;
        
        if (chunk_count % 10 == 0) {
            std::cout << "  -> Sent " << bytes_sent << " / " << file_size << " bytes..." << std::endl;
        }
        chunk_count++;
    }

    std::cout << "[FileAgent] Transfer complete! Sent " << chunk_count - 1 << " total chunks." << std::endl;
    file.close();
    return true;
}

bool FileAgent::ReceiveFile(RUDPSocket& reliable_channel) {
    char chunk_buffer[1024];
    std::cout << "[FileAgent] Waiting for incoming File Metadata..." << std::endl;
    
    //Receive and Parse Metadata
    int bytes = reliable_channel.Receive(chunk_buffer, sizeof(chunk_buffer));
    
    if (bytes <= 0) return false;
    
    chunk_buffer[bytes] = '\0';
    std::string meta_str(chunk_buffer);
    
    if (meta_str.find("META:") != 0) {
        std::cerr << "[FileAgent] Error: Expected META packet, received unknown data." << std::endl;
        return false;
    }

    size_t first_colon = meta_str.find(':');
    size_t second_colon = meta_str.find(':', first_colon + 1);
    
    std::string filename = meta_str.substr(first_colon + 1, second_colon - first_colon - 1);
    long file_size = std::stol(meta_str.substr(second_colon + 1));
    
    std::string save_path = "received_" + filename;
    
    std::cout << "\n[FileAgent] Incoming File Detected!" << std::endl;
    std::cout << "[FileAgent] Name: " << filename << std::endl;
    std::cout << "[FileAgent] Size: " << file_size << " bytes" << std::endl;
    
    //Open output file in binary mode
    std::ofstream outfile(save_path, std::ios::binary);
    if (!outfile.is_open()) {
        std::cerr << "[FileAgent] Error: Could not create file on disk." << std::endl;
        return false;
    }
    
    long total_received = 0;
    int chunks_received = 0;
    
    std::cout << "[FileAgent] Receiving and decrypting data stream..." << std::endl;
    
    //The Reassembly Loop
    while (total_received < file_size) {
        int chunk_len = reliable_channel.Receive(chunk_buffer, sizeof(chunk_buffer));
        //Similarly reassembles the file 1 chunk at a time
        if (chunk_len > 0) {
            outfile.write(chunk_buffer, chunk_len);
            total_received += chunk_len;
            chunks_received++;
            
            if (chunks_received % 10 == 0) {
                std::cout << "  -> Received " << total_received << " / " << file_size << " bytes..." << std::endl;
            }
        }
    }
    
    std::cout << "[FileAgent] Reassembly complete! Saved " << chunks_received << " chunks to: " << save_path << std::endl;
    outfile.close();
    return true;
}
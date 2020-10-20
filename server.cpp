//
//  main.cpp
//  Project2
//
//  Created by Alexander Ma on 5/4/20.
//  Copyright © 2020 Alexander Ma. All rights reserved.
//

#include <errno.h>
#include <string.h>
#include <string>
#include <vector>
#include <chrono>
#include <tuple>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>

//#include <time.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "header.h"

class Reset{
public:
    Reset(bool& syn, bool& syn_waiting, bool& synack, bool& fin, bool& fin_waiting, bool& finack, int& file_fd, std::vector<std::pair<int,int>>& dup_counter, int& file_size){
        m_syn = &syn;
        m_syn_waiting = &syn_waiting;
        m_synack = &synack;
        m_fin = &fin;
        m_fin_waiting = &fin_waiting;
        m_finack = &finack;
        m_file_fd = &file_fd;
        m_dup_counter = &dup_counter;
        m_file_size = &file_size;
    }
    void reset(){
        *m_syn = false;
        *m_syn_waiting = false;
        *m_synack = false;
        *m_fin = false;
        *m_fin_waiting = false;
        *m_finack = true;
        if(*m_file_fd != -1){
            int output = close(*m_file_fd);
            if(output == -1){
                fprintf(stderr, "%s\n", strerror(errno));
            }
        }
        *m_file_fd = -1;
        (*m_dup_counter).clear();
        *m_file_size = 0;
    }
private:
    bool* m_syn;
    bool* m_syn_waiting;
    bool* m_synack;
    bool* m_fin;
    bool* m_fin_waiting;
    bool* m_finack;
    int* m_file_fd;
    std::vector<std::pair<int,int>>* m_dup_counter;
    int* m_file_size;
};

int main(int argc, const char * argv[]) {
    fprintf(stderr, "Hello\n");
    if (argc != 2){
        fprintf(stderr, "Usage: ./server <PORT>\n");
        return 1;
    }
    int port = atoi(argv[1]);
    int m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if(m_socket == -1){
        fprintf(stderr, "%s\n", strerror(errno));
        return 1;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if(addr.sin_addr.s_addr == -1){
        return 1;
    }
    long output = bind(m_socket, (struct sockaddr*) &addr, sizeof(addr));
    if(output == -1){
        fprintf(stderr, "%s\n", strerror(errno));
        return 1;
    }
    
    fcntl(m_socket, F_SETFL, fcntl(m_socket, F_GETFL)|O_NONBLOCK);
    
    char receiveBuffer[1000];
    char sendBuffer[1000];
    char* fileBuffer = (char*)malloc(10000000);
    
    std::vector<std::tuple<long,int,Header,char*>> m_times;
    //time (milliseconds), ack number, Header, message (dynamically allocated)
    
    std::chrono::steady_clock m_timer;
    //std::chrono::duration_cast<std::chrono::milliseconds>(m_timer.now().time_since_epoch()).count()
    
    std::vector<std::pair<int,int>> dup_counter;
    //ACK Number, Count
    
    struct sockaddr_in client_addr;
    memset(&client_addr, 0, sizeof(client_addr));
    socklen_t addr_len = sizeof(client_addr);
    
    unsigned long long connection_order = 0;
    int file_fd = -1;
    int file_size = 0;
    
    bool syn = false;
    bool syn_waiting = false;
    int syn_ack_num = 0;
    bool synack = false;
    
    bool fin = false;
    bool fin_waiting = false;
    int fin_ack_num = 0;
    bool finack = true;
    
    std::srand(time(NULL));
    int sequence_num = std::rand()%25601;
    int ack_num;
    
    Reset reset = Reset(syn, syn_waiting, synack, fin, fin_waiting, finack, file_fd, dup_counter, file_size);
    
    while(true){
        fflush(stdout);
        if(!m_times.empty()){
            std::tuple<long,int,Header,char*> first = m_times.front();
            //first is the oldest
            long stored_time = std::get<0>(first);
            long current_time = std::chrono::duration_cast<std::chrono::milliseconds>(m_timer.now().time_since_epoch()).count();
            if(current_time - stored_time > 500){
                //resend the message
                Header header = std::get<2>(first);
                fprintf(stdout, "TIMEOUT %d\n", header.get_sequence_num());
                output = sendto(m_socket, std::get<3>(first), header.get_data_size()+12, 0, (struct sockaddr*)&client_addr, addr_len);
                if(output == -1){
                    fprintf(stderr, "%s\n", strerror(errno));
                    //Reset
                    reset.reset();
                    continue;
                }
                std::string debug = "RESEND %d %d";
                if(header.is_syn()){
                    debug += " SYN";
                }
                if(header.is_fin()){
                    debug += " FIN";
                }
                if(header.is_ack()){
                    debug += " ACK";
                }
                debug += "\n";
                fprintf(stdout, debug.c_str(), header.get_sequence_num(), header.get_ack_num());
                current_time = std::chrono::duration_cast<std::chrono::milliseconds>(m_timer.now().time_since_epoch()).count();
                m_times.erase(m_times.begin());
                m_times.push_back(std::tuple<long,int,Header,char*>(current_time, std::get<1>(first),std::get<2>(first),std::get<3>(first)));
            }
            //does not use infinite loop to avoid locking self in endless loop if too many messages
            //socket is nonblocking, so this will still happen pretty often
            //at least one timeout for every read
        }
        struct sockaddr_in temp_addr;
        memset(&temp_addr, 0, sizeof(temp_addr));
        output = recvfrom(m_socket, receiveBuffer, 999, 0, (struct sockaddr*) &temp_addr, &addr_len);
        if(output == -1 && errno == EAGAIN){
            //no message received
            continue;
        }
        if(output == -1){
            fprintf(stderr, "%s\n", strerror(errno));
            return 1;
        }
        
        //Packet size: 524 bytes
        //Header size: 12 bytes
        
        //Required fields: 5 bytes
        //1 bit padding
        //sequence number: 15 bits (max 25600)
        //1 bits padding
        //ACK number: 15 bits (same as sequence number)
        //ACK flag: 1 bit
        //SYN flag: 1 bit
        //FIN flag: 1 bit
        //5 bits padding
        
        //Optional fields: 4.25 bytes
        //Offset: 24 bits (max 10000000)
        //Data size: 10 bits (max 524) <-- 12 more than 512
        
        //Two bytes of padding
        
        //Data size: 512 bytes
        
        Header in_header = parseHeader(receiveBuffer);
        std::string debug = "RECV %d %d";
        if(in_header.is_syn()){
            debug += " SYN";
        }
        if(in_header.is_fin()){
            debug += " FIN";
        }
        if(in_header.is_ack()){
            debug += " ACK";
        }
        debug += "\n";
        fprintf(stdout, debug.c_str(), in_header.get_sequence_num(), in_header.get_ack_num());
        
        Header out_header;
        
        long sendLength = 0;
        
        if(finack){
            //last connection finished
            if(in_header.is_syn() && !syn_waiting){
                //client starts opening connection
                client_addr = temp_addr;
                
                out_header.initialize(sequence_num, in_header.get_sequence_num()+1, true, true, false, 0, 0);
                
                char* headerstring = out_header.make_header();
                memcpy(sendBuffer, headerstring, 12);
                free(headerstring);
                
                sendLength = 12;
                
                sequence_num = out_header.get_sequence_num();
                syn_ack_num = out_header.get_sequence_num()+1;
                ack_num = syn_ack_num;
                if(syn_ack_num > 25600){
                    syn_ack_num = 0;
                    ack_num = 0;
                }
                syn_waiting = true;
            } else if(in_header.is_ack() && syn_waiting && in_header.get_ack_num() == syn_ack_num){
                //handshake complete
                synack = true;
                syn_waiting = false;
                finack = false;
                
                for(int i = 0; i < m_times.size(); i++){
                    if(in_header.get_ack_num() == std::get<1>(m_times[i])){
                        free(std::get<3>(m_times[i]));
                        m_times.erase(m_times.begin()+i);
                        break;
                    }
                }
                
                connection_order++;
                std::string name = std::to_string(connection_order) + ".file";
                file_fd = open(name.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0777);
                if(file_fd == -1){
                    fprintf(stderr, "%s\n", strerror(errno));
                    return 1;
                }
            } else {
                continue;
            }
        } else {
            //connection still open
            if(client_addr.sin_addr.s_addr != temp_addr.sin_addr.s_addr){
                continue; //ignore packets from others
            }
        }
        
        bool pure_ack = false;
        if(synack){
            //handshake complete, read data
            //check for fin
            if(in_header.is_fin() && !fin_waiting){
                fin_waiting = true;
                
                //Send ack separately
                out_header.initialize(sequence_num, in_header.get_sequence_num()+1, true, false, false, 0, 0);
                char* headerstring0 = out_header.make_header();
                memcpy(sendBuffer, headerstring0, 12);
                free(headerstring0);
                fprintf(stdout, "SEND %d %d ACK\n", out_header.get_sequence_num(), out_header.get_ack_num());
                if(dup_counter.size() == 20){
                    dup_counter.erase(dup_counter.begin());
                }
                dup_counter.push_back(std::pair<int,int>(out_header.get_ack_num(), 1));
                sendto(m_socket, sendBuffer, 12, 0, (struct sockaddr*)&client_addr, addr_len);
                
                //Send fin
                sequence_num++;
                if(sequence_num > 25600){
                    sequence_num = 0;
                }
                out_header.initialize(sequence_num, 0, false, false, true, 0, 0);
                
                char* headerstring = out_header.make_header();
                memcpy(sendBuffer, headerstring, 12);
                free(headerstring);
                
                sendLength = 12;
                
                fin_ack_num = out_header.get_sequence_num()+1;
                if(fin_ack_num > 25600){
                    fin_ack_num = 0;
                }
            } else if (fin_waiting){
                if(in_header.is_ack() && in_header.get_ack_num() == fin_ack_num){
                    for(int i = 0; i < m_times.size(); i++){
                        if(in_header.get_ack_num() == std::get<1>(m_times[i])){
                            free(std::get<3>(m_times[i]));
                            m_times.erase(m_times.begin()+i);
                            break;
                        }
                    }
                    output = write(file_fd, fileBuffer, file_size);
                    if(output == -1){
                        fprintf(stderr, "%s\n", strerror(errno));
                    }
                    if(output < file_size){
                        fprintf(stderr, "Num Bytes: %ld less than file size %d\n", output, file_size);
                    }
                    reset.reset();
                    continue;
                }
            } else {
                pure_ack = true;
                if(in_header.is_ack()){
                    for(int i = 0; i < m_times.size(); i++){
                        if(in_header.get_ack_num() == std::get<1>(m_times[i])){
                            free(std::get<3>(m_times[i]));
                            m_times.erase(m_times.begin()+i);
                            break;
                        }
                    }
                }
                int temp = in_header.get_sequence_num()+in_header.get_data_size();
                if(temp > 25600){
                    temp = temp-25600;
                }
                
                //read data
                memcpy(fileBuffer+in_header.get_offset(), receiveBuffer+12, in_header.get_data_size());
                int size = in_header.get_offset()+in_header.get_data_size();
                if (size > file_size){
                    file_size = size;
                }
                //send an ack
                out_header.initialize(sequence_num, temp, true, false, false, 0, 0);
                
                char* headerstring = out_header.make_header();
                memcpy(sendBuffer, headerstring, 12);
                free(headerstring);
                
                sendLength = 12;
                
                ack_num = out_header.get_ack_num();
                if(ack_num > 25600){
                    ack_num = 0;
                }
            }
        }
        
        if(!out_header.is_initialized()){
            continue;
        }
        
        if(out_header.is_ack()){
            int i = 0;
            for(i = 0; i < dup_counter.size(); i++){
                if(dup_counter[i].first == out_header.get_ack_num()){
                    dup_counter[i].second++;
                    break;
                }
            }
            if(i == dup_counter.size()){
                if(dup_counter.size() < 20){
                    dup_counter.push_back(std::pair<int,int>(out_header.get_ack_num(),1));
                }
                else{
                    dup_counter.erase(dup_counter.begin());
                    dup_counter.push_back(std::pair<int,int>(out_header.get_ack_num(),1));
                }
            }
        }
        
        output = sendto(m_socket, sendBuffer, sendLength, 0, (struct sockaddr*)&client_addr, addr_len);
        if(output == -1){
            fprintf(stderr, "%s\n", strerror(errno));
            //RESET
            reset.reset();
            continue;
        }
        
        std::string log = "SEND %d %d";
        if(out_header.is_syn()){
            log += " SYN";
        }
        if(out_header.is_fin()){
            log += " FIN";
        }
        if(out_header.is_ack()){
            int i = 0;
            for(i = 0; i < dup_counter.size(); i++){
                if(dup_counter[i].first == out_header.get_ack_num() && dup_counter[i].second > 1){
                    log += " DUP-ACK";
                    break;
                }
            }
            if(i == dup_counter.size()){
                log += " ACK";
            }
        }
        log += "\n";
        fprintf(stdout, log.c_str(), out_header.get_sequence_num(), out_header.get_ack_num());
        
        if(!pure_ack){
            //do not resend acks on timeout
            long time = std::chrono::duration_cast<std::chrono::milliseconds>(m_timer.now().time_since_epoch()).count();
            char* message = (char*)malloc(sendLength);
            memcpy(message,sendBuffer,sendLength);
            int expected_ack = out_header.get_sequence_num();
            if(out_header.get_data_size() > 0){
                expected_ack += out_header.get_data_size();
            } else {
                expected_ack++;
            }
            m_times.push_back(std::tuple<long, int, Header, char*>(time, expected_ack, out_header, message));
        }
    }
    return 0;
}

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

int main(int argc, const char * argv[]) {
    fprintf(stderr, "Hello\n");
    if (argc != 4){
        fprintf(stderr, "Usage: ./client <HOSTNAME-OR-IP> <PORT> <FILENAME>\n");
        return 1;
    }
    const char* hostname = argv[1];
    int port = atoi(argv[2]);
    const char* filename = argv[3];
    int m_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if(m_socket == -1){
        fprintf(stderr, "%s\n", strerror(errno));
        return 1;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    struct hostent* host = gethostbyname(hostname);
    if(host != NULL){
        server_addr.sin_addr.s_addr = *(unsigned int*)(host->h_addr_list[0]);
    } else{
        fprintf(stderr, "%s\n", strerror(errno));
        return 1;
    }
    if(server_addr.sin_addr.s_addr == -1){
        fprintf(stderr, "%s\n", strerror(errno));
        return 1;
    }
    socklen_t addr_len = sizeof(server_addr);
    
    struct sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(0);
    client_addr.sin_addr.s_addr = INADDR_ANY;
    
    long output = bind(m_socket, (struct sockaddr*) &client_addr, addr_len);
    if(output == -1){
        fprintf(stderr, "%s\n", strerror(errno));
        return 1;
    }
    
    fcntl(m_socket, F_SETFL, fcntl(m_socket, F_GETFL)|O_NONBLOCK);
    
    char receiveBuffer[1000];
    char sendBuffer[1000];
    char* fileBuffer = (char*)malloc(10000000);
    
    std::vector<char*> fileParts;
    int file_fd = open(filename, O_RDONLY);
    output = read(file_fd, fileBuffer, 10000000);
    if(output == -1){
        fprintf(stderr, "%s\n", strerror(errno));
        return 1;
    }
    close(file_fd);
    long count = 0;
    while(output >= 512){
        fileParts.push_back(fileBuffer+count);
        count+=512;
        output-=512;
    }
    fileParts.push_back(fileBuffer+count);
    count = output;
    long offset_counter = 0;
    
    std::vector<std::tuple<long,int,Header,char*, bool>> m_times;
    //time (milliseconds), ack number, Header, message (dynamically allocated), acked
    
    int dup_counter[25601];
    memset(dup_counter, 0, 25601*sizeof(int));
    
    std::chrono::steady_clock m_timer;
    //std::chrono::duration_cast<std::chrono::milliseconds>(m_timer.now().time_since_epoch()).count()
        
    bool syn = false;
    bool syn_waiting = false;
    int syn_ack_num = 0;
    bool synack = false;
    
    bool fin = false;
    bool fin_waiting = false;
    int fin_ack_num = 0;
    bool finack = false;
    long endtime;
    
    std::srand(time(NULL));
    int sequence_num = std::rand()%25601;
    int ack_num = 0;
        
    while(true){
        fflush(stdout);
        if(finack){
            long current_time = std::chrono::duration_cast<std::chrono::milliseconds>(m_timer.now().time_since_epoch()).count();
            if(current_time - endtime > 2000){
                break;
            }
        }
        
        if(!m_times.empty()){
            int size = m_times.size();
            for(int i = 0; i < size; i++){
                std::tuple<long,int,Header,char*, bool> current = m_times[i];
                //first is the oldest
                long stored_time = std::get<0>(current);
                long current_time = std::chrono::duration_cast<std::chrono::milliseconds>(m_timer.now().time_since_epoch()).count();
                if(!std::get<4>(current) && current_time - stored_time > 500){
                    //resend the message
                    Header header = std::get<2>(current);
                    fprintf(stdout, "TIMEOUT %d\n", header.get_sequence_num());
                    output = sendto(m_socket, std::get<3>(current), header.get_data_size()+12, 0, (struct sockaddr*)&server_addr, addr_len);
                    if(output == -1){
                        fprintf(stderr, "%s\n", strerror(errno));
                        return 1;
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
                    m_times[i] = std::tuple<long,int,Header,char*,bool>(current_time, std::get<1>(current), std::get<2>(current), std::get<3>(current), false);
                }
            }
            //does not use infinite loop to avoid locking self in endless loop if too many messages
            //socket is nonblocking, so this will still happen pretty often
            //at least one timeout for every read
        }
        
        struct sockaddr_in temp_addr;
        memset(&temp_addr, 0, sizeof(temp_addr));
        Header in_header;
        output = recvfrom(m_socket, receiveBuffer, 999, 0, (struct sockaddr*) &temp_addr, &addr_len);
        if(output == -1){
            if(errno != EAGAIN){
                fprintf(stderr, "%s\n", strerror(errno));
                return 1;
            }
        } else if(temp_addr.sin_addr.s_addr != server_addr.sin_addr.s_addr || temp_addr.sin_port != server_addr.sin_port){
            //do not use incoming data
        } else if(output >= 12){
            in_header = parseHeader(receiveBuffer);
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
        
        Header out_header;
        
        long sendLength = 0;
        
        bool pure_ack = false;
        
        if(in_header.is_initialized() && in_header.is_ack()){
            while(m_times.size() > 0){
                std::tuple<long,int,Header,char*,bool> first = m_times[0];
                if(std::get<1>(first) == in_header.get_ack_num() || std::get<4>(first)){
                    free(std::get<3>(first));
                    m_times.erase(m_times.begin());
                } else {
                    break;
                }
            }
            int i = 1;
            int size = m_times.size();
            for(i = 1; i < size; i++){
                if(std::get<1>(m_times[i]) == in_header.get_ack_num()){
                    std::tuple<long,int,Header,char*, bool> current = m_times[i];
                    m_times[i] = std::tuple<long,int,Header,char*,bool>(std::get<0>(current),std::get<1>(current),std::get<2>(current),std::get<3>(current),true);
                    break;
                }
            }
        }
        
        if(!synack){
            //send connection request
            if(!syn_waiting){
                out_header.initialize(sequence_num, 0, false, true, false, 0, 0);
                
                char* headerstring = out_header.make_header();
                memcpy(sendBuffer, headerstring, 12);
                free(headerstring);
                sendLength = 12;
                
                syn_waiting = true;
                sequence_num++;
                if(sequence_num > 25600){
                    sequence_num = 0;
                }
                ack_num = sequence_num;
                syn_ack_num = out_header.get_sequence_num()+1;
            }
            if(syn_waiting){
                if(in_header.is_initialized() && in_header.is_syn() && in_header.is_ack() && in_header.get_ack_num() == syn_ack_num){
                    syn_waiting = false;
                    synack = true;
                }
            }
        }
        if(synack && !fileParts.empty() && m_times.size() < 10){
            int data_size = 512;
            if(fileParts.size() == 1){
                data_size = count;
            }
            if(in_header.is_initialized()){
                if(in_header.is_syn()){
                    int temp = in_header.get_sequence_num()+1;
                    if(temp > 25600){
                        temp = 0;
                    }
                    out_header.initialize(sequence_num, temp, true, false, false, offset_counter, data_size);
                }
                else
                    out_header.initialize(sequence_num, 0, false, false, false, offset_counter, data_size);
            } else {
                out_header.initialize(sequence_num, 0, false, false, false, offset_counter, data_size);
            }
            char* headerstring = out_header.make_header();
            memcpy(sendBuffer, headerstring, 12);
            free(headerstring);
            memcpy(sendBuffer+12, fileParts.front(), data_size);
            sendLength = 12+data_size;
            
            sequence_num += data_size;
            if(sequence_num > 25600){
                sequence_num -= 25600;
            }
            ack_num = sequence_num;
            offset_counter += data_size;
            fileParts.erase(fileParts.begin());
        } else if(!fin_waiting && !finack && fileParts.empty() && m_times.empty()){
            //send fin if all acked
            if(in_header.is_initialized()){
                if(in_header.is_syn()){
                    int temp = in_header.get_sequence_num()+1;
                    if(temp > 25600){
                        temp = 0;
                    }
                    out_header.initialize(sequence_num, temp, false, false, true, 0, 0);
                } else {
                    out_header.initialize(sequence_num, 0, false, false, true, 0, 0);
                }
            } else {
                out_header.initialize(sequence_num, 0, false, false, true, 0, 0);
            }
            char* headerstring = out_header.make_header();
            memcpy(sendBuffer, headerstring, 12);
            free(headerstring);
            sendLength = 12;
            
            sequence_num++;
            if(sequence_num > 25600){
                sequence_num = 0;
            }
            ack_num = sequence_num;
            fin_waiting = true;
            fin_ack_num = out_header.get_sequence_num()+1;
        } else if(fin_waiting){
            if(in_header.is_initialized() && in_header.is_fin()){
                fin_waiting = false;
                int temp = in_header.get_sequence_num()+1;
                if(temp > 25600){
                    temp = 0;
                }
                pure_ack = true;
                out_header.initialize(sequence_num, temp, true, false, false, 0, 0);
                char* headerstring = out_header.make_header();
                memcpy(sendBuffer, headerstring, 12);
                free(headerstring);
                sendLength = 12;
                
                endtime = std::chrono::duration_cast<std::chrono::milliseconds>(m_timer.now().time_since_epoch()).count();
                finack = true;
            }
        } else {
            if(in_header.is_initialized()){
                //pure ack of repeated packets
                pure_ack = true;
                if(in_header.is_fin() || in_header.is_syn()){
                    int temp = in_header.get_sequence_num()+1;
                    if(temp > 25600){
                        temp = 0;
                    }
                    out_header.initialize(sequence_num, temp, true, false, false, 0, 0);
                    char* headerstring = out_header.make_header();
                    memcpy(sendBuffer, headerstring, 12);
                    free(headerstring);
                    sendLength = 12;
                }
            }
        }
        
        if(!out_header.is_initialized()){//no message to send
            continue;
        }
        
        if(out_header.is_ack()){
            dup_counter[out_header.get_ack_num()]++;
        }
        
        output = sendto(m_socket, sendBuffer, sendLength, 0, (struct sockaddr*)&server_addr, addr_len);
        if(output == -1){
            fprintf(stderr, "%s\n", strerror(errno));
            return 1;
        }
        std::string log = "SEND %d %d";
        if(out_header.is_syn()){
            log += " SYN";
        }
        if(out_header.is_fin()){
            log += " FIN";
        }
        if(out_header.is_ack()){
            if(dup_counter[out_header.get_ack_num()] > 1){
                log += " DUP-ACK";
            } else {
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
                if(expected_ack > 25600){
                    expected_ack -= 25600;
                }
            } else {
                expected_ack++;
                if(expected_ack > 25600){
                    expected_ack = 0;
                }
            }
            m_times.push_back(std::tuple<long, int, Header, char*, bool>(time, expected_ack, out_header, message, false));
        }
    }
    return 0;
}

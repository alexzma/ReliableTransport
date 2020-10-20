//
//  Header.h
//  Project2Server
//
//  Created by Alexander Ma on 5/9/20.
//  Copyright © 2020 Alexander Ma. All rights reserved.
//

#ifndef Header_h
#define Header_h

class Header{
public:
    Header(){
        initialized = false;
    }
    Header(int sequence_num, int ack_num, bool ack, bool syn, bool fin, long offset, int data_size){
        this->sequence_num = sequence_num;
        this->ack_num = ack_num;
        this->ack = ack;
        this->syn = syn;
        this->fin = fin;
        this->offset = offset;
        this->data_size = data_size;
        initialized = true;
    }
    void initialize(int sequence_num, int ack_num, bool ack, bool syn, bool fin, long offset, int data_size){
        this->sequence_num = sequence_num;
        this->ack_num = ack_num;
        this->ack = ack;
        this->syn = syn;
        this->fin = fin;
        this->offset = offset;
        this->data_size = data_size;
        initialized = true;
    }
    int get_sequence_num(){
        return sequence_num;
    }
    int get_ack_num(){
        return ack_num;
    }
    bool is_ack(){
        return ack;
    }
    bool is_syn(){
        return syn;
    }
    bool is_fin(){
        return fin;
    }
    long get_offset(){
        return offset;
    }
    int get_data_size(){
        return data_size;
    }
    bool is_initialized(){
        return initialized;
    }
    char* make_header(){
        char* header = (char*)malloc(12);
        header[0] = (sequence_num & 0xff00) >> 8;
        header[1] = sequence_num & 0x00ff;
        
        header[2] = (ack_num & 0xff00) >> 8;
        header[3] = ack_num & 0x00ff;
        
        char flags = 0x00;
        if(ack){
            flags = flags | 0x80;
        }
        if(syn){
            flags = flags | 0x40;
        }
        if(fin){
            flags = flags | 0x20;
        }
        
        header[4] = flags;
        
        //no offset
        header[5] = (offset & 0xff0000) >> 16;
        header[6] = (offset & 0x00ff00) >> 8;
        header[7] = offset & 0x0000ff;
        //no data
        header[8] = (data_size & 0xff00) >> 8;
        header[9] = data_size & 0x00ff;
        //buffer
        header[10] = 0;
        header[11] = 0;
        return header;
    }
private:
    int sequence_num;
    int ack_num;
    bool ack;
    bool syn;
    bool fin;
    long offset;
    int data_size;
    
    bool initialized;
};

static Header parseHeader(char* input){
    int sequence_num = (0x00ff & input[0])*256+(0x00ff & input[1]);
    int ack_num = (0x00ff & input[2])*256+(0x00ff & input[3]);
    
    bool ack = (input[4] & 0x80);
    bool syn = (input[4] & 0x40);
    bool fin = (input[4] & 0x20);
    
    long offset = (0x0000ff & input[5])*65536+(0x0000ff & input[6])*256+(0x0000ff & input[7]);
    int data_size = (0x00ff & input[8])*256+(0x00ff & input[9]);
    Header header = Header(sequence_num, ack_num, ack, syn, fin, offset, data_size);
    return header;
}

#endif /* Header_h */

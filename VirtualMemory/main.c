//
//  main.c
//  cs143proj2
//
//  Created by Xiang Jiang on 2017/11/24.
//  Copyright © 2017 Xiang Jiang. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define TLB 0
#define LIMIT 500

FILE* init_file;
FILE* va_file;

int PM[512*1024] = {0}; //1024 frames * 512 int per frame = 524288

int * ST = PM; //set the segment table started at PM[0], ST[s] s !> 511 since ST only took frame 0
unsigned int bitmap[32] = {0}; //each int holds 32bit
unsigned int MASK[32];
unsigned int MASK2[32];
unsigned int s_mask, p_mask, w_mask;

int TLB_table[4][4];




int initial_MASK()
{
    MASK[31] = 1;
    for(int i = 30; i >= 0; i--)
        MASK[i] = MASK[i+1] << 1;
    
    for(int i = 0; i < 32; i++)
        MASK2[i] = ~MASK[i];
    return 0;
}

int initial_PM(const char * init_file_name)
{
    //initial_file
    if((init_file = fopen(init_file_name, "r")) == NULL)
    {
        printf("Init Fail open\n");
        return 1;
    }
    
    char init_ST[LIMIT] = {0};
    char init_PT[LIMIT] = {0};
    
    fgets(init_ST, LIMIT, init_file);
    fgets(init_PT, LIMIT, init_file);
    
    //initialize ST
    char * temp_index = strtok(init_ST, " \n");
    char * temp_value = strtok(NULL, " \n");
    while(temp_index != NULL && temp_value != NULL)
    {
        int start_address = atoi(temp_value);
        ST[atoi(temp_index)] = start_address;
        
        if(start_address > 0)
        {
            bitmap[(start_address/512)/32] = bitmap[(start_address/512)/32] | MASK[(start_address/512)%32];
            bitmap[(start_address/512 + 1)/32] = bitmap[(start_address/512)/32] | MASK[(start_address/512 + 1)%32];
        }
        temp_index = strtok(NULL, " \n");
        temp_value = strtok(NULL, " \n");
    }
    
    //initialize PT
    char * page_index = strtok(init_PT, " \n");
    char * st_index = strtok(NULL, " \n");
    char * page_address = strtok(NULL, " \n");
    while(page_index && st_index && page_address)
    {
        if(ST[atoi(st_index)] > 0)
           PM[ST[atoi(st_index)] + atoi(page_index)] = atoi(page_address);
        
        page_index = strtok(NULL, " \n");
        st_index = strtok(NULL, " \n");
        page_address = strtok(NULL, " \n");
    }
    return 0;
}

int TLB_function(unsigned int s, unsigned int p, unsigned int w) //[priority][s][p][PA]
{
    if(ST[s] == -1)
    {
        printf("m ");
        return -1;
    }
    
    for(int i = 0; i < 4; i++)
    {
        if(TLB_table[i][1] == s && TLB_table[i][2] == p && TLB_table[i][3])
        {
            if(TLB_table[i][0] != 3)
            {
                for(int j = 0; j < 4; j++)
                    if(TLB_table[j][0] > TLB_table[i][0])
                        TLB_table[j][0]--;
                TLB_table[i][0] = 3;
            }
            printf("h ");
            return 1;
        }
    }
    
    int index = 0;
    for(int i = 0; i < 4; i++)
    {
        if(TLB_table[i][0]) //when they has priority
            TLB_table[i][0]--;
        else
            index = i; // the one priority is 0
    }
    TLB_table[index][0] = 3;
    TLB_table[index][1] = s;
    TLB_table[index][2] = p;
    TLB_table[index][3] = PM[ST[s] + p];
    printf("m ");
    return 0;
}

int find_free_page()
{
    for(int i = 0; i < 32; i++)
        for(int j = 0; j < 32; j++) //find a free page by searching bitmap
        {
            int test = bitmap[i] & MASK[j];
            if(test == 0)
            {
                bitmap[i] = bitmap[i] | MASK[j]; //set the page to 1
                return i*32 + j;
            }
        }
    return 0;
}

int find_free_pt()
{
    int found_pre = 0;
    for(int i = 0; i < 32; i++)
        for(int j = 0; j < 32; j++) //find a free pt by searching bitmap
        {
            int test = bitmap[i] & MASK[j];
            if(test == 0)
            {
                if(found_pre && j != 0)
                {
                    bitmap[i] = bitmap[i] | MASK[j-1]; //set the pt to 1;
                    bitmap[i] = bitmap[i] | MASK[j]; //set the pt to 1
                    return i*32 + j-1;
                }
                else
                {
                    found_pre = 1;
                }
            }
            else
                found_pre = 0;
        }
    return 0;
}

int read_access(unsigned int s, unsigned int p, unsigned int w)
{
    if(ST[s] == -1 || PM[ST[s]+p] == -1)
    {
        printf("pf ");
        return -1;
    }
    else if(ST[s] == 0 || PM[ST[s]+p] == 0)
    {
        printf("err ");
        return 0;
    }
    
    printf("%d ", PM[ST[s] + p] + w);
    return PM[ST[s] + p] + w;
}

int write_access(unsigned int s, unsigned int p, unsigned int w)
{
    if(ST[s] == -1 || PM[ST[s]+p] == -1)
    {
        printf("pf ");
        return -1;
    }
    else if(ST[s] == 0)
    {
        ST[s] = find_free_pt() * 512;
    }
    else if(PM[ST[s] + p] == 0)
    {
        PM[ST[s] + p] = find_free_page() * 512;
    }
    
    printf("%d ", PM[ST[s] + p] + w);
    return PM[ST[s] + p] + w;
}

int va_to_pa(unsigned int va, int action)
{
    unsigned int s = va & s_mask;
    s = s >> 19;
    unsigned int p = va & p_mask;
    p = p >> 9;
    unsigned int w = va & w_mask;
    
    if(TLB)
        TLB_function(s, p, w);
    
    if(action) //write access
        write_access(s, p, w);
    else
        read_access(s, p, w);
    
    return 0;
}

int operation(const char * va_file_name)
{
    if((va_file = fopen(va_file_name, "r")) == NULL)
    {
        printf("Va Fail open\n");
        return 1;
    }
    
    int action = -1;
    int va = -1;
    int pa = -1;
    while (fscanf(va_file, "%d", &action) != EOF)
    {
        if(fscanf(va_file, "%d", &va) == EOF)
            return 0;
        
        pa = va_to_pa(va, action);
    }
    return 0;
}

int main(int argc, const char * argv[])
{
    initial_MASK();
    bitmap[0] = bitmap[0] | MASK[0]; //initial bitmap
    
    s_mask = ((1 << 9) - 1) << 19;
    p_mask = ((1 << 10) - 1) << 9;
    w_mask = ((1 << 9) - 1) << 0;
    
    if(argc < 3)
    {
        printf("Lack Input files, please checck the input files\n");
        return 1;
    }
    
    initial_PM(argv[1]);
    operation(argv[2]);

    return 0;
}

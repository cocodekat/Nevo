#ifndef NPXM_H
#define NPXM_H

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// ─── UTILS ────────────────────────────────────────────────────────────
//
static unsigned char npxm_hex2byte(char hi, char lo) {
    unsigned char value = 0;
    if(hi>='0' && hi<='9') value += (hi-'0')<<4;
    else if(hi>='A' && hi<='F') value += (hi-'A'+10)<<4;
    else if(hi>='a' && hi<='f') value += (hi-'a'+10)<<4;

    if(lo>='0' && lo<='9') value += (lo-'0');
    else if(lo>='A' && lo<='F') value += (lo-'A'+10);
    else if(lo>='a' && lo<='f') value += (lo-'a'+10);

    return value;
}

//
// ─── COMPILE (PNG → BIMG) ─────────────────────────────────────────────
//
static int npxmcompile_internal(const char *input_file, const char *output_file, int quiet) {
    int width, height, channels;
    unsigned char *img = stbi_load(input_file, &width, &height, &channels, 4);
    if(!img){
        if(!quiet) printf("❌ Failed to load image: %s\n", input_file);
        return 1;
    }

    FILE *f = fopen(output_file, "w");
    if(!f){
        if(!quiet) printf("❌ Failed to open file for writing: %s\n", output_file);
        stbi_image_free(img);
        return 1;
    }

    fprintf(f, "HEADER\nwidth=%d\nheight=%d\npixels:\n", width, height);

    for(int y=0; y<height; y++){
        int x=0;
        while(x<width){
            int i = (y*width + x)*4;
            unsigned char r = img[i], g = img[i+1], b = img[i+2], a = img[i+3];

            int count = 1;
            while(x+count<width && count<255){
                int j = (y*width + x + count)*4;
                if(img[j]==r && img[j+1]==g && img[j+2]==b && img[j+3]==a)
                    count++;
                else break;
            }

            fprintf(f, "#%02X%02X%02X%02X", r,g,b,a);
            if(count>1) fprintf(f, "*%02X", count);
            x+=count;
            if(x<width) fprintf(f, ",");
        }
        fprintf(f, ";\n");
    }

    fclose(f);
    stbi_image_free(img);
    if(!quiet) printf("✅ Saved %s (%dx%d)\n", output_file, width, height);
    return 0;
}

#define npxmcompile(...) npxmcompile_selector(__VA_ARGS__, npxmcompile3, npxmcompile2)(__VA_ARGS__)
#define npxmcompile_selector(_1,_2,_3,NAME,...) NAME
#define npxmcompile2(input,output) npxmcompile_internal(input,output,0)
#define npxmcompile3(input,output,quietflag) npxmcompile_internal(input,output,quietflag)

//
// ─── DEPILE (npxm → PNG) ─────────────────────────────────────────────
//
static int npxmdepile_internal(const char *input_file, const char *output_file, int quiet) {
    FILE *f = fopen(input_file, "r");
    if(!f){ if(!quiet) printf("❌ Failed to open %s\n", input_file); return 1; }

    int width=0, height=0;
    char line[8192];

    while(fgets(line,sizeof(line),f)){
        if(strncmp(line,"width=",6)==0) width = atoi(line+6);
        else if(strncmp(line,"height=",7)==0) height = atoi(line+7);
        else if(strncmp(line,"pixels:",7)==0) break;
    }

    if(width==0 || height==0){
        if(!quiet) printf("❌ Invalid header in %s\n", input_file);
        fclose(f);
        return 1;
    }

    unsigned char *pixels = malloc(width*height*4);
    if(!pixels){ if(!quiet) printf("❌ Failed to allocate memory\n"); fclose(f); return 1; }

    int y=0;
    while(fgets(line,sizeof(line),f) && y<height){
        char *ptr=line;
        int x=0;
        while(x<width){
            while(*ptr==' '||*ptr=='\n'||*ptr=='\r') ptr++;
            if(*ptr!='#') break;

            unsigned char r=npxm_hex2byte(ptr[1],ptr[2]);
            unsigned char g=npxm_hex2byte(ptr[3],ptr[4]);
            unsigned char b=npxm_hex2byte(ptr[5],ptr[6]);
            unsigned char a=npxm_hex2byte(ptr[7],ptr[8]);
            ptr+=9;

            int count=1;
            if(*ptr=='*'){ ptr++; count=npxm_hex2byte(ptr[0],ptr[1]); ptr+=2; }
            if(*ptr==',') ptr++;

            for(int i=0;i<count && x<width;i++){
                int idx=(y*width+x)*4;
                pixels[idx+0]=r;
                pixels[idx+1]=g;
                pixels[idx+2]=b;
                pixels[idx+3]=a;
                x++;
            }
        }
        y++;
    }
    fclose(f);

    if(!stbi_write_png(output_file,width,height,4,pixels,width*4)){
        if(!quiet) printf("❌ Failed to write %s\n", output_file);
        free(pixels);
        return 1;
    }

    free(pixels);
    if(!quiet) printf("✅ Saved %s (%dx%d)\n", output_file, width, height);
    return 0;
}

#define npxmdepile(...) npxmdepile_selector(__VA_ARGS__, npxmdepile3, npxmdepile2)(__VA_ARGS__)
#define npxmdepile_selector(_1,_2,_3,NAME,...) NAME
#define npxmdepile2(input,output) npxmdepile_internal(input,output,0)
#define npxmdepile3(input,output,quietflag) npxmdepile_internal(input,output,quietflag)

//
// ─── MASTER MACRO ────────────────────────────────────────────────────
//
#define image(action) npxm##action
#define quiet 1

#endif // NPXM_H

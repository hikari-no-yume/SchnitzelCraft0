#include <stdio.h>
#include <tchar.h>
#include <winsock2.h>
#include <assert.h>
#include <math.h>
//#include "include/stdint.h"
#include "include/zlib.h"

typedef unsigned char uint8_t;
typedef signed char int8_t;
typedef unsigned short uint16_t;
typedef signed short int16_t;
typedef unsigned long uint32_t;
typedef signed long int32_t;

//#define fopen_s(f, n, m) (((int)((*f = fopen(n, m)), 0))

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif
//#define CHUNK 16384
#define CHUNK 1024

// Uncomment to enable physics
//#define PHYSICS

#define flippy 0 // Cos nopflip
#define numzombies 16 // Cos the more the deadlier etc
//#define numzombies 0
#define maxclients 64 // Player ID cannot be >127 (< 0 signed) as this means "teleport"

#define blockAt(x, y, z) ( ((x)>=0 && (y)>=0 && (z)>=0 && (x)<mapx && (y)<mapy && (z)<mapz) ? (block + (y)*mapx*mapz + (z)*mapz + (x)) : (block) )

struct BLOCKCHANGE {
        char player;
        short x;
        short y;
        short z;
        char newvalue;
} BLOCKCHANGE;
struct MOB {
        char used;
        char respawn;
        char direction;
        char hp;
        char name[65];
        // Position
        short x;
        short y;
        short z;
        char heading;
        char pitch;
} MOB;
struct CLIENT {
        char used;
        char op;
        SOCKET socket;
        char stage; // 0 - Connected
                       // 1 - Authenticated
                       // 2 - Welcome Message Sent
                       // 3 - Map sent
                       // 4 - Other players spawned, ready
        char name[65];
        char protocol;
        // Position
        short x;
        short y;
        short z;
        char heading;
        char pitch;
} CLIENT;

char *block;
int16_t mapx=256, mapy=128, mapz=256;
int32_t mapsize = 0; // 256x128x256
struct CLIENT client[maxclients];
int clients;
struct MOB mob[maxclients];

int def(FILE *source, FILE *dest, int level)
{
    int ret, flush;
    unsigned have;
    z_stream strm;
    char in[CHUNK];
    char out[CHUNK];

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    //ret = deflateInit(&strm, level);
    ret = deflateInit2(&strm, level, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY);
    if (ret != Z_OK)
        return ret;

    /* compress until end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)deflateEnd(&strm);
            return Z_ERRNO;
        }
        flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)deflateEnd(&strm);
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);
        assert(strm.avail_in == 0);     /* all input will be used */

        /* done when last data in file processed */
    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);        /* stream will be complete */

    /* clean up and return */
    (void)deflateEnd(&strm);
    return Z_OK;
}
double findnoise2(double x,double y)
{
 int n, nn;
 n=(int)x+(int)y*57;
 n=(n<<13)^n;
 nn=(n*(n*n*60493+19990303)+1376312589)&0x7fffffff;
 return 1.0-((double)nn/1073741824.0);
}
double interpolate1(double a,double b,double x)
{
 double ft=x * 3.1415927;
 double f=(1.0-cos(ft))* 0.5;
 return a*(1.0-f)+b*f;
}
double noise(double x,double y)
{
 double int1, int2, floorx, floory;
 double s,t,u,v;//Integer declaration
 floorx=(double)((int)x);//This is kinda a cheap way to floor a double integer.
 floory=(double)((int)y);
 s=findnoise2(floorx,floory);
 t=findnoise2(floorx+1,floory);
 u=findnoise2(floorx,floory+1);//Get the surrounding pixels to calculate the transition.
 v=findnoise2(floorx+1,floory+1);
 int1=interpolate1(s,t,x-floorx);//Interpolate between the values.
 int2=interpolate1(u,v,x-floorx);//Here we use x-floorx, to get 1st dimension. Don't mind the x-floorx thingie, it's part of the cosine formula.
 return interpolate1(int1,int2,y-floory);//Here we use y-floory, to get the 2nd dimension.
}

size_t sendByte(SOCKET socket, char value) {
    return send(socket, (char*)&value, sizeof(char), 0);
}
size_t sendByteArray(SOCKET socket, char *array, int len) {
    return send(socket, (char*)array, sizeof(char)*len, 0);
}
size_t sendInt16(SOCKET socket, int16_t value) {
    value = htons(value);
    return send(socket, (char*)&value, sizeof(int16_t), 0);
}
size_t sendInt32(SOCKET socket, int32_t value) {
    value = htonl(value);
    return send(socket, (char*)&value, sizeof(int32_t), 0);
}

char recvByte(SOCKET socket) {
    char value;
    recv(socket, (char*)&value, sizeof(char), 0);
    return value;
}
char* recvByteArray(SOCKET socket, char *array, int len) {
    recv(socket, (char*)array, sizeof(char)*len, 0);
    return array;
}
int16_t recvInt16(SOCKET socket) {
    int16_t value;
    recv(socket, (char*)&value, sizeof(int16_t), 0);
    return ntohs(value);
}
int32_t recvInt32(SOCKET socket) {
    int32_t value;
    recv(socket, (char*)&value, sizeof(int32_t), 0);
    return ntohl(value);
}

void sendPacket_welcome(SOCKET socket, char version, char *name, char *motd, char op){
    sendByte(socket, 0x00); // Welcome
    sendByte(socket, version); // Protocol Version
    sendByteArray(socket, name, 64); // Server Name
    sendByteArray(socket, motd, 64); // MOTD
    sendByte(socket, op); // OP status
}

void sendPacket_levelInitialize(SOCKET socket){
    sendByte(socket, 0x02);
}

void sendPacket_levelChunk(SOCKET socket, short size, char *chunk, char percent){
    sendByte(socket, 0x03); // Level Chunk
    sendInt16(socket, size); // Chunk Size
    sendByteArray(socket, chunk, 1024); // Chunk
    sendByte(socket, percent);
}

void sendPacket_levelFinalize(SOCKET socket, short x, short y, short z){
    sendByte(socket, 0x04); // Finalise
    sendInt16(socket, x); // X
    sendInt16(socket, y); // Y
    sendInt16(socket, z); // Z
}

void sendPacket_setBlock(SOCKET socket, short x, short y, short z, char type){
    sendByte(socket, 0x06); // Set Block
    sendInt16(socket, x); // X
    sendInt16(socket, y); // Y
    sendInt16(socket, z); // Z
    sendByte(socket, type); // Block Type
}

void sendPacket_spawnPlayer(SOCKET socket, char id, char *name, short x, short y, short z, char heading, char pitch){
    sendByte(socket, 0x07); // Spawn Player
    sendByte(socket, id); // Player ID
    sendByteArray(socket, name, 64); // Send Name
    sendInt16(socket, x); // Send X
    sendInt16(socket, y); // Send Y
    sendInt16(socket, z); // Send Z
    sendByte(socket, heading); // Send Heading
    sendByte(socket, pitch); // Send Pitch
}

void sendPacket_positionAndOrientation(SOCKET socket, char id, short x, short y, short z, char heading, char pitch){
    sendByte(socket, 0x08); // Position and Orientation Update
    sendByte(socket, id); // Player ID
    sendInt16(socket, x); // Send X
    sendInt16(socket, y); // Send Y
    sendInt16(socket, z); // Send Z
    sendByte(socket, heading); // Send Heading
    sendByte(socket, pitch); // Send Pitch
}

void sendPacket_despawn(SOCKET socket, char id){
    sendByte(socket, 0x0c); // Despawn
    sendByte(socket, id); // Player ID
}

void sendPacket_chatMessage(SOCKET socket, char id, char *message){
    sendByte(socket, 0x0d); // Chat Message
    sendByte(socket, id); // Player ID
    sendByteArray(socket, message, 64); // Message Body
}

void sendPacket_kick(SOCKET socket, char *message){
    sendByte(socket, 0x0e);
    sendByteArray(socket, message, 64);
}

char* setBlock(short x, short y, short z, char type){
    char* p;
    p=blockAt(x,y,z);
    *p = type;
    return p;
}

char* setBlock_synced(short x, short y, short z, char type){
    char *p = setBlock(x, y, z, type);
    int i;
    
    for (i=0;i<maxclients;i++){
        if (client[i].used==1&&client[i].stage==4){
            sendPacket_setBlock(client[i].socket, x, y, z, type);
        }
    }
    
    return p;
}

char getBlock(short x, short y, short z){
    char* p;
    p=blockAt(x,y,z);
    return *p;
}
char touching(short x, short y, short z, char type){
    char num = 0;
                                       if (getBlock(x-1,y,z)==type) num++;
    if (getBlock(x,y,z-1)==type) num++;                                      if (getBlock(x,y,z+1)==type) num++;
                                       if (getBlock(x+1,y,z)==type) num++;
    return num;
}
char touchingdg(short x, short y, short z, char type){
    char num = 0;
    if (getBlock(x-1,y,z-1)==type) num++;if (getBlock(x-1,y,z)==type) num++;if (getBlock(x-1,y,z+1)==type) num++;
    if (getBlock(x,y,z-1)==type) num++;                                        if (getBlock(x,y,z+1)==type) num++;
    if (getBlock(x+1,y,z-1)==type) num++;if (getBlock(x+1,y,z)==type) num++;if (getBlock(x+1,y,z+1)==type) num++;
    return num;
}
char touchinglr(short x, short y, short z, char type){
    char num = 0;
    num = num + touchingdg(x+3,y,z-3,type);num = num + touchingdg(x+3,y,z,type);num = num + touchingdg(x+3,y,z+3,type);
    num = num + touchingdg(x  ,y,z-3,type);num = num + touchingdg(x  ,y,z,type);num = num + touchingdg(x  ,y,z+3,type);
    num = num + touchingdg(x-3,y,z-3,type);num = num + touchingdg(x-3,y,z,type);num = num + touchingdg(x-3,y,z+3,type);
    return num;
}

void backupmap(){
    int32_t header;
    FILE* fp;
    char fname[255];
    int backupinc = 0;

    puts("Beginning map backup...");
    
    //if (fopen_s(&fp, "backups/backupinc.sys", "rb")==0){
    fp = fopen("backups/backupinc.sys", "rb");
    if (fp!=NULL){
        fread(&backupinc, sizeof(backupinc), 1, fp);
        fclose(fp);
    }

    // Randomly rename previus backup
    //sprintf_s((char*)&fname, 255, "backups/backup_%d.dat", backupinc);
    sprintf(fname, "backups/backup_%d.dat", backupinc);
    rename("backups/backup.dat", fname);
    backupinc++;

    printf("Saving map backup...");
    fp = fopen("backups/backup.dat", "wb");
    //fopen_s(&fp, "backups/backup.dat", "wb");
    header = htonl(mapsize);
    fwrite(&header,sizeof(int32_t),1,fp);
    fwrite(block,sizeof(char)*mapsize,1,fp);
    fclose(fp);
    printf("done.\n");

    //if (fopen_s(&fp, "backups/backupinc.sys", "wb")==0){
    fp = fopen("backups/backupinc.sys", "wb");
    if (fp!=NULL){
        fwrite(&backupinc, sizeof(backupinc), 1, fp);
        fclose(fp);
    }
}

void generateMap(int type) {
    int i, j, k;
    
    switch (type) {
        case 0: // Half flatgrass, half sand
            for (i=0;i<mapx;i++){
                for (j=0;j<mapz;j++){
                    setBlock(i,0,j,0x0C); // Sand
                    if (j!=0&&i!=0){
                        setBlock(i,mapy/2-3,j,0x07); // Bedrock (indestructible)
                    }else{
                        setBlock(i,mapy/2-3,j,0x09); // Stationary Water (indestructible)
                    }
                    setBlock(i,mapy/2-2,j,0x08); // Liquid Water
                    setBlock(i,mapy/2-1,j,0x08); // Liquid Water
                    setBlock(i,mapy/2,j,0x03); // Dirt
                    setBlock(i,mapy/2+1,j,0x03); // Dirt
                    setBlock(i,mapy/2+2,j,0x02); // Grass
                }
            }
            break;
        case 1: // Noise
            for (i=0;i<mapx;i++){
                for (j=0;j<mapz;j++){
                    int height;
                    height = (int)((noise(i*0.025f,j*0.025f)+1.0f)*(mapy/8))+mapy/4;
                    for (k = 0;k<height;k++){
                        if (height>(mapy/4)+(mapy/6)){
                            setBlock(i,k,j,0x01); // Stone
                        }else{
                            if (k!=height-1){
                                setBlock(i,k,j,0x03); // Dirt
                            }else{
                                setBlock(i,k,j,0x02); // Grass
                            }
                        }
                    }
                }
            }
            break;
    }
    for (i=1;i<(mapy/2)+4;i++){ // Builds route to surface
        setBlock(0,i,0,0x07); // Bedrock (indestructible)
        setBlock(1,i,0,0x07); // Bedrock (indestructible)
        setBlock(0,i,1,0x07); // Bedrock (indestructible)
        setBlock(1,i,1,0x09); // Stationary Water (indestructible)
        setBlock(2,i,2,0x07); // Bedrock (indestructible)
        if (i>3){
            setBlock(2,i,1,0x07); // Bedrock (indestructible)
            setBlock(1,i,2,0x07); // Bedrock (indestructible)
        }else{
            setBlock(2,i,1,0x09); // Stationary Water (indestructible)
            setBlock(1,i,2,0x09); // Stationary Water (indestructible)
        }
        setBlock(2,i,0,0x07); // Bedrock (indestructible)
        setBlock(0,i,2,0x07); // Bedrock (indestructible)
    }
}

int main(int argc, char* argv[])
{
    // Vars
    const int DEFAULT_PORT = 25565;
    const char welcomebuf[] = "AJF's Minecraft Server powered by SchnitzelCraft                gg2 is awesome etc...                                           ";
    int j = 0, i = 0, k = 0, l = 0, m = 0, physx = 0, physy = 0, physz = 0, phys = 0, lastmsg = 65, thisclient = 0, sal = 0, filesize = 0, iMode = 1;
    int32_t int32buf = 0;
    char outbuf[1024], inbuf[1024];
    struct sockaddr_in sa;
    struct fd_set readable, writeable;
    struct timeval timeout;
    WSADATA wsaData;
    SOCKET server, tempclient;
    FILE *fp;

    mapsize = mapx*mapy*mapz;
    block = (char*)malloc(mapsize*sizeof(char)); // Allocate
    assert(block!=NULL);
    memset(block,0x00,mapsize*sizeof(char));

    CreateDirectoryA("backups", 0); // Create backups folder (A as W is default)

    fp = fopen("backups/backup.dat", "rb");
    //fopen_s(&fp, "backups/backup.dat", "rb");
    if (fp!=NULL){ // If map exists load it
        printf("Loading map...");
        fread(&int32buf, sizeof(int32_t), 1, fp); // Skip header
        fread(block, sizeof(int8_t)*mapsize, 1, fp); // Read map from backup
        fclose(fp);
    }else{ // Else create anew
        printf("Preparing map...");
        generateMap(0);
    }
    printf("/\n");

    sa.sin_family = AF_INET;
    sa.sin_port = htons(DEFAULT_PORT);
    sa.sin_addr.s_addr = INADDR_ANY;
    sal = sizeof sa;

    timeout.tv_sec = 0;
    timeout.tv_usec = 100; // 20 milliseconds

    for(i = 0;i < maxclients;i++){
        client[i].used = 0;
        mob[i].used = 0;
    }
    for(i = 0; i < numzombies; i++){
        mob[i].used = 1;
        mob[i].x=(rand()%mapx)*32+16;
        mob[i].y=(mapy/2+10)*32;
        mob[i].z=(rand()%mapz)*32+16;
        //mob[i].hp = 5;
        mob[i].hp = 1; // Insta-kill
        mob[i].direction = 0;
        mob[i].heading = 0;
        mob[i].pitch = 0;
        mob[i].respawn = 0;
        //strcpy(&mob[i].name, "ZombieMob                                                       ");
        memset(&mob[i].name, ' ', 64);
        memcpy(&mob[i].name, "&2Zombie&7Mob", 13);
    }

    // init
    WSAStartup(MAKEWORD(2,0), &wsaData);
    server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    //ioctlsocket(server,FIONBIO,&iMode);

    if (bind(server, (SOCKADDR*)&sa, sizeof sa)!=SOCKET_ERROR){
        printf("SchnitzelCraft V0.3 (C) Andrew Faulds 2010-2011 \n");
        printf("This application uses zlib (C) 1995-2010 Jean-loup Gailly and Mark Adler\n");
        printf("Map size: %d", mapx);
        printf("x%dx", mapy);
        printf("%d: ", mapz);
        printf("%dKB\n", (int)(mapsize/1024));
        printf("Connect to port %d\n", DEFAULT_PORT);
        while (1){
            Sleep(50);
            phys = 0;
            // *** CONNECT BEGIN ***
            if (listen(server, 64)!=SOCKET_ERROR){
                FD_ZERO(&readable);
                FD_SET(server, &readable);
                if (select((int)NULL, &readable, NULL, NULL, &timeout) != SOCKET_ERROR){ // Query for waiting connections
                    if (FD_ISSET(server, &readable)){ // If there are waiting connections...
                        tempclient = accept(server, (SOCKADDR*)&sa, &sal);
                        if (tempclient!=SOCKET_ERROR){
                            for(i = 0;i < maxclients;i++){
                                if (client[i].used != 1&&mob[i].used != 1){
                                    thisclient = i;
                                    break;
                                }
                            }
                            printf("Client %d connected\n", thisclient);
                            memset(&client[thisclient], 0, sizeof(CLIENT)); // Reset
                            client[thisclient].used = 1;
                            client[thisclient].stage = 0;
                            client[thisclient].socket = tempclient;
                            //memset(&tempclient, 0, sizeof tempclient);
                            clients++;
                        }else{
                            printf("error: %d", WSAGetLastError());
                            system("pause");
                        }
                    }
                }else{
                    printf("error: %d", WSAGetLastError());
                    system("pause");
                }
            }else{
                printf("error: %d", WSAGetLastError());
                system("pause");
            }
            // *** CONNECT END ***
            memset(&inbuf,0,sizeof(inbuf));
            memset(&outbuf,0,sizeof(outbuf));
            for(i = 0;i < maxclients;i++){
                if (client[i].used==1){
                    FD_ZERO(&readable);
                    FD_SET(client[i].socket, &readable);
                    select((int)NULL, &readable, NULL, NULL, &timeout);
                    // *** RECV BEGIN ***
                    //if (FD_ISSET(client[i].socket, &readable)){
                    while(FD_ISSET(client[i].socket, &readable)){
                        inbuf[0] = recvByte(client[i].socket);
                        switch (inbuf[0]){
                            case 0x00:
                                if (client[i].stage==0){
                                    client[i].protocol = recvByte(client[i].socket);
                                    if (client[i].protocol == 0x07){ // 7
                                        recvByteArray(client[i].socket, client[i].name, 64); // Get Name
                                        client[i].name[64]='\0'; // Null terminate
                                        printf("Client %d identified: ", i);
                                        printf(client[i].name);
                                        recvByteArray(client[i].socket, outbuf, 64); // Skip Verification Key
                                        recvByte(client[i].socket); // Skip Unused byte
                                        client[i].stage = 1;
                                        for (j=0;j<maxclients;j++){ // Yay EVEN MOAR dirty hax
                                            if (client[j].used==1&&j!=i&&client[j].stage==4){
                                                sendPacket_spawnPlayer(client[j].socket, i, client[i].name, client[i].x, client[i].y, client[i].z, client[i].heading, client[i].pitch);
                                                outbuf[0]='&';
                                                outbuf[1]='e'; // Green
                                                outbuf[2]='J';
                                                outbuf[3]='O';
                                                outbuf[4]='I';
                                                outbuf[5]='N';
                                                outbuf[6]=' ';
                                                memcpy(&outbuf[7], &client[i].name, 57);
                                                sendPacket_chatMessage(client[j].socket, i, outbuf);
                                            }
                                        }
                                    }else{
                                        memcpy((char*)&outbuf, "Incompatible Protocol Version                                   ", 64);
                                        sendPacket_kick(client[i].socket, outbuf);
                                        printf("Client %d kicked: Incompatible Protocol Version ", i);
                                        printf("(%d)\n", client[i].protocol);
                                        closesocket(client[i].socket);
                                        client[i].used=0;
                                        goto exitloop;
                                    }
                                }else{
                                    printf("Client %d left\n", i);
                                    closesocket(client[i].socket);
                                    client[i].used=0;
                                    for (j=0;j<maxclients;j++){ // ZOMG MOAR HAX
                                        if (client[j].used==1&&client[j].stage==4){
                                            sendPacket_despawn(client[j].socket, i);
                                            if (flippy==1){
                                                sendPacket_despawn(client[j].socket, 64+i); // Despawn Flippy
                                            }
                                            outbuf[0]='&'; // Yellow
                                            outbuf[1]='c';
                                            outbuf[2]='P';
                                            outbuf[3]='A';
                                            outbuf[4]='R';
                                            outbuf[5]='T';
                                            outbuf[6]=' ';
                                            memcpy(&outbuf[7], &client[i].name, 57);
                                            sendPacket_chatMessage(client[j].socket, i, outbuf);
                                        }
                                    }
                                    backupmap(); // Save map backup
                                    goto exitloop;
                                }
                            break;
                            case 0x01: // Ping
                            break;
                            case 0x05: // Set Block
                                {
                                    struct BLOCKCHANGE bc;
                                    bc.player = i;
                                    bc.x = recvInt16(client[i].socket); // X
                                    bc.y = recvInt16(client[i].socket); // Y
                                    bc.z = recvInt16(client[i].socket); // Z
                                    inbuf[0] = recvByte(client[i].socket); // Get Mode
                                    bc.newvalue = recvByte(client[i].socket); // Block type
                                    if (inbuf[0]==0x00){ // If deleted
                                        bc.newvalue=0x00; // Air (deleted)
                                    }
                                    if (bc.newvalue==0x27){ // If Brown Mushroom
                                        bc.newvalue=0x08; // Liquid Water
                                    } // Disabled due to water physics
                                    if (bc.newvalue==0x2C){ // Step
                                        if (getBlock(bc.x,bc.y-1,bc.z)==0x2C){
                                            sendPacket_setBlock(client[i].socket, bc.x, bc.y, bc.z, 0x00); // Air
                                            bc.newvalue=0x2B; // Double Step
                                            bc.y -= 1;
                                        }
                                    }
                                    if (bc.newvalue==0x03&&touching(bc.x,bc.y,bc.z,0x02)>0){ // If dirt touching grass
                                        bc.newvalue=0x02; // Grass
                                    }
                                    if (getBlock(bc.x,bc.y,bc.z)==0x07||getBlock(bc.x,bc.y,bc.z)==0x09){ // If indestructible
                                        bc.newvalue=getBlock(bc.x,bc.y,bc.z);
                                    }
                                    setBlock_synced(bc.x,bc.y,bc.z,bc.newvalue);
                                    phys=1;
                                    physx=bc.x;
                                    physy=bc.y;
                                    physz=bc.z;
                                }
                            break;
                            case 0x08: // Position/Orientation
                                recvByte(client[i].socket); // Skip Player ID
                                client[i].x = recvInt16(client[i].socket); // Get new Player X
                                client[i].y = recvInt16(client[i].socket); // Get new Player Y
                                client[i].z = recvInt16(client[i].socket); // Get new Player Z
                                client[i].heading = recvByte(client[i].socket); // Get new Player Heading
                                client[i].pitch = recvByte(client[i].socket); // Get new Player Pitch
                            break;
                            case 0x0d: // Chat Message
                                recvByte(client[i].socket); // Skip Player ID
                                recvByteArray(client[i].socket, inbuf, 64); // Recieve message
                                for (j=0;j<maxclients;j++){ // Yay dirty hax
                                    if (client[j].used==1&&client[j].stage==4){
                                        if (lastmsg!=i){
                                            outbuf[0]='&'; // Yellow
                                            outbuf[1]='e';
                                            memcpy(&outbuf[2], &client[i].name, 62);
                                            sendPacket_chatMessage(client[j].socket, i, outbuf);
                                        }
                                        outbuf[0]='-'; // Indent
                                        outbuf[1]=']';
                                        memcpy(&outbuf[2], &inbuf, 62);
                                        sendPacket_chatMessage(client[j].socket, i, outbuf);
                                    }
                                }
                                lastmsg=i;
                            break;
                            default:
                                printf("Error: Unknown packet type: %x\n", inbuf[0]);
                                memcpy((char*)&outbuf, "Incompatible Protocol Version                                   ", 64);
                                sendPacket_kick(client[i].socket, outbuf);
                                printf("Client %d kicked: Incompatible Protocol Version\n", i);
                                closesocket(client[i].socket);
                                memset(&client[i].socket,0,sizeof(client[i].socket));
                                client[i].used=0;
                                goto exitloop;
                            break;
                        }
                        FD_ZERO(&readable);
                        FD_SET(client[i].socket, &readable);
                        select((int)NULL, &readable, NULL, NULL, &timeout);
                    }
exitloop:
                    // *** RECV END ***
                    #ifdef PHYSICS
                    if (phys)
                    for (j=physx-8;j<physx+8;j++){
                        for (k=physz-8;k<physz+8;k++){
                            if (getBlock(j,physy,k)==0x08&&touchinglr(j, physy, k, 0x13)>0){ // If Liquid Water and Touching Sponge (at long distance)
                                setBlock_synced(j,physy,k,0x00); // Set to Air
                            }else if (getBlock(j,physy,k)==0x00&&(touching(j, physy, k, 0x08)>0||getBlock(j, physy+1, k)==0x08)){ // If Air Touching Water/Water Above
                                if (touchinglr(j, physy, k, 0x13)==0){ // Not Touching Sponge (at long distance)
                                    setBlock_synced(j,physy,k,0x08); // Set to Liquid Water
                                }
                            }else if (getBlock(j,physy,k)==0x02&&getBlock(j,physy+1,k)!=0x00){ // If Grass and Vertical is not free
                                setBlock_synced(j,physy,k,0x03); // Set to Dirt
                            }else if (getBlock(j,physy,k)==0x03&&touching(j,physy, k, 0x02)>0){ // Dirt and Touching Grass
                                if (getBlock(j,physy+1,k)==0x00){ // Vertical is free
                                    setBlock_synced(j,physy,k,0x02); // Set to Grass
                                }
                            }
                        }
                    }
                    #endif
                    memset(&inbuf,0,sizeof(inbuf));
                    memset(&outbuf,0,sizeof(outbuf));
                    FD_ZERO(&writeable);
                    FD_SET(client[i].socket, &writeable);
                    select((int)NULL, NULL, &writeable, NULL, &timeout);
                    // *** SEND BEGIN ***
                    if (FD_ISSET(client[i].socket, &writeable)){
                        switch (client[i].stage){
                            case 1: // Welcome message
                                sendPacket_welcome(client[i].socket, 0x07, (char*)welcomebuf, (char*)(welcomebuf+64), client[i].op);
                                printf("Client %d sent welcome message\n", i);
                                client[i].stage=2;
                            break;
                            case 2:// Sending the map, one chunk at a time
                                {
                                FILE *fpin, *fpout;
                                backupmap();

                                printf("Compressing map data...");
                                //if (fopen_s(&fpin, "backups/backup.dat", "rb")==0
                                //    &&fopen_s(&fpout, "map.gz", "wb")==0){
                                fpin = fopen("backups/backup.dat", "rb");
                                fpout = fopen("map.gz", "wb");
                                if (fpin != NULL && fpout != NULL){
                                    k = def(fpin,fpout,9);
                                    if (k!=Z_OK){
                                        printf("Error compressing data: ");
                                        if (k==Z_MEM_ERROR){
                                            printf("Z_MEM_ERROR\n");
                                        }else if(k==Z_STREAM_ERROR){
                                            printf("Z_STREAM_ERROR\n");
                                        }else if(k==Z_VERSION_ERROR){
                                            printf("Z_VERSION_ERROR\n");
                                        }else if(k==Z_ERRNO){
                                            printf("Z_ERRNO:\n");
                                        }else{
                                            printf("%d\n", k);
                                        }
                                    }else{
                                        printf("done.\n");
                                    }
                                    fclose(fpin);
                                    fclose(fpout);
                                }else{
                                    printf("Error compressing data: Failed to open file.\n");
                                }

                                printf("Sending compressed map data...\n");
                                fpin = fopen("map.gz", "rb");
                                //fopen_s(&fpin, "map.gz", "rb");
                                if (fpin != NULL){
                                    fseek(fpin, 0L, SEEK_END);
                                    filesize = ftell(fpin); // Get file size
                                    fseek(fpin, 0L, SEEK_SET);
                                    sendPacket_levelInitialize(client[i].socket);
                                    k = 0;
                                    j = 0;
                                    while(1){
                                        j=fread(&inbuf,1,1024,fpin);
                                        memset(&outbuf,0,sizeof(outbuf));
                                        memcpy(&outbuf,&inbuf,j);
                                        k = k + j;
                                        sendPacket_levelChunk(client[i].socket, j, outbuf, (char)floor((double)k*(100/(double)filesize)));
                                        if (feof(fpin)!=0){
                                            sendPacket_levelFinalize(client[i].socket, mapx, mapy, mapz);
                                            printf("Finalized map data - total %d bytes sent.\n", k);
                                            break;
                                        }
                                    }
                                    fclose(fpin);
                                }else{
                                    printf("Error. Map data could not be sent.");
                                    exit(1);
                                }
                                client[i].stage=3;
                                }
                            break;
                            case 3: // Player data
                                for(j = 0;j < maxclients;j++){
                                    if (client[j].used==1){
                                        if (j!=i){
                                            sendPacket_spawnPlayer(client[i].socket, j, client[j].name, client[j].x, client[j].y, client[j].z, client[j].heading, client[j].pitch);
                                        }
                                        if (flippy==1){
                                            // Flippy
                                            memcpy(&outbuf, &client[j].name, 64);
                                            outbuf[0]='f';
                                            sendPacket_spawnPlayer(client[i].socket, 64+j, outbuf, (mapx*32)-client[j].x, client[j].y, (mapz*32)-client[j].z, client[j].heading+127, client[j].pitch);
                                        }
                                    }
                                    if (mob[j].used==1&&mob[j].respawn==0){
                                        sendPacket_spawnPlayer(client[i].socket, j, mob[j].name, mob[j].x, mob[j].y, mob[j].z, mob[j].heading, mob[j].pitch);
                                    }
                                }
                                // Spawn
                                client[i].x=6*32;
                                client[i].y=(mapy/2+10)*32;
                                client[i].z=6*32;
                                sendPacket_positionAndOrientation(client[i].socket, 0xFF /* Teleport */, client[i].x, client[i].y, client[i].z, client[i].heading, client[i].pitch);
                                printf("done.\n");
                                client[i].stage=4;
                            break;
                            case 4: // Normal
                                for(j = 0;j < maxclients;j++){ // Send Player Positions
                                    if (client[j].used==1){
                                        if (j!=i){
                                            sendPacket_positionAndOrientation(client[i].socket, j, client[j].x, client[j].y, client[j].z, client[j].heading, client[j].pitch);
                                        }
                                        if (flippy==1){
                                            // Flippy
                                            sendPacket_positionAndOrientation(client[i].socket, 64+j, (mapx*32)-client[j].x, client[j].y, (mapz*32)-client[j].z, client[j].heading+127, client[j].pitch);
                                        }
                                    }
                                    if (mob[j].used==1&&mob[j].respawn==0){
                                        sendPacket_positionAndOrientation(client[i].socket, j, mob[j].x, mob[j].y, mob[j].z, mob[j].heading, mob[j].pitch);
                                    }
                                }
                            break;
                        }
                    }
                    // *** SEND END ***
                    // *** MOBS BEGIN ***
                    for (j=0;j<maxclients;j++){
                        if (mob[j].used==1){
                            if (mob[j].respawn==0){ // If timer is 0
                                if (getBlock(mob[j].x/32, mob[j].y/32-2, mob[j].z/32)==0x00){ // Lame Gravity
                                    mob[j].y = mob[j].y - 8;
                                }
                                if (mob[j].y>((mob[j].y/32)*32+16)){ // Y position correction
                                    mob[j].y = mob[j].y - 8;
                                }
                                switch (mob[j].direction){
                                    case 0: // Forward
                                        mob[j].x = mob[j].x + 8;
                                        outbuf[0] = getBlock(mob[j].x/32+1, mob[j].y/32, mob[j].z/32);
                                        outbuf[1] = getBlock(mob[j].x/32+1, mob[j].y/32-1, mob[j].z/32);
                                    break;
                                    case 1: // Right
                                        mob[j].z = mob[j].z + 8;
                                        outbuf[0] = getBlock(mob[j].x/32, mob[j].y/32, mob[j].z/32+1);
                                        outbuf[1] = getBlock(mob[j].x/32, mob[j].y/32-1, mob[j].z/32+1);
                                    break;
                                    case 2: // Back
                                        mob[j].x = mob[j].x - 8;
                                        outbuf[0] = getBlock(mob[j].x/32-1, mob[j].y/32, mob[j].z/32);
                                        outbuf[1] = getBlock(mob[j].x/32-1, mob[j].y/32-1, mob[j].z/32);
                                    break;
                                    case 3: // Left
                                        mob[j].z = mob[j].z - 8;
                                        outbuf[0] = getBlock(mob[j].x/32, mob[j].y/32, mob[j].z/32-1);
                                        outbuf[1] = getBlock(mob[j].x/32, mob[j].y/32-1, mob[j].z/32-1);
                                    break;
                                }
                                // Lame Collision avoidance
                                if (outbuf[0]!=0x00 // Not air
                                    &&outbuf[0]!=0x08 // Not water
                                    &&outbuf[0]!=0x28){ // Not Red Mushroom (for epick trappawge)
                                    mob[j].direction++;
                                    mob[j].x = (mob[j].x/32)*32+16;
                                    mob[j].y = (mob[j].y/32)*32+16;
                                    mob[j].z = (mob[j].z/32)*32+16;
                                // JUMP
                                }else if (outbuf[1]!=0x00 // Not air
                                    &&outbuf[1]!=0x08 // Not water
                                    &&outbuf[1]!=0x28){ // Not Red Mushroom (for epick trappawge)
                                    mob[j].y=mob[j].y+32; // Jump
                                switch (mob[j].direction){
                                    case 0: // Forward
                                        mob[j].x = mob[j].x + 32;
                                    break;
                                    case 1: // Right
                                        mob[j].z = mob[j].z + 32;
                                    break;
                                    case 2: // Back
                                        mob[j].x = mob[j].x - 32;
                                    break;
                                    case 3: // Left
                                        mob[j].z = mob[j].z - 32;
                                    break;
                                }
                                }
                                if (mob[j].x>mapx*32||mob[j].z>mapz*32){
                                    mob[j].direction++;
                                    mob[j].x = (mob[j].x/32)*32+16;
                                    mob[j].y = (mob[j].y/32)*32+16;
                                    mob[j].z = (mob[j].z/32)*32+16;
                                }
                                if (mob[j].direction>3){
                                    mob[j].direction = 0;
                                }
                                mob[j].heading = (mob[j].direction+1)*64; // Adjust visible direction

                                if (getBlock(mob[j].x/32, mob[j].y/32-1, mob[j].z/32)==0x28){ // Touching Red Mushroom
                                    mob[j].hp = mob[j].hp - 1;
                                    setBlock_synced(mob[j].x/32, mob[j].y/32-1, mob[j].z/32, 0x00);
                                    for (k=0;k<maxclients;k++){ // Yay moar dirty hax
                                        if (client[k].used==1&&client[k].stage==4){
                                            strcpy(outbuf, "&7Zombie   &cHIT                                                  ");
                                            outbuf[9]=0x30+j;
                                            sendPacket_chatMessage(client[k].socket, 0, outbuf);
                                        }
                                    }
                                }
                                if (mob[j].hp<=0){ // Kill
                                    //mob[j].x=6*32+16;
                                    //mob[j].y=(mapy/2+10)*32;
                                    //mob[j].z=6*32+16;
                                    mob[j].x=(rand()%mapx)*32+16;
                                    mob[j].y=(mapy/2+10)*32;
                                    mob[j].z=(rand()%mapz)*32+16;
                                    mob[j].hp = 4;
                                    mob[j].heading = 0;
                                    mob[j].pitch = 0;
                                    mob[j].respawn = 1; // Set timer to 1, respawn
                                    for (k=0;k<maxclients;k++){ // Yay moar dirty hax
                                        if (client[k].used==1&&client[k].stage==4){
                                            strcpy(outbuf, "&7Zombie   &4DIED                                                 ");
                                            outbuf[9]=0x30+j;
                                            sendPacket_chatMessage(client[k].socket, 0, outbuf);
                                            sendPacket_despawn(client[k].socket, j); // Despawn
                                        }
                                    }
                                }
                            }else{
                                mob[j].respawn++; // Increment timer
                                if (mob[j].respawn==0){
                                    for (k=0;k<maxclients;k++){ // Yay moar dirty hax
                                        if (client[k].used==1&&client[k].stage==4){
                                            strcpy(outbuf, "&7Zombie   &aRESPAWNED                                            ");
                                            outbuf[9]=0x30+j;
                                            sendPacket_chatMessage(client[k].socket, 0, outbuf);
                                            sendPacket_spawnPlayer(client[k].socket, j, mob[j].name, mob[j].x, mob[j].y, mob[j].z, mob[j].heading, mob[j].pitch);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    // *** MOBS END ***
                }
            }
        }
    }else{
        printf("Bind/listen failure: %d", WSAGetLastError());
        system("pause");
    }

    // Cleanup
    WSACleanup();
    closesocket(server);
    return 0;
}

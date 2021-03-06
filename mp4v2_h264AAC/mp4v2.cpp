#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mp4v2/mp4v2.h>

int getNalu(FILE *pFile, unsigned char *pNalu)
{
    unsigned char c;
    int pos = 0;
    int len;

    if(!pFile)
        return -1;

    if((len = fread(pNalu, 1, 4, pFile)) <= 0)
        return -1;

    if(pNalu[0] != 0 || pNalu[1] != 0 || pNalu[2] != 0 || pNalu[3] != 1)
        return -1;

    pos = 4;

    while(1)
    {
        if(feof(pFile))
            break;

        pNalu[pos] = fgetc(pFile);
        
        if(pNalu[pos-3] == 0 && pNalu[pos-2] == 0 && pNalu[pos-1] == 0 && pNalu[pos] == 1)
        {
            fseek(pFile, -4, SEEK_CUR);
            pos -= 4;
            break;
        }

        pos++;
    }

    len = pos+1;

    return len;
}

int packet2Mp4(const char *inputFile, const char *outputFiles)
{
    FILE *pIn = NULL;
    unsigned char *pBuf = (unsigned char *)malloc(1024*1024*3);
    unsigned char *pNalu = NULL;
    unsigned char naluType;
    int len;
    int num = 0;
    MP4FileHandle pHandle = NULL;
    MP4TrackId videoId;
	int width = 1920;
	int height = 1080;
	int frameRate = 30;
	int timeScale = 90000;
    int addStream = 1;

    pIn = fopen(inputFile, "rb");
    if(!pIn)
        return -1;

    pHandle = MP4Create(outputFiles, 0);
    if(pHandle == MP4_INVALID_FILE_HANDLE)
    {
		printf("ERROR:Create mp4 handle fialed.\n");
		return -1;
    }

    MP4SetTimeScale(pHandle, timeScale);

    while(1)
    {
        len = getNalu(pIn, pBuf);
        if (len <= 0)
            break;

        if (pBuf[0] != 0 || pBuf[1] != 0 || pBuf[2] != 0 || pBuf[3] != 1)
            continue;

        len -= 4;
        pNalu = pBuf+4;
        naluType = pNalu[0]&0x1F;

        switch (naluType)
        {
            case 0x07: // SPS
                printf("------------------------------------\n");
                printf("sps(%d)\n", len);
                if (addStream)
                {
                    videoId = MP4AddH264VideoTrack
                            (pHandle, 
                            timeScale,              // 一秒钟多少timescale
                            timeScale/frameRate,    // 每个帧有多少个timescale
                            width,                  // width
                            height,                 // height
                            pNalu[1],               // sps[1] AVCProfileIndication
                            pNalu[2],               // sps[2] profile_compat
                            pNalu[3],               // sps[3] AVCLevelIndication
                            3);                     // 4 bytes length before each NAL unit
                    if (videoId == MP4_INVALID_TRACK_ID)
                    {
                        printf("Error:Can't add track.\n");
                        return -1;
                    }
                    
                    MP4SetVideoProfileLevel(pHandle, 0x7F);

                    addStream = 0;
                }

                MP4AddH264SequenceParameterSet(pHandle, videoId, pNalu, len);

                break;
            
            case 0x08: // PPS
                printf("pps(%d)\n", len);
                MP4AddH264PictureParameterSet(pHandle, videoId, pNalu, len);
                break;

            default:
                //printf("slice(%d)\n", len);
                pBuf[0] = (len>>24)&0xFF;
                pBuf[1] = (len>>16)&0xFF;
                pBuf[2] = (len>>8)&0xFF;
                pBuf[3] = (len>>0)&0xFF;
                MP4WriteSample(pHandle, videoId, pBuf, len+4, MP4_INVALID_DURATION, 0, 1);
                break;
        }
	}

    free(pBuf);
    fclose(pIn);
    MP4Close(pHandle, 0);

    return 0;
}

int main(int argc, char *argv[])
{
    if (packet2Mp4("/home/lrt/live555/test/1080P.264", "test.mp4"))
    {
        printf("Error:Packet to Mp4 fail.\n");
        return -1;
    }
    return 0;
}


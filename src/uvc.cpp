#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <wels/codec_api.h>
#include <libuvc/libuvc.h>

const int PARSE_SIZE = 1024 * 1024;
// 1. decoder declaration
//decoder declaration
ISVCDecoder *pSvcDecoder;
//input: encoded bitstream start position; should include start code prefix
unsigned char *pBuf = NULL;
//input: encoded bit stream length; should include the size of start code prefix
int iSize = 0;
//output: [0~2] for Y,U,V buffer for Decoding only
unsigned char *pData[3] = {NULL};
//in-out: for Decoding only: declare and initialize the output buffer info, this should never co-exist with Parsing only
SBufferInfo sDstBufInfo;
int count = 0;

void SaveFrame(unsigned char **pData, int width, int height, int iFrame)
{
    FILE *pFile;
    char szFilename[32];
    int i;

    // Open file
    sprintf(szFilename, "frame-%02d.yuv", iFrame);
    pFile = fopen(szFilename, "wb");
    if (pFile == NULL)
        return;

    for (i = 0; i < height; i++)
        fwrite(pData[0] + i * 928, 848, 1, pFile);
    for (i = 0; i < height / 2; i++)
        fwrite(pData[1] + i * 928 / 2, 848 / 2, 1, pFile);
    for (i = 0; i < height / 2; i++)
        fwrite(pData[2] + i * 928 / 2, 848 / 2, 1, pFile);

    // Close file
    fclose(pFile);
}

void video_decode(uvc_frame_t *frame)
{
    int iRet;
    pBuf = (unsigned char *)frame->data;
    iSize = frame->data_bytes;
    //for Decoding only
    iRet = pSvcDecoder->DecodeFrameNoDelay(pBuf, iSize, pData, &sDstBufInfo);
    //decode failed
    if (iRet != 0)
    {
        //error handling (RequestIDR or something like that)
        printf("decode error\n");
    }
    //for Decoding only, pData can be used for render.
    if (sDstBufInfo.iBufferStatus == 1)
    {
        SSysMEMBuffer sSystemBuffer = sDstBufInfo.UsrData.sSystemBuffer;
        printf("UsrData:\n\tw = %d\n\th = %d\n\tformat = %d\n\tiStride = [%d, %d]\n", sSystemBuffer.iWidth, sSystemBuffer.iHeight, sSystemBuffer.iFormat, sSystemBuffer.iStride[0], sSystemBuffer.iStride[1]);
        //output handling (pData[0], pData[1], pData[2])
        SaveFrame(pData, sSystemBuffer.iStride[0], sSystemBuffer.iHeight, count++);
    }else {
        printf("sDstBufInfo.iBufferStatus\n");
    }
}

/* This callback function runs once per frame. Use it to perform any
 * quick processing you need, or have it put the frame into your application's
 * input queue. If this function takes too long, you'll start losing frames. */
void cb(uvc_frame_t *frame, void *ptr)
{
    uvc_frame_t *bgr;
    uvc_frame_t *rgb;
    uvc_error_t ret;
    enum uvc_frame_format *frame_format = (enum uvc_frame_format *)ptr;
    FILE *fp;
    static int jpeg_count = 0;
    static const char *H264_FILE = "iOSDevLog.h264";
    /* static const char *H264_FILE = "iOSDevLog.h264";
     * static const char *MJPEG_FILE = ".jpeg";
     * char filename[16]; */
    char filename[16];

    /* We'll convert the image from YUV/JPEG to BGR, so allocate space */
#if 0
    bgr = uvc_allocate_frame(frame->width * frame->height * 3);
    if (!bgr)
    {
        printf("unable to allocate bgr frame!\n");
        return;
    }
#endif

    printf("callback! frame_format = %d, width = %d, height = %d, length = %lu\n",
           frame->frame_format, frame->width, frame->height, frame->data_bytes);

    switch (frame->frame_format)
    {
    case UVC_FRAME_FORMAT_H264:
        /* use `ffplay H264_FILE` to play */
        printf("H264\n");
        sprintf(filename, "%d%s", jpeg_count++, H264_FILE);
        video_decode(frame);

        // fp = fopen(filename, "a");
        // fwrite(frame->data, 1, frame->data_bytes, fp);
        // fclose(fp);
        // video_decode(frame);
        break;
    case UVC_COLOR_FORMAT_MJPEG:
        /* sprintf(filename, "%d%s", jpeg_count++, MJPEG_FILE);
             * fp = fopen(filename, "w");
             * fwrite(frame->data, 1, frame->data_bytes, fp);
             * fclose(fp); */
        break;
    case UVC_COLOR_FORMAT_YUYV:
        /* Do the BGR conversion */
        ret = uvc_any2bgr(frame, bgr);
        if (ret)
        {
            uvc_perror(ret, "uvc_any2bgr");
            uvc_free_frame(bgr);
            return;
        }
        break;
    default:
        printf("%u\n", frame->frame_format);
        break;
    }

    /* Call a user function:
     *
     * my_type *my_obj = (*my_type) ptr;
     * my_user_function(ptr, bgr);
     * my_other_function(ptr, bgr->data, bgr->width, bgr->height);
     */

    /* Call a C++ method:
     *
     * my_type *my_obj = (*my_type) ptr;
     * my_obj->my_func(bgr);
     */

    /* Use opencv.highgui to display the image:
     * 
     * cvImg = cvCreateImageHeader(
     *     cvSize(bgr->width, bgr->height),
     *     IPL_DEPTH_8U,
     *     3);
     *
     * cvSetData(cvImg, bgr->data, bgr->width * 3); 
     *
     * cvNamedWindow("Test", CV_WINDOW_AUTOSIZE);
     * cvShowImage("Test", cvImg);
     * cvWaitKey(10);
     *
     * cvReleaseImageHeader(&cvImg);
     */

#if 0
    uvc_free_frame(bgr);
#endif
}

int main(int argc, char **argv)
{
    memset(&sDstBufInfo, 0, sizeof(SBufferInfo));
    //in-out: for Parsing only: declare and initialize the output bitstream buffer info for parse only, this should never co-exist with Decoding only
    SParserBsInfo sDstParseInfo;
    memset(&sDstParseInfo, 0, sizeof(SParserBsInfo));
    sDstParseInfo.pDstBuff = new unsigned char[PARSE_SIZE]; //In Parsing only, allocate enough buffer to save transcoded bitstream for a frame

    // 2. decoder creation
    WelsCreateDecoder(&pSvcDecoder);

    // 3. decli
    SDecodingParam sDecParam = {0};
    sDecParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;
    //for Parsing only, the assignment is mandatory
    // sDecParam.bParseOnly = true;

    // 4. initialize the parameter and decoder context, allocate memory
    pSvcDecoder->Initialize(&sDecParam);

    // 5. do actual decoding process in slice level; this can be done in a loop until data ends

    uvc_context_t *ctx;
    uvc_device_t *dev;
    uvc_device_handle_t *devh;
    uvc_stream_ctrl_t ctrl;
    uvc_error_t res;

    /* Initialize a UVC service context. Libuvc will set up its own libusb
     * context. Replace NULL with a libusb_context pointer to run libuvc
     * from an existing libusb context. */
    res = uvc_init(&ctx, NULL);

    if (res < 0)
    {
        uvc_perror(res, "uvc_init");
        return res;
    }

    puts("UVC initialized");

    /* Locates the first attached UVC device, stores in dev */
    res = uvc_find_device(
        ctx, &dev,
        0, 0, NULL); /* filter devices: vendor_id, product_id, "serial_num" */

    if (res < 0)
    {
        uvc_perror(res, "uvc_find_device"); /* no devices found */
    }
    else
    {
        puts("Device found");

        /* Try to open the device: requires exclusive access */
        res = uvc_open(dev, &devh);

        if (res < 0)
        {
            uvc_perror(res, "uvc_open"); /* unable to open device */
        }
        else
        {
            puts("Device opened");

            /* Print out a message containing all the information that libuvc
             * knows about the device */
            uvc_print_diag(devh, stderr);

            const uvc_format_desc_t *format_desc = uvc_get_format_descs(devh);
            const uvc_frame_desc_t *frame_desc = format_desc->frame_descs;
            enum uvc_frame_format frame_format;
            int width = 640;
            int height = 480;
            int fps = 30;

            switch (format_desc->bDescriptorSubtype)
            {
            case UVC_VS_FORMAT_MJPEG:
                frame_format = UVC_COLOR_FORMAT_MJPEG;
                break;
            case UVC_VS_FORMAT_FRAME_BASED:
                frame_format = UVC_FRAME_FORMAT_H264;
                break;
            default:
                frame_format = UVC_FRAME_FORMAT_YUYV;
                break;
            }

            if (frame_desc)
            {
                width = frame_desc->wWidth;
                height = frame_desc->wHeight;
                fps = 10000000 / frame_desc->dwDefaultFrameInterval;
            }

            printf("\nFirst format: (%4s) %dx%d %dfps\n", format_desc->fourccFormat, width, height, fps);

            /* Try to negotiate first stream profile */
            res = uvc_get_stream_ctrl_format_size(
                devh, &ctrl, /* result stored in ctrl */
                frame_format,
                width, height, fps /* width, height, fps */
            );

            /* Print out the result */
            uvc_print_stream_ctrl(&ctrl, stderr);

            if (res < 0)
            {
                uvc_perror(res, "get_mode"); /* device doesn't provide a matching stream */
            }
            else
            {
                /* Start the video stream. The library will call user function cb:
                 *   cb(frame, (void *) 12345)
                 */
                res = uvc_start_streaming(devh, &ctrl, cb, (void *)12345, 0);

                if (res < 0)
                {
                    uvc_perror(res, "start_streaming"); /* unable to start stream */
                }
                else
                {
                    puts("Streaming...");

                    uvc_set_ae_mode(devh, 1); /* e.g., turn on auto exposure */

                    sleep(10); /* stream for 10 seconds */

                    /* End the stream. Blocks until last callback is serviced */
                    uvc_stop_streaming(devh);
                    puts("Done streaming.");
                }
            }

            /* Release our handle on the device */
            uvc_close(devh);
            puts("Device closed");
        }

        /* Release the device descriptor */
        uvc_unref_device(dev);
    }

    /* Close the UVC context. This closes and cleans up any existing device handles,
     * and it closes the libusb context if one was not provided. */
    uvc_exit(ctx);
    puts("UVC exited");
    printf("\n");

    //  Step 6:uninitialize the decoder and memory free

    pSvcDecoder->Uninitialize();

    //  Step 7:destroy the decoder

    WelsDestroyDecoder(pSvcDecoder);

    return 0;
}

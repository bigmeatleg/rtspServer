/************************************************************************************************/
/* Copyright (C), 2016-2017, Allwinner Tech. Co., Ltd.                                          */
/************************************************************************************************/
/**
 * @file sample_vi_ai_encode_vo_ao.cpp
 * @brief 该目录是对mpp中的 [VI+VENC+VO] + [AI+AENC+AO] 通路sample
 * @author id: wangguixing
 * @version v0.1
 * @date 2017-06-08
 */

/************************************************************************************************/
/*                                      Include Files                                           */
/************************************************************************************************/

#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <pthread.h>
#include <json.h>

#include "common.h"
#include "mpp_comm.h"

#include "MediaStream.h"
#include "TinyServer.h"
#include "httpd.h"
#include "rtsp.h"


/************************************************************************************************/
/*                                     Macros & Typedefs                                        */
/************************************************************************************************/
//#define ENABLE_VO_CLOCK

//#define AI_ENABLE
#define HTTP_HEADER_END     "\r\n\r\n"
#define HTTP_GET            "GET"
#define HTTP_POST           "POST"
#define HTTP_FONT           "Helvetica,Arial,sans-serif"

#define TIMEOUT             1500

static httpd g_set;

typedef int(* _gctrl_api)(int IspDev, int *value);
typedef int(* _sctrl_api)(int IspDev, int value);

typedef struct tag_ctrl_node {
	char 		*index_node;
	_gctrl_api	gfunc;
	_sctrl_api	sfunc;
} API_NODE;

/************************************************************************************************/
/*                                    Structure Declarations                                    */
/************************************************************************************************/
struct vi_venc_vo_param {
    pthread_t       thd_id;
    int             venc_chn;
    int             run_flag;
    PAYLOAD_TYPE_E  venc_type;
    MediaStream    *stream;
};

struct isp_request_parm	{
	char 			bitAE_mode: 1;
	char			bitAE_exposureBias: 1;
	char			bitAE_exposure: 1;
	char			bitAE_gain: 1;
	char			bitAWB_mode: 1;
	char			bitAWB_colortemp: 1;
	char			bitAWB_iso_sensitivity_mode: 1;
	char			bitAWB_iso_sensitivity: 1;
};
/************************************************************************************************/
/*                                      Global Variables                                        */
/************************************************************************************************/
static int g_send_flag = 1;
static pthread_t g_thd_id;
static AUDIO_CFG_TYPE_E g_audio_type = AUDIO_AAC_16BIT_16K_MONO;

static struct vi_venc_vo_param g_param_0;
static struct vi_venc_vo_param g_param_1;

static MediaStream *g_stream_0 = NULL;
static MediaStream *g_stream_1 = NULL;

static unsigned int   g_rc_mode   = 0;          /* 0:CBR 1:VBR 2:FIXQp 3:ABR 4:QPMAP */
static unsigned int   g_profile   = 1;          /* 0: baseline  1:MP  2:HP  3: SVC-T [0,3] */
static PAYLOAD_TYPE_E g_venc_type = PT_H264;    /* PT_H264/PT_H265/PT_MJPEG */
static ROTATE_E       g_rotate    = ROTATE_NONE;

static MPP_COM_VI_TYPE_E g_vi_type_0 = VI_720P_30FPS;
static MPP_COM_VI_TYPE_E g_vi_type_1 = VI_720P_25FPS;

static VENC_CFG_TYPE_E g_venc_type_0 = VENC_720P_TO_720P_4M_30FPS;
static VENC_CFG_TYPE_E g_venc_type_1 = VENC_720P_TO_720P_4M_25FPS;

static int iIspDev = ISP_DEV_1;

static API_NODE g_api_list[] = {
	{(char *)ISP_AE_MODE, 				AW_MPI_ISP_AE_GetMode, 					AW_MPI_ISP_AE_SetMode},
	{(char *)ISP_AE_EXPOSUREBIAS, 		AW_MPI_ISP_AE_GetExposureBias, 			AW_MPI_ISP_AE_SetExposureBias},
	{(char *)ISP_AE_GAIN, 				AW_MPI_ISP_AE_GetGain, 					AW_MPI_ISP_AE_SetGain},
	{(char *)ISP_AWB_MODE, 				AW_MPI_ISP_AWB_GetMode, 				AW_MPI_ISP_AWB_SetMode},
	{(char *)ISP_AWB_COLORTEMP, 		AW_MPI_ISP_AWB_GetColorTemp, 			AW_MPI_ISP_AWB_SetColorTemp},
	{(char *)ISP_AWB_ISOMODE, 			AW_MPI_ISP_AWB_GetISOSensitiveMode, 	AW_MPI_ISP_AWB_SetISOSensitiveMode},
	{(char *)ISP_AWB_ISOSENSITIVE, 		AW_MPI_ISP_AWB_GetISOSensitive, 		AW_MPI_ISP_AWB_SetISOSensitive},
	{(char *)ISP_AWB_RGAIN, 			AW_MPI_ISP_AWB_GetRGain, 				AW_MPI_ISP_AWB_SetRGain},
	{(char *)ISP_AWB_BGAIN, 			AW_MPI_ISP_AWB_GetBGain, 				AW_MPI_ISP_AWB_SetBGain},
	{(char *)ISP_FLICKER, 				AW_MPI_ISP_GetFlicker, 					AW_MPI_ISP_SetFlicker},
	{(char *)ISP_BRIGHTNESS, 			AW_MPI_ISP_GetBrightness, 				AW_MPI_ISP_SetBrightness},
	{(char *)ISP_CONTRAST, 				AW_MPI_ISP_GetContrast, 				AW_MPI_ISP_SetContrast},
	{(char *)ISP_SATURATION, 			AW_MPI_ISP_GetSaturation,				AW_MPI_ISP_SetSaturation},
	{(char *)ISP_SHARPNESS, 			AW_MPI_ISP_GetSharpness, 				AW_MPI_ISP_SetSharpness},
	{NULL, NULL}
};

//static MPP_MENU_VI_VENC_CFG_E  g_vi_ve_cfg = VI_4K_25FPS_VE_4K_25FPS_VE_720P_25FPS;

/************************************************************************************************/
/*                                    Function Declarations                                     */
/************************************************************************************************/
/* None */


/************************************************************************************************/
/*                                     Function Definitions                                     */
/************************************************************************************************/
	
int holdloop()
{
	char str[256] = {0};

	while(1){
		memset(str, 0, sizeof(str));
		fgets(str, sizeof(str), stdin);
		if('q' == str[0]){
			return 0;
		}
		
		printf("\n");
		sleep(1);
    }
	
}

static ERRORTYPE MPPCallbackFunc(void *cookie, MPP_CHN_S *pChn, MPP_EVENT_TYPE event, void *pEventData)
{
    DB_PRT(" pChn:%d  event:%d \n", pChn->mModId, (int)event);

    return SUCCESS;
}

static void *SelectStreamSendByRtsp(void *param)
{
    int ret = 0, chn_cnt = 0;
    int time_out = 0;
    unsigned char *buf = NULL;
    unsigned int   len = 0;
    uint64_t       pts = 0;
    int            frame_type;
    VencHeaderData head_info;
    AUDIO_STREAM_S stream;

    int hand_fd[VENC_MAX_CHN_NUM] = {0};
    handle_set rd_fds, bak_fds;
    PAYLOAD_TYPE_E venc_type[VENC_MAX_CHN_NUM];
    VENC_CHN_ATTR_S venc_attr;
    MediaStream *p_stream[VENC_MAX_CHN_NUM] = {NULL};
    MediaStream *rtsp_stream = NULL;

    AW_MPI_SYS_HANDLE_ZERO(&rd_fds);
    AW_MPI_SYS_HANDLE_ZERO(&bak_fds);

    /****** Add Video fd ******/
    chn_cnt = 0;
    hand_fd[0] = AW_MPI_VENC_GetHandle(VENC_CHN_0);
    AW_MPI_SYS_HANDLE_SET(hand_fd[0], &bak_fds);
#if 0	
    hand_fd[1] = AW_MPI_VENC_GetHandle(VENC_CHN_1);
    AW_MPI_SYS_HANDLE_SET(hand_fd[1], &bak_fds);
    chn_cnt++;
#endif	 

    AW_MPI_VENC_GetChnAttr(VENC_CHN_0, &venc_attr);
    venc_type[0] = venc_attr.VeAttr.Type;
#if 0	
    AW_MPI_VENC_GetChnAttr(VENC_CHN_1, &venc_attr);
    venc_type[1] = venc_attr.VeAttr.Type;
#endif	

    chn_cnt++;
#ifdef AI_ENABLE
    /****** Add Audio fd ******/
    hand_fd[chn_cnt] = AW_MPI_AENC_GetHandle(AENC_CHN_0);
    AW_MPI_SYS_HANDLE_SET(hand_fd[chn_cnt], &bak_fds);
#endif

    p_stream[0] = g_stream_0;
//    p_stream[1] = g_stream_1;
    p_stream[2] = NULL;

    time_out = 100; //100ms time out

    while (g_send_flag) {
        rd_fds = bak_fds;

        ret = AW_MPI_SYS_HANDLE_Select(&rd_fds, time_out);
        if (ret == 0) {
            DB_PRT("Do AW_MPI_SYS_HANDLE_Select time_out:%d!\n", time_out);
            usleep(10*1000);
            continue;
        } else if (ret < 0) {
            ERR_PRT("Do AW_MPI_SYS_HANDLE_Select error! ret:%d  errno[%d] errinfo[%s]\n",
                    ret, errno, strerror(errno));
            continue;
        } else {
            /** Check Video fd **/
            for (int i = 0; i < chn_cnt; i++) {
                if (AW_MPI_SYS_HANDLE_ISSET(hand_fd[i], &rd_fds)) {
                    buf        = NULL;
                    len        = 0;
                    frame_type = -1;

                    ret = mpp_comm_venc_get_stream(i, venc_type[i], 70, &buf, &len, &pts, &frame_type, &head_info);
                    if (ret) {
                        ERR_PRT("Do mpp_comm_venc_get_stream fail! ret:%d\n", ret);
                        continue;
                    }
                    //DB_PRT("get stream  chn:%d  buf:%p  len:%d  frame_type:%d \n", i, buf, len, frame_type);
                    if (NULL == p_stream) {
                        continue;
                    }

                    rtsp_stream = p_stream[i];
                    if (1 == frame_type) {
                        /* Get I frame */
                        rtsp_stream->appendVideoData(head_info.pBuffer, head_info.nLength, pts, MediaStream::FRAME_DATA_TYPE_HEADER);
                        rtsp_stream->appendVideoData(buf, len, pts, MediaStream::FRAME_DATA_TYPE_I);
                    } else {
                        rtsp_stream->appendVideoData(buf, len, pts, MediaStream::FRAME_DATA_TYPE_P);
                    }
                }
            }

#ifdef AI_ENABLE		
            /** Check Audio fd **/
            if (AW_MPI_SYS_HANDLE_ISSET(hand_fd[chn_cnt], &rd_fds)) {
                ret = AW_MPI_AENC_GetStream(AENC_CHN_0, &stream, 100);
                if (ret < 0) {
                    ERR_PRT("Do AW_MPI_AENC_GetStream error! ret:%d \n", ret);
                    continue;
                }

                g_stream_0->appendAudioData(stream.pStream, stream.mLen, stream.mTimeStamp);
                //g_stream_1->appendAudioData(stream.pStream, stream.mLen, stream.mTimeStamp);

                AW_MPI_AENC_ReleaseStream(AENC_CHN_0, &stream);
            }
#endif			
        }
    }

    DB_PRT("Out this function ... ... \n");
    return NULL;
}


static int CreateRtspServer(TinyServer **rtsp)
{
    int ret  = 0;
    int port = 8554;
    char ip[64] = {0};
    std::string ip_addr;

    ret = get_net_dev_ip("eth0", ip);
    if (ret) {
        ERR_PRT("Do get_net_dev_ip fail! ret:%d\n", ret);
        return -1;
    }
    DB_PRT("This dev eth0 ip:%s \n", ip);

    ip_addr = ip;
    *rtsp = TinyServer::createServer(ip_addr, port);

    DB_PRT("============================================================\n");
    DB_PRT("   rtsp://%s:%d/ch0~n  \n", ip_addr.c_str(), port);
    DB_PRT("============================================================\n");

    return 0;
}


static int rtsp_start(TinyServer *rtsp)
{
    int ret = 0;

    MediaStream::MediaStreamAttr attr;

    if (PT_H264 == g_venc_type) {
        attr.videoType  = MediaStream::MediaStreamAttr::VIDEO_TYPE_H264;
    } else if (PT_H265 == g_venc_type) {
        attr.videoType  = MediaStream::MediaStreamAttr::VIDEO_TYPE_H265;
    }
	
    attr.audioType  = MediaStream::MediaStreamAttr::AUDIO_TYPE_AAC;
    attr.streamType = MediaStream::MediaStreamAttr::STREAM_TYPE_UNICAST;

    g_stream_0 = rtsp->createMediaStream("ch0", attr);
    //g_stream_1 = rtsp->createMediaStream("ch1", attr);
    rtsp->runWithNewThread();

    ret = pthread_create(&g_thd_id, NULL, SelectStreamSendByRtsp, NULL);
    if (ret) {
        ERR_PRT("Do pthread_create SelectStreamSendByRtsp fail! ret:%d  errno[%d] errinfo[%s]\n",
                ret, errno, strerror(errno));
    }

    return 0;
}

int get_config_data(json_object **obj, const char *key)
{
	json_object *jobj;
	int val, i;
	for(int i = 0; NULL != g_api_list[i].index_node; i ++){
		if(strcmp(g_api_list[i].index_node, key) == 0){
			g_api_list[i].gfunc(iIspDev, &val);
			jobj = json_object_new_int(val);
			json_object_object_add(*obj, key, jobj);
		}
	}
	
	return 0;
}

int set_config_data(json_object **obj, const char *key, int value)
{
	json_object *jobj;
	int val, i ;
	for(int i =0; NULL != g_api_list[i].index_node; i ++){
		if(strcmp(g_api_list[i].index_node, key) == 0){
			g_api_list[i].sfunc(iIspDev, value);
			g_api_list[i].gfunc(iIspDev, &val);
			jobj = json_object_new_int(val);
			json_object_object_add(*obj, key, jobj);
		}
	}

	return 0;
}

void httpdCallBack(void *data_socket, void *data_act)
{
	socket_data *d = (socket_data *)data_socket;
	char *act = (char *)data_act;
	char 	*pos;
	int		arraylen, i;
	struct json_object *m_jobj, *d_jobj, *jitem;
	struct json_object *g_jobj;
	json_bool	status;
	char response[4096];

	const char *json_key;
	struct json_object *json_value;
	int m_value;

	m_jobj = json_tokener_parse(d->body);	
	if(strcmp(act, "get") == 0){		
		if(m_jobj != NULL){
			g_jobj = json_object_new_object();
			status = json_object_object_get_ex(m_jobj, (const char*)"ISP", &d_jobj);
		
			if(status){
				arraylen = json_object_array_length(d_jobj);
				for(i = 0; i < arraylen; i++){
					jitem = json_object_array_get_idx(d_jobj, i);
					//printf("%d: %s\n", i, json_object_get_string(jitem));
					get_config_data(&g_jobj, json_object_get_string(jitem));
				}
				strcpy(response, json_object_to_json_string(g_jobj));
			}	
		} else {
			strcpy(response, "data format error.");
		}
		httpd_send_response(d, response);
	} else {
		if(m_jobj != NULL){
			g_jobj = json_object_new_object();
			status = json_object_object_get_ex(m_jobj, (const char*)"ISP", &d_jobj);
		
			if(status){
				json_object_object_foreach(d_jobj, json_key, json_value){
					m_value = json_object_get_int(json_value);
					set_config_data(&g_jobj, json_key, m_value);
				}

				strcpy(response, json_object_to_json_string(g_jobj));
			}	
		} else {
			strcpy(response, "data format error.");
		}
		httpd_send_response(d, response);
	}
}

void* httpd_start(void *data)
{
	//httpd start for control isp
	HTTPD_CallBack _httpdcallback;
	_httpdcallback._CALLBACK = httpdCallBack;
   	httpd_init();

	httpd_set_callback(_httpdcallback);
	
	g_set.port = 80;
	g_set.base = "./";
	
    httpd_show_status();

    httpd_loop();
    httpd_uninit();
}

static void rtsp_stop(TinyServer *rtsp)
{
    g_send_flag = 0;
    usleep(10*1000);

    rtsp->stop();
    pthread_join(g_thd_id, 0);
    delete g_param_0.stream;
    //delete g_param_1.stream;
}


static int vi_create(void)
{
    int ret = 0;
    VI_ATTR_S vi_attr;

    /**  create vi dev 2  src 1080P **/
    ret = mpp_comm_vi_get_attr(g_vi_type_0, &vi_attr);
    if (ret) {
        ERR_PRT("Do mpp_comm_vi_get_attr fail! ret:%d \n", ret);
        return -1;
    }
    ret = mpp_comm_vi_dev_create(VI_DEV_0, &vi_attr);
    if (ret) {
        ERR_PRT("Do mpp_comm_vi_dev_create fail! ret:%d  vi_dev:%d\n", ret, VI_DEV_0);
        return -1;
    }
    ret = mpp_comm_vi_chn_create(VI_DEV_0, VI_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_vi_chn_create fail! ret:%d  vi_dev:%d  vi_chn:%d \n", ret, VI_DEV_0, VI_CHN_0);
        return -1;
    }

#if 0
    /**  create vi dev 3  src 720P **/
    ret = mpp_comm_vi_get_attr(g_vi_type_1, &vi_attr);
    if (ret) {
        ERR_PRT("Do mpp_comm_vi_get_attr fail! ret:%d \n", ret);
        return -1;
    }
    ret = mpp_comm_vi_dev_create(VI_DEV_3, &vi_attr);
    if (ret) {
        ERR_PRT("Do mpp_comm_vi_dev_create fail! ret:%d  vi_dev:%d\n", ret, VI_DEV_3);
        return -1;
    }
    ret = mpp_comm_vi_chn_create(VI_DEV_3, VI_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_vi_chn_create fail! ret:%d  vi_dev:%d  vi_chn:%d \n", ret, VI_DEV_3, VI_CHN_0);
        return -1;
    }
    ret = mpp_comm_vi_chn_create(VI_DEV_3, VI_CHN_1);
    if (ret) {
        ERR_PRT("Do mpp_comm_vi_chn_create fail! ret:%d  vi_dev:%d  vi_chn:%d \n", ret, VI_DEV_3, VI_CHN_1);
        return -1;
    }
#endif

    /*  Run isp server */
    AW_MPI_ISP_Init();
    AW_MPI_ISP_Run(iIspDev);

    return 0;
}


static int vi_destroy(void)
{
    int ret = 0;

    /*  Stop isp server */
    AW_MPI_ISP_Stop(iIspDev);
    AW_MPI_ISP_Exit();

    /**  destroy vi dev 2  **/
    ret = mpp_comm_vi_chn_destroy(VI_DEV_0, VI_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_vi_chn_destroy fail! ret:%d  vi_dev:%d  vi_chn:%d \n", ret, VI_DEV_0, VI_CHN_0);
    }
    ret = mpp_comm_vi_dev_destroy(VI_DEV_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_vi_dev_destroy fail! ret:%d  vi_dev:%d \n", ret, VI_DEV_0);
    }

#if 0
    /**  destroy vi dev 3  **/
    ret = mpp_comm_vi_chn_destroy(VI_DEV_3, VI_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_vi_chn_destroy fail! ret:%d  vi_dev:%d  vi_chn:%d \n", ret, VI_DEV_3, VI_CHN_0);
    }
    ret = mpp_comm_vi_chn_destroy(VI_DEV_3, VI_CHN_1);
    if (ret) {
        ERR_PRT("Do mpp_comm_vi_chn_destroy fail! ret:%d  vi_dev:%d  vi_chn:%d \n", ret, VI_DEV_1, VI_CHN_1);
    }
    ret = mpp_comm_vi_dev_destroy(VI_DEV_3);
    if (ret) {
        ERR_PRT("Do mpp_comm_vi_dev_destroy fail! ret:%d  vi_dev:%d \n", ret, VI_DEV_3);
    }
#endif
    return 0;
}


static int venc_create(void)
{
    int ret = 0;

    VENC_CFG_S venc_cfg  = {0};

    ret = mpp_comm_venc_get_cfg(g_venc_type_0, &venc_cfg);
    if (ret) {
        ERR_PRT("Do mpp_comm_venc_get_cfg fail! ret:%d \n", ret);
        return -1;
    }
    ret = mpp_comm_venc_create(VENC_CHN_0, g_venc_type, g_rc_mode, g_profile, g_rotate, &venc_cfg);
    if (ret) {
        ERR_PRT("Do mpp_comm_venc_create fail! ret:%d \n", ret);
        return -1;
    }
	
#if 0
    ret = mpp_comm_venc_get_cfg(g_venc_type_1, &venc_cfg);
    if (ret) {
        ERR_PRT("Do mpp_comm_venc_get_cfg fail! ret:%d \n", ret);
        return -1;
    }
    ret = mpp_comm_venc_create(VENC_CHN_1, g_venc_type, g_rc_mode, g_profile, g_rotate, &venc_cfg);
    if (ret) {
        ERR_PRT("Do mpp_comm_venc_create fail! ret:%d \n", ret);
        return -1;
    }
#endif

    /* Register Venc call back function */
    MPPCallbackInfo venc_cb;
    venc_cb.cookie   = NULL;
    venc_cb.callback = (MPPCallbackFuncType)&MPPCallbackFunc;
    AW_MPI_VENC_RegisterCallback(VENC_CHN_0, &venc_cb);

    return 0;
}

static int venc_destroy(void)
{
    int ret = 0;

    ret = mpp_comm_venc_destroy(VENC_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_venc_destroy fail! ret:%d  venc_chn:%d \n", ret, VENC_CHN_0);
    }
	
#if 0	
    ret = mpp_comm_venc_destroy(VENC_CHN_1);
    if (ret) {
        ERR_PRT("Do mpp_comm_venc_destroy fail! ret:%d  venc_chn:%d \n", ret, VENC_CHN_1);
    }
#endif

    return 0;
}


static int vo_create(void)
{
    int ret = 0;
    VO_DEV_TYPE_E vo_type;
    VO_DEV_CFG_S  vo_cfg;
    vo_type           = VO_DEV_LCD;
    vo_cfg.res_width  = 720;
    vo_cfg.res_height = 1280;
    ret = mpp_comm_vo_dev_create(vo_type, &vo_cfg);
    if (ret) {
        ERR_PRT("Do mpp_comm_vo_dev_create fail! ret:%d  vo_type:%d\n", ret, vo_type);
        return -1;
    }

    VO_CHN_CFG_S vo_chn_cfg;
    vo_chn_cfg.top        = 0;
    vo_chn_cfg.left       = 0;
    vo_chn_cfg.width      = 720;
    vo_chn_cfg.height     = 1280;
    vo_chn_cfg.vo_buf_num = 0;
    ret = mpp_comm_vo_chn_create(VO_CHN_0, &vo_chn_cfg);
    if (ret) {
        ERR_PRT("Do mpp_comm_vo_chn_create fail! ret:%d  vo_chn:%d\n", ret, VO_CHN_0);
        return -1;
    }

#ifdef ENABLE_VO_CLOCK
    /* Create Clock for vo mode */
    ret = mpp_comm_vo_clock_create(CLOCK_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_vo_clock_create fail! ret:%d  clock_chn:%d\n", ret, CLOCK_CHN_0);
        return -1;
    }
#endif

    return 0;
}

static int vo_destroy(void)
{
    int ret = 0;

#ifdef ENABLE_VO_CLOCK
    ret = mpp_comm_vo_clock_destroy(CLOCK_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_vo_clock_destroy fail! ret:%d  clock_chn:%d\n", ret, CLOCK_CHN_0);
    }
#endif

    ret = mpp_comm_vo_chn_destroy(VO_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_vo_chn_destroy fail! ret:%d  vo_chn:%d\n", ret, VO_CHN_0);
    }

    ret = mpp_comm_vo_dev_destroy(VO_DEV_LCD);
    if (ret) {
        ERR_PRT("Do mpp_comm_vo_dev_destroy fail! ret:%d  vo_type:%d\n", ret, VO_DEV_LCD);
    }

    return 0;
}


static int ai_create(void)
{
    int ret = 0;
    AUDIO_CFG_S audio_cfg = {0};

    /* Step 1.  Get audio config by type */
    ret = mpp_comm_get_audio_cfg(g_audio_type, &audio_cfg);
    if (ret) {
        ERR_PRT("Do mpp_comm_get_audio_cfg fail! ret:%d \n", ret);
        return ret;
    }

    /* Step 2. Create ai device and channel. */
    ret = mpp_comm_ai_create(AI_CHN_0, &audio_cfg);
    if (ret) {
        ERR_PRT("Do mpp_comm_ai_create fail! ai_chn:%d ret:%d \n", AI_CHN_0, ret);
        return ret;
    }
    return 0;
}


static int ai_destroy(void)
{
    int ret = 0;

    ret = mpp_comm_ai_destroy(AI_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_ai_destroy fail! ai_chn:%d ret:0x%x \n", AI_CHN_0, ret);
    }

    return 0;
}


static int ao_create(void)
{
    int ret = 0;
    AUDIO_CFG_S audio_cfg = {0};

    /* Step 1.  Get audio config by type */
    ret = mpp_comm_get_audio_cfg(g_audio_type, &audio_cfg);
    if (ret) {
        ERR_PRT("Do mpp_comm_get_audio_cfg fail! ret:%d \n", ret);
        return ret;
    }

    ret = mpp_comm_ao_create(AO_CHN_0, &audio_cfg, NULL);
    if (ret) {
        ERR_PRT("Do mpp_comm_ao_create fail! ao_chn:%d ret:%d \n", AO_CHN_0, ret);
        return ret;
    }
    ret = mpp_comm_ao_clock_create(CLOCK_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_ao_clock_create fail! clock_chn:%d ret:%d \n", CLOCK_CHN_0, ret);
        return ret;
    }

    return 0;
}


static int ao_destroy(void)
{
    int ret = 0;

    ret = mpp_comm_ao_clock_destroy(CLOCK_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_ao_clock_destroy fail! ret:%d \n", ret);
    }

    ret = mpp_comm_ao_destroy(AO_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_ao_destroy fail! ret:%d \n", ret);
    }
    return 0;
}


static int aenc_create(void)
{
    int ret = 0;
    AUDIO_CFG_S audio_cfg = {0};

    ret = mpp_comm_get_audio_cfg(g_audio_type, &audio_cfg);
    if (ret) {
        ERR_PRT("Do mpp_comm_get_audio_cfg fail! ret:%d \n", ret);
        return ret;
    }

    ret = mpp_comm_aenc_create(AENC_CHN_0, &audio_cfg);
    if (ret) {
        ERR_PRT("Do mpp_comm_aenc_create fail! aenc_chn:%d ret:%d \n", AENC_CHN_0, ret);
        return ret;
    }

    return 0;
}


static int aenc_destroy(void)
{
    int ret = 0;
    ret = mpp_comm_aenc_destroy(AENC_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_aenc_destroy fail! aenc_chn:%d ret:0x%x \n", AENC_CHN_0, ret);
    }

    return 0;
}


static int components_bind(void)
{
    int ret = 0;

    /****** Bind video mode ******/
    ret = mpp_comm_vi_bind_venc(VI_DEV_0, VI_CHN_0, VENC_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_vi_bind_venc fail! ret:%d \n", ret);
        return -1;
    }
	
#if 0	
    ret = mpp_comm_vi_bind_venc(VI_DEV_3, VI_CHN_0, VENC_CHN_1);
    if (ret) {
        ERR_PRT("Do mpp_comm_vi_bind_venc fail! ret:%d \n", ret);
        return -1;
    }
#endif

#if 0
#ifdef ENABLE_VO_CLOCK
    ret = mpp_comm_vo_bind_clock(CLOCK_CHN_0, VO_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_vo_bind_clock fail! ret:%d clock_chn:%d vo_chn:%d\n", ret, CLOCK_CHN_0, VO_CHN_0);
        return -1;
    }
#endif

    ret = mpp_comm_vi_bind_vo(VI_DEV_3, VI_CHN_1, VO_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_vi_bind_vo fail! ret:%d \n", ret);
        return -1;
    }
#endif 

#ifdef AI_ENABLE
    /****** Bind audio mode ******/
    ret = mpp_comm_ai_bind_aenc(AI_CHN_0, AENC_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_ai_bind_aenc fail! ret:%d \n", ret);
        return -1;
    }
#endif	

#if 0
    ret = mpp_comm_ao_bind_clock(CLOCK_CHN_0, AO_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_ao_bind_clock fail! ret:%d \n", ret);
        return -1;
    }
#endif	

    return 0;
}


static int components_unbind(void)
{
    int ret = 0;

    /****** UnBind video mode ******/
    ret = mpp_comm_vi_unbind_venc(VI_DEV_0, VI_CHN_0, VENC_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_vi_unbind_venc fail! ret:%d\n", ret);
    }
	
#if 0	
    ret = mpp_comm_vi_unbind_venc(VI_DEV_3, VI_CHN_0, VENC_CHN_1);
    if (ret) {
        ERR_PRT("Do mpp_comm_vi_unbind_venc fail! ret:%d\n", ret);
    }

    ret = mpp_comm_vi_unbind_vo(VI_DEV_3, VI_CHN_1, VO_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_vi_unbind_vo fail! ret:%d\n", ret);
    }
#endif	

#if 0
#ifdef ENABLE_VO_CLOCK
    ret = mpp_comm_vo_unbind_clock(CLOCK_CHN_0, VO_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_vo_unbind_clock fail! ret:%d clock_chn:%d vo_chn:%d\n", ret, CLOCK_CHN_0, VO_CHN_0);
    }
#endif
#endif

#ifdef AI_ENABLE
    /****** UnBind audio mode ******/
    ret = mpp_comm_ai_unbind_aenc(AI_CHN_0, AENC_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_ai_unbind_aenc fail! ret:%d \n", ret);
        return -1;
    }
#endif	

#if 0
    ret = mpp_comm_ao_unbind_clock(CLOCK_CHN_0, AO_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_ao_unbind_clock fail! ret:%d \n", ret);
        return -1;
    }
#endif	

    return 0;
}


static int components_start(void)
{
    int ret = 0;

    /****** Start video mode ******/
    ret = AW_MPI_VI_EnableVirChn(VI_DEV_0, VI_CHN_0);
    if (ret) {
        ERR_PRT("Do AW_MPI_VI_EnableVirChn fail! ret:%d\n", ret);
        return -1;
    }
#if 0	
    ret = AW_MPI_VI_EnableVirChn(VI_DEV_3, VI_CHN_0);
    if (ret) {
        ERR_PRT("Do AW_MPI_VI_EnableVirChn fail! ret:%d\n", ret);
        return -1;
    }
    ret = AW_MPI_VI_EnableVirChn(VI_DEV_3, VI_CHN_1);
    if (ret) {
        ERR_PRT("Do AW_MPI_VI_EnableVirChn fail! ret:%d\n", ret);
        return -1;
    }
#endif	

    ret = AW_MPI_VENC_StartRecvPic(VENC_CHN_0);
    if (ret) {
        ERR_PRT("Do AW_MPI_VENC_StartRecvPic fail! ret:%d\n", ret);
        return -1;
    }

#if 0	
    ret = AW_MPI_VENC_StartRecvPic(VENC_CHN_1);
    if (ret) {
        ERR_PRT("Do AW_MPI_VENC_StartRecvPic fail! ret:%d\n", ret);
        return -1;
    }
#endif 	

#if 0
#ifdef ENABLE_VO_CLOCK
    ret = AW_MPI_CLOCK_Start(CLOCK_CHN_0);
    if (ret) {
        ERR_PRT("Do AW_MPI_CLOCK_Start fail! clock_chn:%d  ret:%d\n", CLOCK_CHN_0, ret);
        return -1;
    }
#endif

    ret = mpp_comm_vo_chn_start(VO_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_vo_chn_start vo_chn:%d fail! ret:%d\n", VO_CHN_0, ret);
        return -1;
    }
#endif

#if 0
    /****** Start audio mode ******/
    ret = AW_MPI_CLOCK_Start(CLOCK_CHN_0);
    if (ret) {
        ERR_PRT("Do AW_MPI_CLOCK_Start fail! ret:%d\n", ret);
        return -1;
    }

#endif

#ifdef AI_ENABLE
    ret = AW_MPI_AI_EnableChn(AI_DEV_0, AI_CHN_0);
    if (ret) {
        ERR_PRT("Do AW_MPI_AI_EnableChn fail! ret:%d\n", ret);
        return -1;
    }

    ret = AW_MPI_AENC_StartRecvPcm(AENC_CHN_0);
    if (ret) {
        ERR_PRT("Do AW_MPI_AENC_StartRecvPcm fail! ret:%d\n", ret);
        return -1;
    }
#endif

#if 0
    ret = AW_MPI_AO_StartChn(AO_DEV_0, AO_CHN_0);
    if (ret) {
        ERR_PRT("Do AW_MPI_AO_EnableChn fail! ret:%d\n", ret);
        return -1;
    }
#endif	

    return 0;
}


static int components_stop(void)
{
    int ret = 0;

#if 0
    /****** Stop video mode ******/
    ret = mpp_comm_vo_chn_stop(VO_CHN_0);
    if (ret) {
        ERR_PRT("Do mpp_comm_vo_chn_stop vo_chn:%d fail! ret:%d\n", VO_CHN_0, ret);
    }

#ifdef ENABLE_VO_CLOCK
    ret = AW_MPI_CLOCK_Stop(CLOCK_CHN_0);
    if (ret) {
        ERR_PRT("Do AW_MPI_CLOCK_Stop fail! clock_chn:%d  ret:%d\n", CLOCK_CHN_0, ret);
    }
#endif
#endif

    ret = AW_MPI_VENC_StopRecvPic(VENC_CHN_0);
    if (ret) {
        ERR_PRT("Do AW_MPI_VENC_StopRecvPic fail! ret:%d  venc_chn:%d \n", ret, VENC_CHN_0);
    }

#if 0	
    ret = AW_MPI_VENC_StopRecvPic(VENC_CHN_1);
    if (ret) {
        ERR_PRT("Do AW_MPI_VENC_StopRecvPic fail! ret:%d  venc_chn:%d \n", ret, VENC_CHN_1);
    }
#endif	

    ret = AW_MPI_VI_DisableVirChn(VI_DEV_0, VI_CHN_0);
    if (ret) {
        ERR_PRT("Do AW_MPI_VI_DisableVirChn fail! ret:%d  venc_chn:%d \n", ret, VENC_CHN_0);
    }

#if 0	
    ret = AW_MPI_VI_DisableVirChn(VI_DEV_3, VI_CHN_0);
    if (ret) {
        ERR_PRT("Do AW_MPI_VI_DisableVirChn fail! ret:%d  venc_chn:%d \n", ret, VENC_CHN_0);
    }
    ret = AW_MPI_VI_DisableVirChn(VI_DEV_3, VI_CHN_1);
    if (ret) {
        ERR_PRT("Do AW_MPI_VI_DisableVirChn fail! ret:%d  venc_chn:%d \n", ret, VENC_CHN_0);
    }
#endif	
#ifdef AI_ENABLE
    /****** Stop audio mode ******/
    ret = AW_MPI_AENC_StopRecvPcm(AENC_CHN_0);
    if (ret) {
        ERR_PRT("Do AW_MPI_AENC_StopRecvPcm fail! ret:%d\n", ret);
    }	

#if 0
    ret = AW_MPI_AO_StopChn(AO_DEV_0, AO_CHN_0);
    if (ret) {
        ERR_PRT("Do AW_MPI_AO_StopChn fail! ret:%d\n", ret);
    }
#endif	

    ret = AW_MPI_AI_DisableChn(AI_DEV_0, AI_CHN_0);
    if (ret) {
        ERR_PRT("Do AW_MPI_AI_DisableChn fail! ret:%d\n", ret);
    }
	
#if 0
    ret = AW_MPI_CLOCK_Stop(CLOCK_CHN_0);
    if (ret) {
        ERR_PRT("Do AW_MPI_CLOCK_Stop fail! ret:%d\n", ret);
    }
#endif
#endif
    return 0;
}


int main(int argc, char **argv)
{
    int ret = 0;
	pthread_t thread_httpd;
	
    /* Setup 0. Create rtsp */
    TinyServer *rtsp;
    ret = CreateRtspServer(&rtsp);
    if (ret) {
        ERR_PRT("Do CreateRtspServer fail! ret:%d \n", ret);
        return -1;
    }

    /* Step 1. Init mpp system */
    ret = mpp_comm_sys_init();
    if (ret) {
        ERR_PRT("Do mpp_comm_sys_init fail! ret:%d \n", ret);
        return -1;
    }

    /* Step 2. Get vi config to create 1080P/720P vi channel */
    ret = vi_create();
    if (ret) {
        ERR_PRT("Do vi_create fail! ret:%d\n", ret);
        goto _exit_1;
    }

    /* Step 3. Get venc config to create 1080P/720P venc channel */
    ret = venc_create();
    if (ret) {
        ERR_PRT("Do venc_create fail! ret:%d\n", ret);
        goto _exit_2;
    }

	
#if 0
    /* Step 4. Create vo device and layer */
    ret = vo_create();
    if (ret) {
        ERR_PRT("Do vo_create fail! ret:%d \n", ret);
        goto _exit_3;
    }
#endif 
    /* Step 5. Create audio(ai, ao, aenc) mode. */

#ifdef AI_ENABLE
    ret = ai_create();
    if (ret) {
        ERR_PRT("Do ai_create fail! ret:%d \n", ret);
        goto _exit_3;
    }
	
    ret = aenc_create();
    if (ret) {
        ERR_PRT("Do aenc_create fail! ret:%d \n", ret);
        goto _exit_4;
    }
#endif	

    /* Step 6. Bind clock to vo,  and vi to vo, vi to venc */
    ret = components_bind();
    if (ret) {
        ERR_PRT("Do components_bind fail! ret:%d \n", ret);
        goto _exit_5;
    }

    /* Setup 7. Start  vi, venc, clock, vo */
    ret = components_start();
    if (ret) {
        ERR_PRT("Do components_start fail! ret:%d \n", ret);
        goto _exit_6;
    }

    /* Setup 8. Create thread, and get stream send to rtsp */
    rtsp_start(rtsp);

	pthread_create(&thread_httpd, NULL, httpd_start, 0);
	
	holdloop();    

    rtsp_stop(rtsp);
    delete rtsp;

_exit_6:
    components_stop();

_exit_5:
    components_unbind();

#ifdef AI_ENABLE
_exit_4:
		aenc_destroy();

_exit_3:
    ai_destroy();
#endif

#if 0
_exit_3:
    vo_destroy();
#endif

_exit_2:
    venc_destroy();

_exit_1:
    vi_destroy();

    ret = mpp_comm_sys_exit();
    if (ret) {
        ERR_PRT("Do mpp_comm_sys_exit fail! ret:%d \n", ret);
    }

    return ret;
}


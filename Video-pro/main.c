
#include <getopt.h>
#include "include/config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <signal.h>

#define SQLUSER      "root"
#define SQLPASSWD    "zqy19950513"
#define DATABASE     "wyw0907"

_global V_global;

static void help(char *arg)
{
    printf("---------------------------------------------\n");
    printf("arqument: -v/-video  + \"\" open    choosed video\n");
    printf("arqument: -s/-sql    + \"\" connect choosed sql\n");
    printf("arqument: -t/-http   + \"\" connect choosed http-service\n");
    printf("arqument: -r/-reset         reset this video\n");
    printf("arqument: -h/-help          print help message\n");
    printf("arqument: -p/-people + \"\" add a new people into group\n");
    printf("arqument: -l/-local         use off-line mode\n");
    printf("----------------------------------------------\n");
    printf("\n");
    printf("if the video has not setted:\n");
    printf("you can use your App to scan two-dimension code to set this video\n");
    printf("or you can use off-line mode\n");
    printf("\n");
    printf("\n");
    return;
}
static int sql_connect(char *opt)
{
    if(!opt)
        return -1;
    V_global.conn = mysql_init(NULL);
    char value = 1;
    mysql_options(V_global.conn, MYSQL_OPT_RECONNECT, (char *)&value);
    //连接数据库
    if(! mysql_real_connect(V_global.conn,opt,SQLUSER,SQLPASSWD,DATABASE, 0, NULL, 0))
    {
        LOG("connect mysql error!\n")
        return -1;
    }
    MYSQL_RES *sqlres;
    char sqlcmd[32] = {0};
    sprintf(sqlcmd,"select name from videouser where videoid=%s",V_global.VideoId);
    mysql_query(V_global.conn,sqlcmd);
    sqlres = mysql_store_result(V_global.conn);
    if(!sqlres){
        LOG("select table error!\n")
        return -1;
    }
    MYSQL_ROW sqlrow;
    sqlrow = mysql_fetch_row(sqlres);
    if(!sqlrow){
        LOG("this video has not setted\n")
   //     help(NULL);
        exit(1);
    }
    char *username = (char *)sqlrow[0];
    if(username == NULL){
        LOG("unknown error!\n")
        return -1;
    }
    LOG("welcome to use video:%s,%s\n",V_global.VideoId,username);
    return 0;
}

void sigint_handle(int sig)
{

    pthread_cancel(V_global.getvideo);

    pthread_cancel(V_global.sendvideo);

    pthread_cancel(V_global.dealvideo);
    pthread_mutex_destroy(&V_global.mutex);
    pthread_cond_destroy(&V_global.cond);
    if(V_global.TcpFd!=-1)
        close(V_global.TcpFd);
    if(V_global.UdpFd!=-1)
        close(V_global.UdpFd);
    exit(0);
}


int main(int argc,char **argv)
{
    int reset_flag = 0;
    int sql_ret = -1;
    int http_ret = -1;
    int ret = -1;
    memset(V_global.VideoId,0,sizeof(V_global.VideoId));
    strcpy(V_global.VideoId,"99");
    V_global.TcpFd = -1;
    V_global.UdpFd = -1;
    V_global.videoReq = NOREQUEST;
    memset(V_global.VideoBuf,0,PICBUFSIZE);
    memset(V_global.group,0,sizeof(V_global.group));
    strcpy(V_global.group,"vedio99");

    char add_pp[32] = {0};

    signal(SIGINT,sigint_handle);

    while(1) {
        int option_index = 0, c = 0;
        static struct option long_options[] = {
            {"h", no_argument, 0, 0},
            {"help", no_argument, 0, 0},
            {"v", required_argument, 0, 0},
            {"video", required_argument, 0, 0},
            {"s", required_argument, 0, 0},
            {"sql", required_argument, 0, 0},
            {"t", required_argument, 0, 0},
            {"http", required_argument, 0, 0},
            {"r", no_argument, 0, 0},
            {"reset", no_argument, 0, 0},
            {"l",no_argument,0,0},
            {"local",no_argument,0,0},
            {"p", required_argument, 0, 0},
            {"people", required_argument, 0, 0},

            {0, 0, 0, 0}
        };

        c = getopt_long_only(argc, argv, "", long_options, &option_index);

        /* no more options to parse */
        if(c == -1) break;

        /* unrecognized option */
        if(c == '?') {
            help(argv[0]);
            return 0;
        }

        switch(option_index) {
            /* h, help */
        case 0:
        case 1:
            help(argv[0]);
            break;

            /* v, video */
        case 2:
        case 3:
        //    video_init(optarg);
            break;
            /* s,sql */
        case 4:
        case 5:
            sql_ret = sql_connect(optarg);
    //        printf("sql_ret : %d\n",sql_ret);
            break;
            /* t,http */
        case 6:
        case 7:
            http_ret = http_connect(optarg);
     //       printf("http_ret : %d\n",http_ret);
            break;
            /* r,reset */
        case 8:
        case 9:
            reset_flag = 1;
            break;

            /* -l.-local */
        case 10:
        case 11:
            V_global.mode = OFFLINE;
            break;
            /* p,people */
        case 12:
        case 13:
            strcpy(add_pp,optarg);
            break;
        }
    }



    if(V_global.mode == OFFLINE){
        if(V_global.TcpFd != -1)
            close(V_global.TcpFd);
        if(V_global.UdpFd != -1)
            close(V_global.UdpFd);
        off_line_process();
        return 0;
    }
    if(sql_ret < 0 || http_ret < 0){
        LOG("connect to sql or http-service error!\n")
        return 1;
    }
    if(reset_flag == 1){
        ret = video_reset();
        if(ret == -1){
            LOG("reset error!\n")
        }
        else
            LOG("reset video success!\n")
        if(V_global.TcpFd != -1)
            close(V_global.TcpFd);
        if(V_global.UdpFd != -1)
            close(V_global.UdpFd);
        return 0;
    }
    if(add_pp[0]!='\0'){
        printf("add people\n");
        ret = add_people(add_pp);
        if(ret != 0){
            LOG("add people error!\n")
            return -1;
        }
        else
            LOG("add people : %s\n",add_pp);
    }
//    return 0;

    pthread_mutex_init(&V_global.mutex,NULL);
    pthread_cond_init(&V_global.cond,NULL);
//    ret = pthread_create(&V_global.getvideo,NULL,getvideo_thread,NULL);
//    if(ret != 0){
//        LOG("create getvideo error!\n")
//        exit(1);
//    }
//    pthread_detach(V_global.getvideo);
//    ret = pthread_create(&V_global.sendvideo,NULL,sendvideo_thread,NULL);
//    if(ret != 0){
//        LOG("create sendvideo error!\n")
//        exit(1);
//    }
//    pthread_detach(V_global.sendvideo);
    ret = pthread_create(&V_global.dealvideo,NULL,dealvideo_thread,NULL);
    if(ret != 0){
        LOG("create dealvideo error!\n")
        exit(1);
    }
    pthread_detach(V_global.dealvideo);

    struct sockaddr_in service;
    socklen_t socklength;
    memset(&service,0,sizeof(service));

    int maxFd = -1;
    maxFd = (V_global.TcpFd>V_global.UdpFd?V_global.TcpFd:V_global.UdpFd) + 1;
    fd_set rdset;
    struct timeval timeout = {5,0};
    char rdbuf[BUFSIZE] = {0};
    while(1)
    {
        memset(rdbuf,0,BUFSIZE);
        FD_ZERO(&rdset);
        FD_SET(V_global.TcpFd,&rdset);
        FD_SET(V_global.UdpFd,&rdset);
//        if(V_global.ConnectFd > 0){
//            maxFd = V_global.ConnectFd;
//            FD_SET(V_global.ConnectFd,&rdset);
//        }

        select(maxFd,&rdset,NULL,NULL,&timeout);
        if(FD_ISSET(V_global.TcpFd,&rdset))
        {

        }
        if(FD_ISSET(V_global.UdpFd,&rdset))
        {
            if(recvfrom(V_global.UdpFd,rdbuf,BUFSIZE,0,\
                        (struct sockaddr *)&service,&socklength) > 0)
                data_deal_handle(rdbuf,(struct sockaddr *)&service,socklength);
        }

    }

    pthread_cancel(V_global.getvideo);

    pthread_cancel(V_global.sendvideo);

    pthread_cancel(V_global.dealvideo);
    pthread_mutex_destroy(&V_global.mutex);
    pthread_cond_destroy(&V_global.cond);
    if(V_global.TcpFd!=-1)
        close(V_global.TcpFd);
    if(V_global.UdpFd!=-1)
        close(V_global.UdpFd);

    return 0;
}


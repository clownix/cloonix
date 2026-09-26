/* web_vnc.c — VNC WebSocket bridge and generic Unix-socket proxy.
 * Part of the GUI server TU; see web_common.h. */

#include "web_common.h"

static void ws_vnc_bridge(int browser_fd, int vnc_fd,
                             const char *leftover, int leftover_len)
{
    char buf[BUF_SIZE];
    fd_set rd;
    int maxfd=(browser_fd>vnc_fd?browser_fd:vnc_fd)+1;
    long b2s=0,s2b=0;

    /* Process any bytes already read with the HTTP headers */
    if(leftover_len>0){
        const uint8_t *p=(const uint8_t*)leftover;
        int rem=leftover_len;
        while(rem>=2){
            uint8_t b0=p[0],b1=p[1];
            int masked=(b1&0x80)!=0;
            uint64_t plen=b1&0x7f;
            int hlen=2;
            if(plen==126&&rem>=4){plen=((uint64_t)p[2]<<8)|p[3];hlen=4;}
            else if(plen==127&&rem>=10){plen=0;for(int i=0;i<8;i++)plen=(plen<<8)|p[2+i];hlen=10;}
            else if(plen>125)break;
            if(masked)hlen+=4;
            if(rem<hlen+(int)plen)break;
            if((b0&0x0f)==8)goto done;
            const uint8_t *mask=masked?p+hlen-4:NULL;
            const uint8_t *payload=p+hlen;
            for(uint64_t i=0;i<plen;i++){
                char c=payload[i]^(masked?mask[i%4]:0);
                if(write(vnc_fd,&c,1)!=1){goto done;}b2s++;}
            p+=hlen+plen;rem-=hlen+(int)plen;
        }
    }

    while(1){
        FD_ZERO(&rd);FD_SET(browser_fd,&rd);FD_SET(vnc_fd,&rd);
        int rc=select(maxfd,&rd,NULL,NULL,NULL);
        if(rc<0){if(errno==EINTR)continue;break;}
        if(rc==0)break;

        if(FD_ISSET(browser_fd,&rd)){
            uint8_t hdr[14];
            if(read(browser_fd,hdr,2)!=2)break;
            int masked=(hdr[1]&0x80)!=0;
            uint64_t plen=hdr[1]&0x7f;
            if(plen==126){if(read(browser_fd,hdr+2,2)!=2)break;plen=((uint64_t)hdr[2]<<8)|hdr[3];}
            else if(plen==127){if(read(browser_fd,hdr+2,8)!=8)break;plen=0;for(int i=0;i<8;i++)plen=(plen<<8)|hdr[2+i];}
            uint8_t mask[4]={0};
            if(masked){if(read(browser_fd,mask,4)!=4)break;}
            if((hdr[0]&0x0f)==8)break;
            if((hdr[0]&0x0f)==9){      /* ping → pong */
                uint8_t ping_buf[125]={0};
                ssize_t n=plen>0?read(browser_fd,ping_buf,plen<125?plen:125):0;
                if(n>0&&masked)for(ssize_t i=0;i<n;i++)ping_buf[i]^=mask[i%4];
                uint8_t pong[127];pong[0]=0x8A;pong[1]=(uint8_t)(n>0?n:0);
                if(n>0)memcpy(pong+2,ping_buf,n);
                (void)write(browser_fd,pong,2+(n>0?n:0));continue;}
            uint64_t remaining=plen;uint64_t frame_offset=0;
            while(remaining>0){
                size_t chunk=remaining<(size_t)BUF_SIZE?remaining:BUF_SIZE;
                ssize_t n=read(browser_fd,buf,chunk);
                if(n<=0)goto done;
                if(masked)for(ssize_t i=0;i<n;i++)buf[i]^=mask[(frame_offset+i)%4];
                if(write(vnc_fd,buf,n)!=n)goto done;
                b2s+=n;remaining-=n;frame_offset+=n;}
        }

        if(FD_ISSET(vnc_fd,&rd)){
            ssize_t n=read(vnc_fd,buf+10,BUF_SIZE-10);
            if(n<=0)break;
            uint8_t *f=(uint8_t*)buf;int hlen;
            f[0]=0x82; /* FIN + binary */
            if(n<126){f[1]=(uint8_t)n;hlen=2;}
            else if(n<65536){f[1]=126;f[2]=(n>>8)&0xff;f[3]=n&0xff;hlen=4;}
            else{f[1]=127;for(int i=0;i<8;i++)f[2+i]=(n>>((7-i)*8))&0xff;hlen=10;}
            memmove(buf+hlen,buf+10,n);
            if(write(browser_fd,buf,hlen+n)!=hlen+n)break;
            s2b+=n;}
    }
done:
    (void)b2s;(void)s2b;
}

static void pump(int a,int b);

static void proxy_to_vnc_unix(int browser_fd,
                                const char *method,
                                const char *path,
                                const char *proto,
                                const char *full_hdr,
                                int full_hdr_len,
                                const char *leftover,
                                int leftover_len)
{
    const char *after=path+strlen(VNC_PREFIX);
    const char *slash=strchr(after,'/');
    char vm[128]={0};char new_path[512];
    if(slash){size_t nlen=slash-after;if(nlen>=sizeof(vm))goto err;
        memcpy(vm,after,nlen);snprintf(new_path,sizeof(new_path),"%s",slash[0]?slash:"/");}
    else{snprintf(vm,sizeof(vm),"%s",after);strcpy(new_path,"/");}

    char sock_path[512]={0};
    pthread_mutex_lock(&g_vnc_mutex);
    for(int i=0;i<g_nvnc;i++){
        if(strcmp(g_vnc[i].vm,vm)==0){snprintf(sock_path, sizeof(sock_path), "%s", g_vnc[i].sock);break;}}
    pthread_mutex_unlock(&g_vnc_mutex);

    int is_ws=(strstr(full_hdr,"Upgrade: websocket")!=NULL||
               strstr(full_hdr,"Upgrade: WebSocket")!=NULL);

    if(!sock_path[0]||!is_ws){
        char file[1024];snprintf(file,sizeof(file),"%s/cloonix-novnc.html",g_www_dir);
        int f=open(file,O_RDONLY);
        if(f<0){const char *r="HTTP/1.1 404 Not Found\r\nContent-Length:0\r\nConnection: close\r\n\r\n";(void)write(browser_fd,r,strlen(r));return;}
        struct stat st;fstat(f,&st);
        char hdr[256];int hlen=snprintf(hdr,sizeof(hdr),
            "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %lld\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n",(long long)st.st_size);
        (void)write(browser_fd,hdr,hlen);
        char rbuf[BUF_SIZE];ssize_t rn;
        while((rn=read(f,rbuf,sizeof(rbuf)))>0)(void)write(browser_fd,rbuf,rn);
        close(f);return;}

    int sfd=-1;struct sockaddr_un sun={0};
    sun.sun_family=AF_UNIX;snprintf(sun.sun_path, sizeof(sun.sun_path), "%s", sock_path);
    for(int attempt=0;attempt<50;attempt++){
        sfd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);
        if(sfd<0){KERR("WARNING vnc socket: %s",strerror(errno));goto err;}
        if(connect(sfd,(struct sockaddr*)&sun,sizeof(sun))==0)break;
        close(sfd);sfd=-1;usleep(200000);}
    if(sfd<0){
        KERR("WARNING vnc %s: nothing listens after 10 s",sock_path);
        const char *r="HTTP/1.1 502 Bad Gateway\r\nContent-Length:22\r\nConnection: close\r\n\r\nVNC socket not found\n";
        (void)write(browser_fd,r,strlen(r));return;}

    char ws_key[128]={0};
    const char *kp=strcasestr(full_hdr,"Sec-WebSocket-Key:");
    if(kp){kp+=18;while(*kp==' ')kp++;
        int ki=0;while(*kp&&*kp!='\r'&&*kp!='\n'&&ki<127)ws_key[ki++]=*kp++;
        ws_key[ki]=0;while(ki>0&&(ws_key[ki-1]==' '||ws_key[ki-1]=='\t'))ws_key[--ki]=0;}

    const char *magic="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char combined[256];snprintf(combined,sizeof(combined),"%s%s",ws_key,magic);
    uint8_t sha[20];sha1((uint8_t*)combined,strlen(combined),sha);
    char accept[64];base64_enc(sha,20,accept);

    char resp101[512];
    int rlen=snprintf(resp101,sizeof(resp101),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "Sec-WebSocket-Protocol: binary\r\n"
        "\r\n",accept);
    (void)write(browser_fd,resp101,rlen);

    ws_vnc_bridge(browser_fd,sfd,leftover,leftover_len);
    close(sfd);return;

err:{const char *e="HTTP/1.1 500 Internal Server Error\r\nContent-Length:0\r\nConnection: close\r\n\r\n";
     (void)write(browser_fd,e,strlen(e));}
}

static void pump(int a, int b)
{
    char buf[BUF_SIZE];
    fd_set rd;
    int maxfd = (a > b ? a : b) + 1;
    while (1) {
        FD_ZERO(&rd);
        FD_SET(a, &rd);
        FD_SET(b, &rd);
        if (select(maxfd, &rd, NULL, NULL, NULL) <= 0) break;
        if (FD_ISSET(a, &rd)) {
            ssize_t n = read(a, buf, sizeof(buf));
            if (n <= 0) break;
            if (write(b, buf, n) != n) break;
        }
        if (FD_ISSET(b, &rd)) {
            ssize_t n = read(b, buf, sizeof(buf));
            if (n <= 0) break;
            if (write(a, buf, n) != n) break;
        }
    }
}

static void proxy_to_unix(int browser_fd,
                           const char *method,
                           const char *path,
                           const char *proto,
                           const char *full_hdr,
                           int         full_hdr_len)
{
    const char *after = path + strlen(WS_PREFIX);
    const char *slash = strchr(after, '/');
    char sockname[256];
    char new_path[512];

    if (slash) {
        size_t nlen = slash - after;
        if (nlen >= sizeof(sockname)) {
            KERR("ERROR vnc proxy: socket name too long in %s", path);
            goto err;
        }
        memcpy(sockname, after, nlen);
        sockname[nlen] = '\0';
        snprintf(new_path, sizeof(new_path), "%s", slash[0] ? slash : "/");
    } else {
        snprintf(sockname, sizeof(sockname), "%s", after);
        strcpy(new_path, "/");
    }

    if (!web_name_ok(sockname)) {
        KERR("ERROR ws proxy: refused the socket name [%s] in %s", sockname, path);
        goto err;
    }

    char sock_path[512];
    snprintf(sock_path, sizeof(sock_path), "%s/%s", g_snf_dir, sockname);

    /* Connect to webshark Unix socket — retry for up to 10s while webshark
       starts */
    int ufd = -1;
    struct sockaddr_un sun = {0};
    sun.sun_family = AF_UNIX;
    snprintf(sun.sun_path, sizeof(sun.sun_path), "%s", sock_path);

    for (int attempt = 0; attempt < 50; attempt++) {
        ufd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (ufd < 0) {
            KERR("ERROR vnc proxy: socket() failed errno=%d", errno);
            goto err;
        }
        if (connect(ufd, (struct sockaddr *)&sun, sizeof(sun)) == 0)
            break;
        close(ufd); ufd = -1;
        usleep(200000);     /* 200ms between attempts → 10s total */
    }

    if (ufd < 0) {
        char msg[256];
        int mlen = snprintf(msg, sizeof(msg),
            "HTTP/1.1 502 Bad Gateway\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 38\r\nConnection: close\r\n\r\n"
            "webshark not running for this socket.\n");
        (void)write(browser_fd, msg, mlen);
        return;
    }

    char fwd[MAX_HDR + 512];
    int  flen = 0;

    flen += snprintf(fwd + flen, sizeof(fwd) - flen,
                     "%s %s %s\r\n", method, new_path, proto);

    const char *hdr_rest = strstr(full_hdr, "\r\n");
    if (hdr_rest) {
        hdr_rest += 2;
        size_t rlen = full_hdr_len - (hdr_rest - full_hdr);
        if (flen + (int)rlen < (int)sizeof(fwd)) {
            memcpy(fwd + flen, hdr_rest, rlen);
            flen += rlen;
        }
    }

    if (write(ufd, fwd, flen) != flen) {
        KERR("ERROR vnc proxy: cannot forward the request to %s errno=%d",
             sock_path, errno);
        close(ufd);
        return;
    }

    pump(browser_fd, ufd);
    close(ufd);
    return;

err: {
        http_fail(browser_fd, 500, "vnc proxy", "cannot reach the panel");
    }
}

